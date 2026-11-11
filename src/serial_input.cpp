// =============================================================================
// serial_input.cpp — line commands + pasted JSON, straight from the monitor
// =============================================================================
// HOW THE INPUT WORKS (state machine over the raw bytes):
//
//   [typing a line]  -> "h=15 tmax=31" / "fecha=..." / "limpiar" / "ayuda"
//                       or Enter alone = re-run
//   line starts with '{' -> switches to PASTE mode: every following byte
//                       (newlines included) is appended to a buffer while a
//                       brace counter (string-aware, so braces inside
//                       "notes": "..." don't count) watches for the closing
//                       '}' of the top-level object.
//   closing '}' found -> validate with the SAME code as loadFieldReading()
//                       (readingFromJson), and only if everything passes,
//                       write it to /field_data.json in flash.
//
// Failures never touch the existing file, so a bad paste cannot brick the
// prototype's data.
// =============================================================================

#include "serial_input.h"

#include <LittleFS.h>
#include <ArduinoJson.h>

namespace {

// ------------------------------ buffers -------------------------------------
constexpr size_t kLineMax  = 2048;  // one typed or pasted line
constexpr size_t kPasteMax = 6144;  // whole pasted JSON object

char   lineBuf[kLineMax];
size_t lineLen      = 0;
bool   lineOverflow = false;
bool   lastWasCR    = false;   // to swallow the '\n' of a "\r\n" pair

char   pasteBuf[kPasteMax];
size_t pasteLen   = 0;
bool   inPaste    = false;
int    braceDepth = 0;
bool   inString   = false;     // inside a JSON string literal
bool   escaped    = false;     // previous byte was '\'

// ------------------------- quick overrides (RAM) ----------------------------
struct Overrides {
  bool  hasHumidity = false;  float humidity = 0.0f;
  bool  hasTmin     = false;  float tmin     = 0.0f;
  bool  hasTmax     = false;  float tmax     = 0.0f;
  bool  hasTmean    = false;  float tmean    = 0.0f;
  bool  hasDate     = false;  char  date[11] = {};
};

Overrides ov;

void clearOverrides() { ov = Overrides{}; }

// ------------------------------ small utils ---------------------------------
int rank(SerialInput s) {
  if (s == SerialInput::DataSaved) return 2;
  if (s == SerialInput::Rerun)     return 1;
  return 0;
}

// Trims spaces/tabs/CRLF around a mutable C string.
char* trim(char* s) {
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') ++s;
  size_t len = strlen(s);
  while (len > 0) {
    const char c = s[len - 1];
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
    s[--len] = '\0';
  }
  return s;
}

// Strict float parse: the WHOLE token must be a number ("15", "-3.5").
bool parseFloat(const char* s, float& out) {
  if (s == nullptr || *s == '\0') return false;
  char* end = nullptr;
  const float v = strtof(s, &end);
  if (end == s || *end != '\0') return false;
  if (v != v) return false;  // NaN
  out = v;
  return true;
}

bool looksLikeIsoDate(const char* v) {
  if (strlen(v) != 10) return false;
  if (v[4] != '-' || v[7] != '-') return false;
  for (size_t i = 0; i < 10; ++i) {
    if (i == 4 || i == 7) continue;
    if (v[i] < '0' || v[i] > '9') return false;
  }
  return true;
}

void printHelp() {
  Serial.println("[entrada] ==== Comandos del monitor serie ====");
  Serial.println("[entrada]  h=15  |  humedad=15      humedad del suelo [%] (temporal)");
  Serial.println("[entrada]  tmin=14 tmax=27 tmean=21   temperaturas [°C] (temporales)");
  Serial.println("[entrada]  fecha=2026-11-05       fecha -> día del año (temporal)");
  Serial.println("[entrada]  limpiar                borra los valores temporales");
  Serial.println("[entrada]  { ... }                pega un JSON completo: se valida y");
  Serial.println("[entrada]                            se guarda en la flash (permanente)");
  Serial.println("[entrada]  Enter                  repite el informe");
}

// --------------------------- command tokens ---------------------------------
// Recognized-but-rejected values (h=200) are still "understood": they report
// their own error. Returns false only for genuinely unknown tokens.
enum class Token { Unknown, Handled };

Token handleToken(char* tok, bool& applied) {
  if (strcmp(tok, "limpiar") == 0 || strcmp(tok, "borrar") == 0 ||
      strcmp(tok, "clear") == 0) {
    clearOverrides();
    Serial.println("[entrada] valores temporales borrados.");
    applied = true;
    return Token::Handled;
  }

  char* eq = strchr(tok, '=');
  if (eq == nullptr) return Token::Unknown;
  *eq = '\0';
  const char* key = tok;
  const char* val = eq + 1;

  // --- date key (string value) ---
  if (strcmp(key, "fecha") == 0 || strcmp(key, "d") == 0) {
    if (!looksLikeIsoDate(val)) {
      Serial.println("[entrada] ERROR: la fecha debe ser AAAA-MM-DD.");
      return Token::Handled;
    }
    memset(ov.date, 0, sizeof(ov.date));
    memcpy(ov.date, val, 10);
    ov.hasDate = true;
    Serial.printf("[entrada] fecha -> %s (temporal)\n", ov.date);
    applied = true;
    return Token::Handled;
  }

  // --- numeric keys ---
  const bool isHum  = (strcmp(key, "h") == 0 || strcmp(key, "humedad") == 0);
  const bool isTmin = (strcmp(key, "tmin") == 0);
  const bool isTmax = (strcmp(key, "tmax") == 0);
  const bool isTm   = (strcmp(key, "tmean") == 0 || strcmp(key, "tmedia") == 0);
  if (!isHum && !isTmin && !isTmax && !isTm) return Token::Unknown;

  float f = 0.0f;
  if (!parseFloat(val, f)) {
    Serial.printf("[entrada] ERROR: valor numérico no válido en \"%s=%s\".\n",
                  key, val);
    return Token::Handled;
  }

  if (isHum) {
    if (f < 0.0f || f > 100.0f) {
      Serial.println("[entrada] ERROR: la humedad debe estar entre 0 y 100.");
      return Token::Handled;
    }
    ov.hasHumidity = true;
    ov.humidity = f;
    Serial.printf("[entrada] humedad -> %.1f %% (temporal)\n", f);
  } else if (isTmin) {
    ov.hasTmin = true;
    ov.tmin = f;
    Serial.printf("[entrada] tmin -> %.1f °C (temporal)\n", f);
  } else if (isTmax) {
    ov.hasTmax = true;
    ov.tmax = f;
    Serial.printf("[entrada] tmax -> %.1f °C (temporal)\n", f);
  } else {
    ov.hasTmean = true;
    ov.tmean = f;
    Serial.printf("[entrada] tmean -> %.1f °C (temporal)\n", f);
  }
  applied = true;
  return Token::Handled;
}

// ------------------------------ paste flow ----------------------------------
SerialInput finishPaste();

// Appends one byte of the pasted JSON and watches for the closing brace.
SerialInput feedPasteByte(char c) {
  if (pasteLen + 1 >= kPasteMax) {
    inPaste = false;
    pasteLen = 0;
    Serial.println("[entrada] ERROR: el JSON supera el tamaño máximo; se descarta "
                   "y se conserva el archivo anterior.");
    return SerialInput::None;
  }
  pasteBuf[pasteLen++] = c;

  if (escaped) { escaped = false; return SerialInput::None; }
  if (inString) {
    if (c == '\\') escaped = true;
    else if (c == '"') inString = false;
    return SerialInput::None;
  }
  if (c == '"') { inString = true; return SerialInput::None; }
  if (c == '{') { ++braceDepth; return SerialInput::None; }
  if (c == '}') {
    if (braceDepth > 0) --braceDepth;
    if (braceDepth == 0) return finishPaste();
  }
  return SerialInput::None;
}

// Validates the buffered JSON and, only if it is fully valid, replaces
// /field_data.json in flash. Any failure keeps the previous file intact.
SerialInput finishPaste() {
  pasteBuf[pasteLen] = '\0';
  inPaste = false;

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, pasteBuf, pasteLen);
  if (err) {
    Serial.printf("[entrada] ERROR: JSON no válido: %s — se conserva el archivo anterior.\n",
                  err.c_str());
    return SerialInput::None;
  }

  FieldReading check;
  if (!readingFromJson(doc, check, /*quiet=*/true)) {
    Serial.println("[entrada] ERROR: el JSON no pasa la validación — "
                   "se conserva el archivo anterior.");
    return SerialInput::None;
  }

  File file = LittleFS.open("/field_data.json", "w");
  if (!file) {
    Serial.println("[entrada] ERROR: no se pudo abrir /field_data.json para escritura.");
    return SerialInput::None;
  }
  const size_t written =
      file.write(reinterpret_cast<const uint8_t*>(pasteBuf), pasteLen);
  file.close();
  if (written != pasteLen) {
    Serial.println("[entrada] ERROR: escritura incompleta en la flash.");
    return SerialInput::None;
  }

  clearOverrides();
  Serial.printf("[entrada] nuevo dato guardado en la flash (%u bytes); "
                "valores temporales borrados.\n",
                static_cast<unsigned>(written));
  return SerialInput::DataSaved;
}

// ------------------------------ line flow -----------------------------------
SerialInput handleLine(char* raw) {
  char* s = trim(raw);

  if (*s == '\0') return SerialInput::Rerun;   // plain Enter = re-run

  if (s[0] == '{') {                           // begin (or finish) a paste
    inPaste = true;
    pasteLen = 0;
    braceDepth = 0;
    inString = false;
    escaped = false;
    for (char* p = s; *p != '\0'; ++p) feedPasteByte(*p);
    if (!inPaste) return SerialInput::DataSaved;  // single-line JSON, done
    Serial.println("[entrada] pegando JSON… (se guarda al cerrar con '}')");
    return SerialInput::None;
  }

  if (strcmp(s, "ayuda") == 0 || strcmp(s, "help") == 0 || strcmp(s, "?") == 0) {
    printHelp();
    return SerialInput::None;
  }

  bool applied = false;
  bool unknown = false;
  char* tok = s;
  while (*tok != '\0') {
    char* end = tok;
    while (*end != '\0' && *end != ' ' && *end != '\t' && *end != ',' &&
           *end != ';' && *end != '\r' && *end != '\n') {
      ++end;
    }
    const char saved = *end;
    *end = '\0';
    if (*tok != '\0') {
      if (handleToken(tok, applied) == Token::Unknown) {
        unknown = true;
        Serial.printf("[entrada] comando no reconocido: \"%s\" (escribe 'ayuda')\n",
                      tok);
      }
    }
    *end = saved;
    tok = (*end != '\0') ? end + 1 : end;
  }

  if (unknown && !applied) return SerialInput::None;
  if (applied) return SerialInput::Rerun;
  return SerialInput::None;   // unknown only: nothing to re-run
}

}  // namespace

SerialInput readSerialInput() {
  SerialInput best = SerialInput::None;

  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());

    if (inPaste) {
      const SerialInput r = feedPasteByte(c);
      if (rank(r) > rank(best)) best = r;
      continue;
    }

    if (c == '\n' || c == '\r') {
      // Swallow the '\n' that follows a '\r' so CRLF produces ONE line.
      if (c == '\n' && lastWasCR && lineLen == 0) {
        lastWasCR = false;
        continue;
      }
      lastWasCR = (c == '\r');

      if (lineOverflow) {
        Serial.println("[entrada] ERROR: línea demasiado larga; se ignora.");
        lineOverflow = false;
        lineLen = 0;
        continue;
      }

      lineBuf[lineLen] = '\0';
      const SerialInput r = handleLine(lineBuf);
      lineLen = 0;
      if (rank(r) > rank(best)) best = r;
      continue;
    }

    lastWasCR = false;
    if (lineLen + 1 < kLineMax) {
      lineBuf[lineLen++] = c;
    } else {
      lineOverflow = true;
    }
  }

  return best;
}

void applySerialOverrides(FieldReading& r) {
  bool any = false;

  if (ov.hasHumidity) {
    r.groundHumidityPct = ov.humidity;
    any = true;
  }

  if (ov.hasTmin || ov.hasTmax || ov.hasTmean) {
    const float origTmin  = r.tminC;
    const float origTmax  = r.tmaxC;
    const float origTmean = r.tmeanC;
    const bool  origFromFile = r.tmeanFromFile;

    if (ov.hasTmin)  r.tminC = ov.tmin;
    if (ov.hasTmax)  r.tmaxC = ov.tmax;
    if (ov.hasTmean) {
      r.tmeanC = ov.tmean;
      r.tmeanFromFile = true;
    }

    if (r.tmaxC < r.tminC) {
      // √(tmax − tmin) in FAO eq. 52 would be NaN — keep the file's values.
      Serial.println("[entrada] ERROR: tmax no puede ser menor que tmin; "
                     "temperaturas temporales ignoradas.");
      r.tminC = origTmin;
      r.tmaxC = origTmax;
      r.tmeanC = origTmean;
      r.tmeanFromFile = origFromFile;
    } else {
      any = true;
    }
  }

  if (ov.hasDate) {
    if (setReadingDate(r, ov.date)) {
      any = true;
    } else {
      Serial.println("[entrada] ERROR: fecha temporal inválida; se ignora.");
    }
  }

  if (any) {
    Serial.println("[entrada] usando valores temporales ('limpiar' para quitarlos)");
  }
}

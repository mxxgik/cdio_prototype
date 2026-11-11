// =============================================================================
// field_data.cpp — parse the simulated sensor file
// =============================================================================
// WHY a file and not sensors? The project brief says proof-of-concept with no
// hardware attached. Keeping the readings behind a small "loader" function
// means the calculation and alert code never knows where the numbers came
// from: later, replace the body of loadFieldReading() with two analogRead()
// calls and nothing else changes.
//
// The file itself lives in data/field_data.json. PlatformIO's `uploadfs`
// target copies that folder into the ESP32 flash, where LittleFS serves it.
// =============================================================================

#include "field_data.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

namespace {

// "2026-11-02" -> year/month/day, with basic sanity checks.
bool parseIsoDate(const char* iso, int& year, int& month, int& day) {
  if (iso == nullptr || strlen(iso) != 10) return false;
  if (iso[4] != '-' || iso[7] != '-') return false;
  if (sscanf(iso, "%4d-%2d-%2d", &year, &month, &day) != 3) return false;
  if (month < 1 || month > 12 || day < 1 || day > 31) return false;
  return true;
}

// Day of year (1..366) with leap-year handling. The day of year J drives the
// sun's position in FAO eq. 23-24, so the date matters for the calculations.
int computeDayOfYear(int year, int month, int day) {
  static const int kCumulativeDays[12] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
  };
  const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
  int doy = kCumulativeDays[month - 1] + day;
  if (leap && month > 2) ++doy;
  return doy;
}

}  // namespace

bool readingFromJson(JsonDocument& doc, FieldReading& out, bool quiet) {
  memset(&out, 0, sizeof(out));

  // --- required: date -------------------------------------------------------
  const char* dateStr = doc["date"] | "";
  int month = 0, day = 0;
  if (!parseIsoDate(dateStr, out.year, month, day)) {
    Serial.printf("[data] ERROR: \"date\" incorrecta o ausente (debe ser AAAA-MM-DD): \"%s\"\n",
                  dateStr);
    return false;
  }
  snprintf(out.isoDate, sizeof(out.isoDate), "%s", dateStr);
  out.dayOfYear = computeDayOfYear(out.year, month, day);

  // --- required: temperatures ----------------------------------------------
  if (!doc["tmin_c"].is<float>() || !doc["tmax_c"].is<float>()) {
    Serial.println("[data] ERROR: faltan \"tmin_c\" y \"tmax_c\" (obligatorios).");
    return false;
  }
  out.tminC = doc["tmin_c"].as<float>();
  out.tmaxC = doc["tmax_c"].as<float>();
  if (out.tmaxC < out.tminC) {
    Serial.println("[data] ERROR: tmax_c es menor que tmin_c.");
    return false;
  }

  // --- optional: mean temperature (FAO eq. 9 defines it as (tmax+tmin)/2) ---
  if (doc["tmean_c"].is<float>()) {
    out.tmeanC = doc["tmean_c"].as<float>();
    out.tmeanFromFile = true;
  } else {
    out.tmeanC = 0.5f * (out.tmaxC + out.tminC);
    out.tmeanFromFile = false;
  }

  // --- required: ground humidity -------------------------------------------
  if (!doc["ground_humidity_pct"].is<float>()) {
    Serial.println("[data] ERROR: falta \"ground_humidity_pct\" (obligatorio).");
    return false;
  }
  out.groundHumidityPct = doc["ground_humidity_pct"].as<float>();
  if (out.groundHumidityPct < 0.0f || out.groundHumidityPct > 100.0f) {
    Serial.println("[data] ERROR: ground_humidity_pct debe estar entre 0 y 100 (%).");
    return false;
  }

  // --- optional: field id ---------------------------------------------------
  const char* id = doc["field_id"] | "field";
  snprintf(out.fieldId, sizeof(out.fieldId), "%s", id);

  if (!quiet) {
    Serial.printf("[data] ok: %s, doy=%d, T=%.1f/%.1f/%.1f °C, suelo=%.1f%%\n",
                  out.fieldId, out.dayOfYear, out.tminC, out.tmaxC, out.tmeanC,
                  out.groundHumidityPct);
  }
  return true;
}

bool setReadingDate(FieldReading& out, const char* iso) {
  int year = 0, month = 0, day = 0;
  if (!parseIsoDate(iso, year, month, day)) return false;
  snprintf(out.isoDate, sizeof(out.isoDate), "%s", iso);
  out.year = year;
  out.dayOfYear = computeDayOfYear(year, month, day);
  return true;
}

bool loadFieldReading(FieldReading& out) {
  File file = LittleFS.open("/field_data.json", "r");
  if (!file) {
    Serial.println("[data] ERROR: no se encontró /field_data.json en la flash.");
    Serial.println("[data]        Solución: ejecuta `pio run -t uploadfs` una vez.");
    return false;
  }

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    Serial.printf("[data] ERROR: JSON no válido: %s\n", err.c_str());
    return false;
  }

  return readingFromJson(doc, out);
}

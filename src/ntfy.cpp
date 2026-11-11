// =============================================================================
// ntfy.cpp — send the report to the farmer's phone via ntfy.sh
// =============================================================================
// The whole delivery contract with ntfy:
//   POST https://ntfy.sh/<topic>
//   Headers:  Title: <notification headline>
//             Content-Type: text/plain; charset=utf-8
//   Body:     the report, exactly as printed on the serial monitor
// Answer on success: HTTP 200 with a small JSON body like {"id":"bXki..."}.
//
// Unlike CallMeBot there is no phone number and no API key to encode into the
// URL — the topic path segment is the only credential, and the message travels
// as a plain UTF-8 body (so no percent-encoding of °, ² and newlines needed).
// =============================================================================

#include "ntfy.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include "config.h"

bool connectWiFi(unsigned long timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.printf("[wifi] conectando a \"%s\"", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[wifi] ERROR: no se pudo conectar (revisa config.h).");
    return false;
  }
  Serial.printf("[wifi] conectado, IP %s\n", WiFi.localIP().toString().c_str());
  return true;
}

bool sendNtfyMessage(const String& title, const String& text) {
#if !ENABLE_NTFY
  (void)title;
  (void)text;
  Serial.println("[ntfy] deshabilitado (ENABLE_NTFY = 0 en config.h); "
                 "el mensaje solo se imprime arriba.");
  return false;
#else
  // Friendly guard: refuse to shout into the void while config.h still holds
  // the placeholder topic.
  if (strcmp(NTFY_TOPIC, "REPLACE_WITH_YOUR_SECRET_TOPIC") == 0) {
    Serial.println("[ntfy] ERROR: NTFY_TOPIC en config.h sigue siendo el de "
                   "ejemplo. Elige un nombre de tema largo y aleatorio, "
                   "suscríbete a él (app ntfy o navegador), recompila y sube "
                   "el programa.");
    return false;
  }

  if (!connectWiFi()) return false;

  // Topic names must be URL-safe (a-zA-Z0-9_-); we never encode them because
  // an invalid character would silently create a *different* topic.
  const String url = String(NTFY_SERVER) + "/" + NTFY_TOPIC;

  WiFiClientSecure client;
  // Skip TLS certificate validation. Acceptable for a proof of concept
  // (the ESP32 CA bundle is not pinned here); for production, pin ntfy's
  // certificate instead of calling setInsecure().
  client.setInsecure();

  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(20000);

  if (!http.begin(client, url)) {
    Serial.println("[ntfy] ERROR: no se pudo iniciar la petición HTTP.");
    return false;
  }

  http.addHeader("Title", title);
  http.addHeader("Content-Type", "text/plain; charset=utf-8");
  http.addHeader("Priority", "default");

  const int code = http.POST(text);   // ntfy takes the raw text as body
  const String body = http.getString();
  http.end();

  Serial.printf("[ntfy] HTTP %d: %s\n", code, body.c_str());
  if (code == 200) {
    Serial.println("[ntfy] mensaje enviado");
    return true;
  }
  Serial.println("[ntfy] fallo al enviar. Causas típicas: nadie suscrito al "
                 "tema en este dispositivo, error en el nombre del tema o "
                 "falta de Wi-Fi.");
  return false;
#endif
}

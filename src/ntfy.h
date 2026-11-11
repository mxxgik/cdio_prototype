#pragma once
// =============================================================================
// ntfy.h — delivery layer (ntfy.sh push gateway)
// =============================================================================
// ntfy is a free, open-source push service. One HTTPS POST to
//   https://ntfy.sh/<your-topic>     (request body = the message text)
// delivers a push notification to every device subscribed to that topic.
//
// No account, no API key, no phone number: the topic name IS the secret,
// so pick a long random one (see include/config.h for how to subscribe).
//
// Nothing else in the project knows ntfy exists: if the gateway ever changes
// (self-hosted ntfy, Discord webhook, MQTT, an official API ...), only this
// file changes.
// =============================================================================

#include <Arduino.h>

// Connects to Wi-Fi (blocking, prints progress). Returns true when connected.
bool connectWiFi(unsigned long timeoutMs = 20000);

// POSTs the message to NTFY_TOPIC with `title` as the notification title.
// Returns true on HTTP 200.
// Respects ENABLE_NTFY from config.h (when 0, only logs and returns false).
bool sendNtfyMessage(const String& title, const String& text);

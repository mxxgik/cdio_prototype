#pragma once
// =============================================================================
// config.h — THE ONE FILE YOU EDIT
// =============================================================================
// This header holds everything that depends on *your* situation rather than on
// the program logic:
//
//   1. Wi-Fi credentials          -> so the ESP32 can reach the internet
//   2. Push destination           -> ntfy topic name (the only secret)
//   3. Field identity             -> name, location, size, grass type, soil
//
// Everything else (parsing, physics, message composition) lives in src/.
// The FAO values below are taken from Irrigation & Drainage Paper 56:
//   Table 12  -> crop coefficients Kc
//   Table 19  -> soil water characteristics (field capacity / wilting point)
//   Table 22  -> rooting depth Zr and depletion fraction p
// =============================================================================

// ------------------------------- 1. Wi-Fi -----------------------------------
#define WIFI_SSID       "CHAPARRO NOSSA"
#define WIFI_PASSWORD   "Adriana2006"

// ------------------------ 2. Push delivery (ntfy.sh) -------------------------
// ntfy is a free, open-source push service: no account, no API key and no
// phone number anywhere. You invent a long random "topic" name; that topic
// name IS the credential — anyone who knows it can post to it and receive
// from it, so make it unguessable (mix letters, digits, a random suffix).
//
// Setup, once:
//   a) On your phone install the free "ntfy" app (Play Store / F-Droid /
//      iOS App Store) — or simply open https://ntfy.sh/<topic> in a browser,
//      no app needed.
//   b) Make up a topic, e.g. cdio-field-a7f3k9q2x4z, and SUBSCRIBE TO IT
//      FIRST: ntfy delivers live, it is not an inbox — anything sent while
//      nobody is listening is gone.
//   c) Put EXACTLY the same topic below (a-zA-Z0-9_- only), rebuild, upload.
//
// Test from your computer before touching the ESP32:
//   curl -d "hello from the field" https://ntfy.sh/YOUR_TOPIC
//
// Set ENABLE_NTFY to 0 to print the report only on the serial monitor
// (useful for the very first test, before setting anything up).
#define ENABLE_NTFY 1
#define NTFY_SERVER "https://ntfy.sh"
#define NTFY_TOPIC  "cdio-field-2stm8os4kd"

// ------------------------------- 3. Field -----------------------------------
#define FIELD_NAME    "Potrero norte"
#define FIELD_AREA_M2 1000.0f  // used to convert L/m2 into total liters

// Latitude in decimal degrees (positive = northern hemisphere).
// Needed because the sun's energy input (Ra) depends on it — FAO eq. 21-22.
#define FIELD_LAT_DEG 40.4168f

// --- Grass type: pick ONE ----------------------------------------------------
#define GRASS_COOL_SEASON 0  // bluegrass, ryegrass, fescue
#define GRASS_WARM_SEASON 1  // bermuda, St. Augustine
#define GRASS_TYPE        GRASS_COOL_SEASON

#if GRASS_TYPE == GRASS_COOL_SEASON
  #define GRASS_LABEL  "pasto de clima frío"
  #define KC           0.95f  // FAO 56 Table 12: turf grass, cool season, Kc mid
  #define P_DEPLETION  0.40f  // FAO 56 Table 22: turf grass, cool season, p
#else
  #define GRASS_LABEL  "pasto de clima cálido"
  #define KC           0.85f  // FAO 56 Table 12: turf grass, warm season, Kc mid
  #define P_DEPLETION  0.50f  // FAO 56 Table 22: turf grass, warm season, p
#endif

// --- Soil: pick ONE (values are midpoints of the ranges in FAO 56 Table 19) --
#define SOIL_LABEL    "franco"
#define SOIL_THETA_FC 0.25f  // field capacity  [m3/m3]  (loam range 0.20-0.30)
#define SOIL_THETA_WP 0.12f  // wilting point   [m3/m3]  (loam range 0.07-0.17)

// Other soil presets (uncomment one, comment the one above):
// #define SOIL_LABEL    "franco arenoso"
// #define SOIL_THETA_FC 0.23f  // range 0.18-0.28
// #define SOIL_THETA_WP 0.11f  // range 0.06-0.16
//
// #define SOIL_LABEL    "limo"
// #define SOIL_THETA_FC 0.32f  // range 0.28-0.36
// #define SOIL_THETA_WP 0.17f  // range 0.12-0.22
//
// #define SOIL_LABEL    "arcilla"
// #define SOIL_THETA_FC 0.36f  // range 0.32-0.40
// #define SOIL_THETA_WP 0.22f  // range 0.20-0.24

// Rooting depth of the grass [m]. FAO 56 Table 22 lists 0.5-1.0 m for
// turf grass; deeper roots = bigger water reservoir = longer time to stress.
#define ROOT_DEPTH_M 0.70f

// The report switches to a "plan irrigation" warning when the field reaches
// the stress threshold within this many hours.
#define WARN_WITHIN_HOURS 24.0f

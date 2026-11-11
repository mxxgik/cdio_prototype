#pragma once
// =============================================================================
// field_data.h — the "sensor" layer
// =============================================================================
// In the final product this struct would be filled by real sensors (a soil
// moisture probe + a temperature sensor). In this prototype the same values
// are read from the file data/field_data.json that lives on the ESP32 flash,
// so nothing else in the program has to change when real sensors arrive.
// =============================================================================

#include <Arduino.h>
#include <ArduinoJson.h>

struct FieldReading {
  char  fieldId[48];        // free label from the file, e.g. "north_paddock"
  char  isoDate[11];        // "YYYY-MM-DD" as written in the file
  int   year;               // parsed from isoDate
  int   dayOfYear;          // 1..366 — required to compute solar radiation
  float tminC;              // daily minimum air temperature  [°C]
  float tmaxC;              // daily maximum air temperature  [°C]
  float tmeanC;             // daily mean air temperature     [°C]
  bool  tmeanFromFile;      // false => derived as (tmax+tmin)/2, FAO eq. 9
  float groundHumidityPct;  // volumetric soil water content  [%]  (21.0 = 0.21 m3/m3)
};

// Reads and validates /field_data.json from LittleFS (the ESP32's flash
// filesystem). Returns false and prints a diagnostic when anything is wrong.
// LittleFS must already be mounted by the caller.
bool loadFieldReading(FieldReading& out);

// Fills `out` from an already-deserialized document, performing exactly the
// same validation (and printing the same [data] diagnostics) as
// loadFieldReading(). Exists so the serial "paste JSON" input can check new
// data BEFORE it overwrites the file — a rejected paste leaves the old,
// working file untouched.
// `quiet` suppresses the success line only; errors are always printed.
bool readingFromJson(JsonDocument& doc, FieldReading& out, bool quiet = false);

// Overwrites isoDate / year / dayOfYear from a "YYYY-MM-DD" string (used by
// the serial quick-override `fecha=`). Returns false — leaving `out`
// unchanged — when the string is not a valid date.
bool setReadingDate(FieldReading& out, const char* iso);

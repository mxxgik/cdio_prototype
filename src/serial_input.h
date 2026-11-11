#pragma once
// =============================================================================
// serial_input.h — live data input through the serial monitor
// =============================================================================
// Two ways to feed new readings to the program WITHOUT re-uploading the
// filesystem image:
//
//   1. QUICK OVERRIDES — one or more key=value pairs per line:
//        h=15                      soil humidity [%]      (RAM only)
//        tmin=14 tmax=27 tmean=21  temperatures [°C]      (RAM only)
//        fecha=2026-11-05          date -> day of year    (RAM only)
//        limpiar                   drop all overrides
//      They are applied on top of the file's values for as long as the board
//      stays powered (gone after reboot — deliberately: the file keeps its
//      last saved state).
//
//   2. PASTE — a complete JSON object (one or many lines):
//        { "field_id": "...", "date": "...", ... }
//      The paste is fully validated FIRST; only then is it written to
//      /field_data.json in the ESP32 flash, so it becomes the permanent new
//      data and survives reboots (and replaces the need for uploadfs).
//
// Enter alone re-runs the report; "ayuda" prints this summary.
// =============================================================================

#include <Arduino.h>

#include "field_data.h"

// What the consumed bytes imply for the caller:
//   None      -> keep looping (nothing to re-run yet, or the input was bad)
//   Rerun     -> regenerate + resend the report (Enter, or an override)
//   DataSaved -> new JSON was stored in flash; regenerate + resend
enum class SerialInput { None, Rerun, DataSaved };

// Consumes every byte waiting in the serial buffer. Non-blocking: call it
// from loop() whenever Serial.available() > 0.
SerialInput readSerialInput();

// Applies the active quick overrides to a freshly loaded reading. Must be
// called right after loadFieldReading() and before any calculation.
void applySerialOverrides(FieldReading& reading);

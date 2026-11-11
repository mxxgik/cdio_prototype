// =============================================================================
// main.cpp — the whole pipeline, start to finish
// =============================================================================
//   1. mount LittleFS  (the flash filesystem holding the data file)
//   2. read + validate data/field_data.json          -> field_data.cpp
//   3. compute how fast water evaporates (ETo)       -> fao_et.cpp   (eq. 21, 52)
//   4. compute the soil water balance + forecasts    -> soil_water.cpp (Ch. 8)
//   5. compose the message                           -> alert.cpp
//   6. print it on serial + push it to the phone         -> ntfy.cpp
//
// The prototype runs the report once at boot. From then on the serial monitor
// is the live input channel (see serial_input.h):
//   Enter          -> re-run the report
//   h=15 tmax=31   -> temporary overrides (RAM), applied for every re-run
//   { ... }        -> pasted JSON: validated, stored in flash, becomes the
//                     permanent data (no `uploadfs` needed anymore)
//   ayuda          -> the full command list
// =============================================================================

#include <Arduino.h>
#include <LittleFS.h>

#include "alert.h"
#include "config.h"
#include "fao_et.h"
#include "field_data.h"
#include "soil_water.h"
#include "ntfy.h"
#include "serial_input.h"

namespace {

// Shown after every report so the demo operator always knows what to type.
const char kHint[] =
    "Pulsa Enter para repetir el informe; escribe 'ayuda' para ver los "
    "comandos.";

void runReport() {
  // --- 2. readings ----------------------------------------------------------
  FieldReading reading;
  if (!loadFieldReading(reading)) return;
  applySerialOverrides(reading);   // temporary h= / tmin= / fecha= values

  // --- 3. evaporation power of the atmosphere (temperature-only method) -----
  const float eto = fao::hargreavesETo(reading.dayOfYear, FIELD_LAT_DEG,
                                       reading.tminC, reading.tmaxC,
                                       reading.tmeanC);

  // --- 4. soil water balance (FAO 56 Chapter 8) -----------------------------
  const WaterStatus water = evaluateWaterStatus(reading, eto);

  Serial.printf("[calc] ETo por Ra=%.2f mm/día | TAW=%.0f RAW=%.0f "
                "Dr=%.0f Ks=%.2f ETc=%.2f\n",
                eto, water.tawMm, water.rawMm, water.depletionMm, water.ks,
                water.etcAdjMmDay);

  // --- 5. message -----------------------------------------------------------
  const String message = buildAlertMessage(reading, water, eto);

  Serial.println("============= informe del campo =============");
  Serial.println(message);
  Serial.println("========================================");

  // --- 6. delivery ----------------------------------------------------------
  const String title = String("Alerta de campo - ") + reading.fieldId;
  sendNtfyMessage(title, message);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("Prototipo de alerta de campo — arrancando");

  // "true" = format the partition if the filesystem is missing/corrupt.
  if (!LittleFS.begin(true)) {
    Serial.println("[fs] ERROR: no se pudo montar LittleFS — la flash puede "
                   "estar corrupta.");
    return;
  }

  runReport();

  Serial.println(kHint);
}

void loop() {
  if (Serial.available() > 0) {
    const SerialInput action = readSerialInput();
    if (action == SerialInput::Rerun || action == SerialInput::DataSaved) {
      delay(50);   // let the rest of a fast paste arrive, then recompute
      runReport();
      Serial.println(kHint);
    }
  }
  delay(20);
}

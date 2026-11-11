#pragma once
// =============================================================================
// alert.h — compose the push notification the farmer receives
// =============================================================================
// Kept separate from the math on purpose: the calculations produce numbers,
// this module decides what they *mean* (OK / plan irrigation / irrigate now)
// and how to say it. Change the wording here without touching the physics.
// =============================================================================

#include <Arduino.h>

#include "field_data.h"
#include "soil_water.h"

// Returns the complete multi-line message, e.g.:
//   Alerta de campo - North paddock
//   Temperatura: 20.9 °C promedio (14.2 mín / 27.5 máx)
//   ...
String buildAlertMessage(const FieldReading& reading,
                         const WaterStatus& water,
                         float etoMmDay);

// =============================================================================
// soil_water.cpp — FAO 56 Chapter 8 implemented one equation at a time.
// Each block names the equation it implements so it can be checked against
// the FAO document (and against tools/verify_formulas.py on your computer).
// =============================================================================

#include "soil_water.h"

#include <Arduino.h>

#include "config.h"

WaterStatus evaluateWaterStatus(const FieldReading& reading, float etoMmDay) {
  WaterStatus s{};
  s.ks = 1.0f;

  const float thetaFC   = SOIL_THETA_FC;    // field capacity    [m3/m3]
  const float thetaWP   = SOIL_THETA_WP;    // wilting point     [m3/m3]
  const float rootDepth = ROOT_DEPTH_M;     // Zr                [m]
  const float p         = P_DEPLETION;      // depletion fraction (Table 22)

  // --- eq. 82: total available water ---------------------------------------
  s.tawMm = 1000.0f * (thetaFC - thetaWP) * rootDepth;
  if (s.tawMm < 1.0f) s.tawMm = 1.0f;  // guard against absurd configurations

  // --- eq. 83: readily available water (stress threshold) -------------------
  s.rawMm = p * s.tawMm;

  // --- eq. 87: depletion from the *measured* soil moisture ------------------
  const float thetaMeasured = reading.groundHumidityPct / 100.0f;  // % -> m3/m3
  s.depletionMm = 1000.0f * (thetaFC - thetaMeasured) * rootDepth;

  // --- eq. 86: physically impossible values are clamped ---------------------
  if (s.depletionMm < 0.0f)     s.depletionMm = 0.0f;      // above field capacity
  if (s.depletionMm > s.tawMm)  s.depletionMm = s.tawMm;   // below wilting point

  // --- eq. 58: crop evapotranspiration under standard conditions ------------
  s.etcMmDay = KC * etoMmDay;

  // --- eq. 84: water stress coefficient -------------------------------------
  if (s.depletionMm <= s.rawMm) {
    s.ks = 1.0f;  // no stress while there is readily available water left
  } else {
    s.ks = (s.tawMm - s.depletionMm) / ((1.0f - p) * s.tawMm);
    if (s.ks < 0.0f) s.ks = 0.0f;
    if (s.ks > 1.0f) s.ks = 1.0f;
  }
  s.stressed = (s.depletionMm >= s.rawMm);

  // --- eq. 81: crop evapotranspiration under soil water stress --------------
  s.etcAdjMmDay = s.ks * KC * etoMmDay;

  // --- forecast: time until the stress threshold is reached -----------------
  // Chapter 8: "irrigations should be applied before or at the moment when
  // the readily available soil water is depleted". We extrapolate at today's
  // ET rate — a first-order estimate, honest about being an approximation.
  if (s.stressed || s.etcMmDay < 0.05f) {
    s.hoursToStress = 0.0f;
  } else {
    const float remainingMm = s.rawMm - s.depletionMm;
    s.hoursToStress = (remainingMm / s.etcMmDay) * 24.0f;
  }

  // --- irrigation depth: "net irrigation depth ≤ Dr" -------------------------
  // Refilling exactly Dr brings the root zone back to field capacity without
  // wasting water below the root zone (deep percolation). 1 mm over 1 m2 is
  // exactly 1 liter, which makes the unit conversion trivial.
  s.irrigationMm     = s.depletionMm;
  s.irrigationLPerM2 = s.depletionMm;
  s.irrigationTotalL = s.depletionMm * FIELD_AREA_M2;

  return s;
}

#pragma once
// =============================================================================
// soil_water.h — the heart of the alert: FAO 56 Chapter 8
// =============================================================================
// Everything here comes from the chapter you linked
// (https://www.fao.org/4/X0490E/x0490e0e.htm — "ETc under soil water stress"):
//
//   TAW = 1000 (θFC - θWP) Zr        (eq. 82) total water the roots can use
//   RAW = p · TAW                    (eq. 83) fraction usable WITHOUT stress
//   Dr  = 1000 (θFC - θmeasured) Zr  (eq. 87) current depletion, clamped
//                                     to [0, TAW] by eq. 86
//   Ks  = 1 if Dr ≤ RAW, else (TAW - Dr) / ((1-p) TAW)   (eq. 84)
//   ETc adj = Ks · Kc · ETo          (eq. 81) water use under stress
//   I ≤ Dr                           ("forecasting or allocating irrigations")
//
// In plain words: the soil is a tank. TAW is its full size, RAW is the part
// the grass can drink without complaining, Dr is how empty it is right now,
// and Ks tells us how much the grass has already slowed down its drinking.
// =============================================================================

#include "field_data.h"

struct WaterStatus {
  float tawMm;          // total available water in the root zone   [mm]
  float rawMm;          // readily available water (stress starts)   [mm]
  float depletionMm;    // Dr: missing to reach field capacity       [mm]
  float ks;             // water stress coefficient                  [0..1]
  float etcMmDay;       // crop ET without stress = Kc · ETo         [mm/day]
  float etcAdjMmDay;    // crop ET under current stress              [mm/day]
  bool  stressed;       // true when Dr has reached RAW
  float hoursToStress;  // forecast at current ET rate               [h]
  float irrigationMm;           // water to refill to field capacity [mm]
  float irrigationLPerM2;       // same number, since 1 mm = 1 L/m2
  float irrigationTotalL;       // for the whole field                [L]
};

// etoMmDay comes from fao::hargreavesETo(); field parameters (Kc, p, soil,
// root depth) come from include/config.h.
WaterStatus evaluateWaterStatus(const FieldReading& reading, float etoMmDay);

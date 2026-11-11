#pragma once
// =============================================================================
// fao_et.h — reference evapotranspiration (how fast the field loses water)
// =============================================================================
// Formulas from FAO Irrigation & Drainage Paper 56, "Crop evapotranspiration":
//
//   * eq. 21-27 (Chapter 3): extraterrestrial radiation Ra — the sun's energy
//     arriving at the top of the atmosphere, which depends on latitude and
//     day of the year.
//   * eq. 52 (Chapter 3, "alternative equation for ETo when weather data are
//     missing"): the Hargreaves equation — the FAO's own sanctioned method to
//     estimate ETo from *temperature alone*, which is exactly what our
//     prototype measures.
//
// Units: Ra in MJ m-2 day-1, ETo in mm/day (a "depth" of water: 1 mm = the
// amount of water that evaporates from 1 m2 of field).
// =============================================================================

namespace fao {

// Extraterrestrial radiation Ra [MJ m-2 day-1] — FAO eq. 21 (+ eq. 22..27)
float extraterrestrialRadiation(int dayOfYear, float latitudeDeg);

// Reference crop evapotranspiration ETo [mm/day] — Hargreaves, FAO eq. 52.
// ETo = 0.0023 (Tmean + 17.8) (Tmax - Tmin)^0.5 Ra
// (Ra converted from MJ to mm with eq. 20: 1 MJ m-2 day-1 = 0.408 mm/day)
float hargreavesETo(int dayOfYear, float latitudeDeg,
                    float tminC, float tmaxC, float tmeanC);

}  // namespace fao

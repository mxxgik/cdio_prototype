// =============================================================================
// fao_et.cpp — radiation + Hargreaves ETo, straight from FAO 56
// =============================================================================
// Why Hargreaves and not the famous Penman-Monteith equation? Penman-Monteith
// (FAO eq. 6) needs net radiation, air humidity and wind speed on top of
// temperature. Our prototype only measures temperature, and FAO itself says:
//   "As an alternative, ETo can be estimated using the Hargreaves ETo
//    equation" (Chapter 3, eq. 52) — the method designed for missing weather
//    data. It is a sanctioned approximation, not a homemade shortcut.
// =============================================================================

#include "fao_et.h"

#include <math.h>

namespace fao {
namespace {

constexpr float kPi          = 3.14159265358979323846f;
constexpr float kSolarConst  = 0.0820f;  // solar constant [MJ m-2 min-1]
constexpr float kMjToMm      = 0.408f;   // eq. 20: MJ m-2 day-1 -> mm/day

}  // namespace

float extraterrestrialRadiation(int dayOfYear, float latitudeDeg) {
  // Clamp the day of year into a valid range (defensive; the parser already
  // produces valid values).
  if (dayOfYear < 1)  dayOfYear = 1;
  if (dayOfYear > 366) dayOfYear = 366;

  const float j  = static_cast<float>(dayOfYear);
  const float phi = latitudeDeg * kPi / 180.0f;                    // eq. 22
  const float dr  = 1.0f + 0.033f * cosf(2.0f * kPi * j / 365.0f); // eq. 23
  const float dec = 0.409f * sinf(2.0f * kPi * j / 365.0f - 1.39f);// eq. 24

  // Sunset hour angle, eq. 25. acos() only accepts [-1, 1]; floating point
  // noise near the poles can push the argument slightly outside.
  float arg = -tanf(phi) * tanf(dec);
  if (arg > 1.0f)  arg = 1.0f;
  if (arg < -1.0f) arg = -1.0f;
  const float ws = acosf(arg);                                      // eq. 25

  const float term = ws * sinf(phi) * sinf(dec) +
                     cosf(phi) * cosf(dec) * sinf(ws);
  const float ra = (24.0f * 60.0f / kPi) * kSolarConst * dr * term; // eq. 21

  // At extreme latitudes in winter the sun can stay below the horizon for the
  // whole day; physically Ra is then 0, never negative.
  return (ra > 0.0f) ? ra : 0.0f;
}

float hargreavesETo(int dayOfYear, float latitudeDeg,
                    float tminC, float tmaxC, float tmeanC) {
  float diurnalRange = tmaxC - tminC;
  if (diurnalRange < 0.0f) diurnalRange = 0.0f;  // sqrt() must stay real

  const float raMmDay = kMjToMm * extraterrestrialRadiation(dayOfYear, latitudeDeg);

  // FAO eq. 52: ETo = 0.0023 (Tmean + 17.8) (Tmax - Tmin)^0.5 Ra
  // with Ra expressed in mm/day (as the FAO text requires).
  const float eto = 0.0023f * (tmeanC + 17.8f) * sqrtf(diurnalRange) * raMmDay;
  return (eto > 0.0f) ? eto : 0.0f;
}

}  // namespace fao

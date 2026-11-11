// =============================================================================
// alert.cpp — turn numbers into a farmer-readable message
// =============================================================================

#include "alert.h"

#include "config.h"

namespace {

// Format the "time until stress" in whichever unit stays easiest to read.
void formatTimeToStress(float hours, bool stressed, char* out, size_t outSize) {
  if (stressed) {
    snprintf(out, outSize, "YA SUPERÓ el umbral");
  } else if (hours < 48.0f) {
    snprintf(out, outSize, "~%.0f h", hours);
  } else {
    snprintf(out, outSize, "~%.1f días", hours / 24.0f);
  }
}

}  // namespace

String buildAlertMessage(const FieldReading& reading,
                         const WaterStatus& water,
                         float etoMmDay) {
  // --- decide the verdict ----------------------------------------------------
  const char* verdict;
  if (water.stressed) {
    verdict = "ESTRÉS HÍDRICO - riegue ya";
  } else if (water.hoursToStress <= WARN_WITHIN_HOURS) {
    verdict = "se seca rápido - programe el riego pronto";
  } else {
    verdict = "OK - aún no necesita riego";
  }

  char stressText[48];
  formatTimeToStress(water.hoursToStress, water.stressed, stressText,
                     sizeof(stressText));

  // --- compose ---------------------------------------------------------------
  char buffer[1100];
  snprintf(
      buffer, sizeof(buffer),
      "*Alerta de campo* - %s\n"
      "Fecha: %s | %s | suelo: %s\n"
      "\n"
      "*Temperatura:* %.1f °C promedio (%.1f mín / %.1f máx)\n"
      "*Humedad del suelo:* %.1f %% (capacidad de campo %.0f %%)\n"
      "*Depleción en la raíz:* %.0f mm de %.0f mm disponibles\n"
      "\n"
      "*Tiempo hasta el estrés:* %s\n"
      "*Agua para rellenar el campo:* %.0f mm = %.0f L/m² (%.0f L en total)\n"
      "\n"
      "Uso de agua hoy: ETo %.1f · ETc %.1f mm/día · Ks %.2f\n"
      "*Estado:* %s",

      reading.fieldId,
      reading.isoDate, GRASS_LABEL, SOIL_LABEL,

      reading.tmeanC, reading.tminC, reading.tmaxC,

      reading.groundHumidityPct, SOIL_THETA_FC * 100.0f,

      water.depletionMm, water.rawMm,

      stressText,

      water.irrigationMm, water.irrigationLPerM2, water.irrigationTotalL,

      etoMmDay, water.etcAdjMmDay, water.ks,

      verdict);

  return String(buffer);
}

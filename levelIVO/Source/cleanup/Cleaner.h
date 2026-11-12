// levelIVO — LIMPIEZA (motor B). Corre antes de la nivelación: repara clipping, clics de boca y golpes al micro,
// reduce ruido de fondo y sangrado de cascos (con el beat como referencia si lo hay) y prepara la ganancia
// de limpieza (respiraciones, otras voces, huecos) que se aplica junto a la curva de nivel.
#pragma once
#include "../analysis/Analysis.h"

namespace levelivo
{
struct CleanParams
{
    bool noiseOn = true;    float noiseDb = 9.0f;     // reducción máxima entre frases (0..30 dB)
    bool bleedOn = true;    float bleedAmt = 0.7f;    // 0..1
    bool otherOn = false;   float otherDb = 12.0f;    // 0..30 dB
    bool hitsOn = true;     float hitsAmt = 1.0f;     // 0..1
    bool mouthOn = true;    float mouthAmt = 0.6f;    // sensibilidad 0..1
    bool breathOn = true;   float breathDb = 8.0f;    // 0..24 dB
    bool clipRepair = true;
    bool anySpectral() const { return (noiseOn && noiseDb > 0.1f) || (bleedOn && bleedAmt > 0.01f); }
};

struct CleanResult
{
    juce::AudioBuffer<float> cleaned;   // audio reparado y con ruido / sangrado reducidos
    std::vector<float> gainDb;          // ganancia de limpieza por trama (respiraciones, otras voces, huecos)
    std::vector<float> reductionDb;     // reducción espectral media por trama (para la pantalla)
    std::vector<Event> fixedClicks;
    int nClicks = 0, nHits = 0, nBreaths = 0, nOthers = 0, nClipsFixed = 0, nClipsUnfixable = 0;
    float coherence = -1.0f;            // parecido entre el beat de referencia y el sangrado (0..1), -1 sin referencia
    int refDelay = 0;                   // muestras
    float noiseFloorDb = -120.0f;       // ruido medio en las pausas (dBFS)
};

struct Cleaner
{
    static bool process (const Take&, const Analysis&, const CleanParams&, CleanResult&,
                         const std::atomic<bool>& cancel, const std::function<void (float)>& progress = {});
};
} // namespace levelivo

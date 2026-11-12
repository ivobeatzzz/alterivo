// levelIVO — DECISIÓN de nivel (motor A). A partir de la detección decide cuánto subir o bajar cada tramo,
// de arriba abajo (tendencia de frase → palabra → sílaba) sin corregir dos veces lo mismo, y construye
// una única curva de ganancia que no cruza ataques ni hace pasar picos del techo.
#pragma once
#include "../analysis/Analysis.h"

namespace levelivo
{
struct LevelParams
{
    float phrase = 0.9f, word = 0.8f, syllable = 0.3f;   // fuerza de cada nivel (0..1)
    float intensity = 1.0f;                              // multiplicador global (0..1.5)
    float sensitivity = 0.55f;                           // 0 = sólo errores grandes, 1 = corrige variaciones pequeñas
    float maxGain = 9.0f, maxAtt = 9.0f;                 // dB
    float smoothingMs = 30.0f;
    bool peakProtect = true;  float ceilingDb = -1.0f;
    bool consonantProtect = true;
    bool targetOn = false;    float targetLufs = -18.0f;
    float beatFollow = 0.0f;                             // 0..1: el objetivo sigue al nivel del beat
};

// Edición manual del usuario sobre la toma (muestras)
struct Edit
{
    enum Kind { offset = 0, lock = 1, expression = 2 };
    int kind = offset;
    int a = 0, b = 0;
    float value = 0;    // dB (offset) o ganancia fijada (lock)
};

struct WordInfo
{
    float levelDb = 0;      // medida
    float trendDb = 0;      // tendencia local
    float corrDb = 0;       // corrección decidida (sin suavizar)
    float conf = 1;
    int reason = 0;         // 0 = ninguna, 1 = deriva, 2 = se pierde, 3 = sobresale, 4 = sílaba, 5 = manual, 6 = bloqueada
    bool expression = false, locked = false;
};

struct LevelResult
{
    std::vector<float> gainDb;        // curva final por trama (lo que se aplica)
    std::vector<float> targetDb;      // curva antes de suavizar (escalones por palabra)
    std::vector<WordInfo> words;
    float meanAbsCorr = 0;            // corrección media en las zonas con voz
    float staticOffset = 0;           // objetivo de sonoridad
    float spreadBefore = 0, spreadAfter = 0;   // dispersión entre palabras (dB)
};

struct Leveler
{
    // framePeakDb: pico por trama de lo que entra a la nivelación (tras la limpieza) para proteger el techo
    // extraGainDb: ganancia de la limpieza por trama (respiraciones, huecos) que se suma antes del techo
    static void decide (const Analysis&, const LevelParams&, const std::vector<Edit>&,
                        const std::vector<float>& framePeakDb, const std::vector<float>& extraGainDb,
                        LevelResult& out);
};
} // namespace levelivo

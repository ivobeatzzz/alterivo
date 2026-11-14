// reverseIVO — AJUSTES DE GENERACIÓN. Todo lo que define un reverse. Lo leen igual el modo AUTO y el MANUAL:
// los dos usan exactamente el mismo motor, así que la calidad nunca depende del modo.
// C++ puro (sin JUCE): las pruebas de tests/ compilan el motor sin el framework.
#pragma once
#include <cmath>

namespace reverseivo
{
enum class Fit { stretch = 0, natural = 1 };              // encajar toda la región en la duración, o a su velocidad
enum class Ending { soft = 0, natural = 1, hard = 2 };     // cómo llega al golpe final
enum class Division { d16 = 0, d8t, d8, d8d, d4t, d4, d4d, d2, d1 };

// Divisiones musicales en negras (1/16 = 0.25 negras…)
inline double divisionQuarters (int d)
{
    static const double q[] = { 0.25, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 4.0 };
    return q[d < 0 ? 0 : (d > 8 ? 8 : d)];
}

struct RenderSettings
{
    // ---- CREATE
    int durationIndex = 2;        // 0: 1/4 de compás, 1: 1/2, 2: 1 compás, 3: 2 compases, 4: 4 compases, 5: libre
    double freeSeconds = 2.0;     // duración libre (o si el DAW no da tempo)
    bool smartSync = true;
    Fit fit = Fit::stretch;
    float intensity = 0.6f;       // 0..1  cuánto lavado de reverb crece y cuán agresiva es la subida
    float pitch = 0.0f;           // semitonos (todo el efecto)
    float character = 0.3f;       // 0 limpio .. 1 sucio (saturación, armónicos, textura)

    // ---- CUSTOMIZE · volumen
    float volStartDb = -18.0f, volEndDb = 0.0f;
    float volCurve = 0.4f;        // 0 = lineal en dB, 1 = muy exponencial (todo al final)
    float fadeIn = 0.0f;          // 0..1 de la duración
    // ---- pitch
    float pitchStart = 0.0f, pitchEnd = 0.0f;   // evolución en semitonos (se suma a pitch)
    float pitchCurve = 0.5f;
    float microPitch = 0.0f;      // 0..1  pequeñas variaciones orgánicas (hasta ±25 cents)
    // ---- filtro
    float hpStart = 20.0f, hpEnd = 20.0f;         // Hz
    float lpStart = 20000.0f, lpEnd = 20000.0f;   // Hz
    float resonance = 0.15f;      // 0..1
    float filterCurve = 0.5f;
    // ---- transitorio
    Ending ending = Ending::natural;
    float transient = 0.4f;       // 0..1 fuerza del ataque final
    // ---- capas
    bool mainOn = true;  float mainDb = 0.0f;  float mainWidth = 1.0f;
    bool texOn = false;  float texDb = -9.0f;  float texPitch = 12.0f; float texWidth = 1.4f; float texSmear = 0.7f;

    // ---- FX · espacio
    float wash = 0.6f;            // reverb ANTES de invertir (el «swell» clásico de reverse)
    bool reverbOn = true;  float revSize = 0.6f, revDecay = 2.5f, revPredelay = 20.0f, revMix = 0.18f, revDamp = 0.45f;
    bool delayOn = false;  int delayDiv = (int) Division::d8; float delayFb = 0.35f, delayMix = 0.2f; bool pingPong = true;
    // ---- ritmo
    bool tremOn = false;   int tremDiv = (int) Division::d16; float tremDepth = 0.5f;
    bool cutsOn = false;   int cutsFrom = (int) Division::d4, cutsTo = (int) Division::d16; float cutsDepth = 0.8f;
    bool stutterOn = false; int stutterDiv = (int) Division::d16; float stutterLen = 0.25f;   // fracción final repetida
    // ---- carácter
    float drive = 0.0f;           // 0..1 saturación (sobremuestreo 4x)
    float crush = 0.0f;           // 0..1 reducción de bits
    float noise = 0.0f;           // 0..1 capa de ruido que sigue a la envolvente
    float harmonics = 0.0f;       // 0..1 excitador armónico
    float subDb = 0.0f, airDb = 0.0f;
    // ---- estéreo
    float width = 1.0f;           // 0..2
    float autoPan = 0.0f;  int panDiv = (int) Division::d4;
    // ---- FX movement (0..1: cuánto evoluciona cada efecto a lo largo del reverse)
    float moveReverb = 0.5f, moveDelay = 0.0f, moveDrive = 0.0f, movePan = 0.0f, moveFilter = 0.0f, moveWidth = 0.3f, moveTrem = 0.0f;
    // ---- inteligentes
    bool smartEnding = true;
    float preImpact = 0.3f;       // 0..1 tensión en el último tramo
    float tailSeconds = 1.5f;     // cola de reverb/delay después del golpe
    float outFadeMs = 300.0f;     // desvanecimiento final de la cola
    bool clipSafe = true;

    // duración en segundos según tempo y compás
    double durationSeconds (double bpm, double beatsPerBar) const
    {
        if (durationIndex >= 5 || ! smartSync || bpm <= 0.0) return freeSeconds;
        static const double bars[] = { 0.25, 0.5, 1.0, 2.0, 4.0 };
        return bars[durationIndex] * beatsPerBar * 60.0 / bpm;
    }
};
} // namespace reverseivo

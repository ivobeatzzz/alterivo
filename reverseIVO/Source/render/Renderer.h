// reverseIVO — MOTOR DE GENERACIÓN. Función pura: (audio fuente, región, ajustes, tempo) → reverse terminado.
// Corre en segundo plano (nunca en el hilo de audio) y no depende de JUCE: se prueba igual dentro y fuera del plugin.
#pragma once
#include "Settings.h"
#include <atomic>
#include <memory>
#include <vector>

namespace reverseivo
{
struct Source
{
    std::vector<float> l, r;
    double sampleRate = 48000.0;
    int size() const { return (int) l.size(); }
};
using SourcePtr = std::shared_ptr<const Source>;

struct Tempo
{
    double bpm = 0.0;            // 0 = el DAW no da tempo
    double beatsPerBar = 4.0;
    double bpmOr120() const { return bpm > 0.0 ? bpm : 120.0; }
};

struct Result
{
    std::vector<float> l, r;     // reverse completo: subida + cola después del golpe
    int hitIndex = 0;            // muestra exacta del golpe (= duración)
    double sampleRate = 48000.0;
    double bpm = 0.0, seconds = 0.0;
    std::vector<float> peaks;    // forma de onda para la pantalla (máximos por tramo)
    std::vector<float> volLane, pitchLane, filterLane;   // curvas de CUSTOMIZE (0..1) para dibujarlas
    float gainDb = 0.0f;         // ganancia aplicada para dejar el pico en su sitio
    int generation = 0;
    int size() const { return (int) l.size(); }
};
using ResultPtr = std::shared_ptr<const Result>;

// regionStart/regionEnd en muestras de la fuente. cancel: si pasa a true, se abandona (llegó un ajuste más nuevo).
std::shared_ptr<Result> render (const Source& src, int regionStart, int regionEnd, const RenderSettings& s, const Tempo& tempo,
                  const std::atomic<bool>* cancel = nullptr);

// Las mismas curvas que usa el motor, para dibujarlas en la interfaz sin generar nada (k puntos)
void laneCurves (const RenderSettings& s, int k, std::vector<float>& vol, std::vector<float>& pitch, std::vector<float>& filter);
} // namespace reverseivo

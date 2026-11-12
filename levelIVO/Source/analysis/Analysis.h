// levelIVO — DETECCIÓN. Mide la toma (sonoridad, bandas, periodicidad), la segmenta en frases, palabras y
// sílabas con su confianza, y localiza respiraciones, otras voces, golpes al micro, clics y clipping.
// No decide nada: la decisión de ganancia está en leveling/ y la reparación en cleanup/.
#pragma once
#include "Take.h"
#include <atomic>
#include <functional>

namespace levelivo
{
// Segmento en tramas de análisis (5 ms)
struct Seg
{
    int a = 0, b = 0;      // [a, b)
    float conf = 1.0f;     // 0..1: seguridad del corte
    int parent = -1;       // frase de una palabra, palabra de una sílaba
    int len() const { return b - a; }
};
// Evento en muestras
struct Event
{
    int a = 0, b = 0;      // [a, b) en muestras de la toma
    int channel = -1;      // -1 = todos
    float strength = 0;    // fuerza de la detección (clics) o nivel de recorte (clipping)
    bool repairable = true;
};

struct Analysis
{
    double sampleRate = 48000;
    int hop = 240;                      // muestras por trama (5 ms)
    int nFrames = 0;
    std::vector<float> L;               // sonoridad corta ponderada K (dB, ventana 10 ms)
    std::vector<float> V;               // energía de vocal 300-2500 Hz (dB, 20 ms)
    std::vector<float> H;               // energía alta > 3 kHz (dB)
    std::vector<float> Lo;              // energía grave < 100 Hz (dB) para golpes
    std::vector<float> tilt;            // brillo: 2-6 kHz menos 200-900 Hz (dB)
    std::vector<float> per;             // periodicidad 0..1 (1 = muy afinada / sonora)
    std::vector<float> f0;              // Hz (0 = sorda)
    std::vector<float> refL;            // sonoridad del beat de referencia (dB) si existe
    std::vector<float> peakDb;          // pico por trama del audio original (dB)
    std::vector<uint8_t> active;        // voz principal sonando
    std::vector<uint8_t> regionStart;   // 1 en la primera trama de cada región (corte duro)

    std::vector<Seg> phrases, words, syllables, breaths, others;
    std::vector<int> onsets;            // tramas con ataque (consonantes, golpes de voz)
    std::vector<Event> clips, hits, clicks;
    std::vector<float> clipLevel;       // por canal (0 = sin clipping)

    float floorDb = -80, peakLevelDb = -10, voiceDb = -30;  // suelo, pico (p99) y nivel típico de la voz
    float f0Median = 0;

    int frameOf (int sample) const { return std::clamp (sample / std::max (1, hop), 0, std::max (0, nFrames - 1)); }
    int sampleOf (int frame) const { return frame * hop; }
    float segLevel (const Seg& s) const;  // media de energía de L en el segmento
};
using AnalysisPtr = std::shared_ptr<const Analysis>;

struct Analyzer
{
    // progress: 0..1. Devuelve false si se canceló.
    static bool analyze (const Take& take, Analysis& out, const std::atomic<bool>& cancel,
                         const std::function<void (float)>& progress = {});
};
} // namespace levelivo

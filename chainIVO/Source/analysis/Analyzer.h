// ANALYSIS ENGINE — estudia la pista (y la voz principal si llega por sidechain) en un hilo aparte.
// El hilo de audio sólo copia muestras a un FIFO sin bloqueos; si el FIFO se llena, se descartan muestras
// (nunca se bloquea el audio). Todo lo que se mide aquí es real y se usa en AUTO y en la pantalla.
#pragma once
#include "../dsp/DspCore.h"
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

namespace chainivo
{
constexpr int kBands = 24;   // tercios de octava agrupados, de 63 Hz a 16 kHz

struct AnalysisResult
{
    // Cantidad de material
    float activeSeconds = 0.0f, totalSeconds = 0.0f;
    float progress = 0.0f;           // 0..1 (8 s de voz = 1)
    bool ready = false;

    // Pista
    float levelP50 = -40.0f, levelP90 = -30.0f, noiseFloor = -90.0f, dynamicSpread = 10.0f;
    float sibilance = -20.0f;        // dB de 5-10 kHz frente a toda la voz
    float sibilantFreq = 6500.0f;
    float harsh = -15.0f, mud = -12.0f, nasal = -12.0f, air = -30.0f;
    float centroid = 2000.0f;
    float voicedRatio = 0.7f;        // tramos con tono claro (una voz afinada) frente a ruidosos (susurro, coro denso)
    float activity = 0.5f;           // fracción del tiempo con voz
    float phraseMedian = 1.5f, phraseShortRatio = 0.0f;   // s; fracción de eventos de menos de 0.25 s
    float stereoCorrelation = 1.0f, sideRatio = 0.0f;     // 1 = mono; sideRatio en dB-ish lineal 0..1
    std::array<float, kBands> spectrum {};                // dB, media de los tramos con voz

    // Voz principal (sólo si hay sidechain con señal)
    bool hasLead = false;
    std::array<float, kBands> leadSpectrum {};
    float overlapWhenLead = 0.0f;    // P(pista suena | principal suena)
    float overlapWhenSilent = 0.0f;  // P(pista suena | principal calla)
    float envelopeCorrelation = 0.0f;
    float pitchOffsetSemis = 0.0f;   // mediana de la diferencia de tono cuando suenan juntas
    float pitchAgree = 0.0f;         // fracción de tramos con el mismo tono (±0.5 st)
    float levelVsLead = 0.0f;        // dB
    int conflictBand = -1;           // banda (1.5-6 kHz) donde más se pisan
    float conflictAmount = 0.0f;     // 0..1

    // Instrumental de referencia (archivo que carga el usuario; sólo se guarda su espectro, nunca el audio)
    bool hasInstrumental = false;
    std::array<float, kBands> instrumentalSpectrum {};
};

class Analyzer : private juce::Thread
{
public:
    Analyzer();
    ~Analyzer() override;

    void prepare (double sampleRate);
    void releaseResources();
    // Hilo de audio: sin bloqueos ni memoria nueva
    void push (const float* l, const float* r, const float* lead, int n);
    void restart();                       // volver a analizar desde cero
    AnalysisResult getResult() const;     // hilo de la interfaz
    static float bandCentre (int b);
    // Espectro medio (dB por banda) de un archivo: para el instrumental de referencia. Se llama fuera del audio.
    static bool spectrumOfFile (const juce::File&, std::array<float, kBands>& out, std::atomic<bool>& cancel);

private:
    void run() override;
    void analyseFrame();
    void publish();

    double sr = 48000.0;
    static constexpr int kFifo = 1 << 19, kFft = 2048, kHop = 1024;
    juce::AbstractFifo fifo { kFifo };
    std::vector<float> fl, fr, fs;               // FIFO
    std::vector<float> wl, wr, ws;               // ventana deslizante
    int filled = 0;
    std::atomic<bool> restartRequested { false }, prepared { false };

    juce::dsp::FFT fft { 11 };
    std::vector<float> window, work, work2;
    std::array<int, kBands + 1> bandEdge {};

    // Estadísticos
    struct Hist
    {
        std::array<int, 240> bins {}; int count = 0;
        void add (float db) { const int b = juce::jlimit (0, 239, (int) ((db + 120.0f) * 2.0f)); ++bins[(size_t) b]; ++count; }
        float pct (float p) const
        {
            if (count == 0) return -120.0f;
            int acc = 0; const int target = (int) (p * (float) count);
            for (int b = 0; b < 240; ++b) { acc += bins[(size_t) b]; if (acc > target) return (float) b * 0.5f - 120.0f; }
            return 0.0f;
        }
        void clear() { bins.fill (0); count = 0; }
    };
    Hist allHist, activeHist;
    int frames = 0, activeFrames = 0, voicedFrames = 0;
    std::array<double, kBands> ltas {}, leadLtas {};
    int leadActiveFrames = 0;
    double sibSum = 0, harshSum = 0, mudSum = 0, nasalSum = 0, airSum = 0, centroidSum = 0;
    double corrLR = 0, eL = 0, eR = 0, eS = 0, eM = 0;
    std::array<double, kBands> sibBandSum {};
    // Frases
    int runFrames = 0, gapFrames = 0;
    std::vector<float> phrases;
    // Relación con la voz principal
    int bothActive = 0, leadOnly = 0, leadSilentFrames = 0, trackWhenLeadSilent = 0;
    double ecSum = 0, ecA = 0, ecB = 0, ecAA = 0, ecBB = 0; int ecN = 0;
    std::vector<float> pitchDiffs;
    double levelDiffSum = 0;

    mutable juce::CriticalSection resultLock;
    AnalysisResult result;
};
} // namespace chainivo

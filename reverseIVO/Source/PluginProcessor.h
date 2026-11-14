// reverseIVO — PROCESADOR.
// Hilo de audio: captura lo que entra con la posición musical del DAW y reproduce el reverse ya generado,
// sincronizado con la línea de tiempo para que termine exactamente en el golpe (sin latencia: todo está calculado
// antes). También reproduce la escucha previa, el disparo por MIDI y aplica el ducking de la cola.
// La generación corre en el motor (segundo plano). La interfaz sólo lee y escribe parámetros y acciones.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "core/Params.h"
#include "core/Engine.h"
#include "presets/Presets.h"

namespace reverseivo
{
// Cola sin bloqueos del hilo de audio al hilo que guarda la captura
class CaptureFifo
{
public:
    struct Header { double ppq; bool playing; int n; };
    void prepare (int capacitySamples);
    bool push (double ppq, bool playing, const float* l, const float* r, int n);   // hilo de audio
    bool pop (Header& h, juce::AudioBuffer<float>& out);                           // hilo de captura
    void reset() { fifo.reset(); hfifo.reset(); }
private:
    juce::AudioBuffer<float> data;
    juce::AbstractFifo fifo { 1 }, hfifo { 1 };
    std::vector<Header> headers;
};

class ReverseIvoProcessor : public juce::AudioProcessor,
                            private juce::AudioProcessorValueTreeState::Listener,
                            private juce::Timer,
                            private juce::Thread
{
public:
    ReverseIvoProcessor();
    ~ReverseIvoProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "reverseIVO"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;
    PresetBank presets;
    juce::String presetName { "Init" }, presetCategory;
    std::atomic<float> uiScale { 1.0f };

    // ---- acciones (hilo de mensajes)
    void toggleCapture();
    bool isCapturing() const { return capturing.load(); }
    double capturedSeconds() const { return capSeconds.load(); }
    void importFile (const juce::File&);
    void generate();                         // GENERATE (en AUTO también se llama solo)
    bool hasPendingChanges() const { return dirty.load(); }
    bool isGenerating() const { return engine.busy(); }
    void setPreview (bool play);
    bool isPreviewing() const { return previewing.load(); }
    void setSourcePreview (bool play);
    bool isSourcePreviewing() const { return srcPreviewing.load(); }
    void setRegion (int start, int end);     // en muestras de la fuente
    void autoRegion();
    void nudgeHit (double quarters);
    void setHitAtPlayhead();
    bool exportWav (const juce::File&, juce::String& error);
    juce::File exportForDrag();              // archivo temporal para arrastrar al DAW
    juce::File defaultExportFile() const;
    void loadPreset (int index);
    bool saveUserPreset (const juce::String& name);
    void resetParameters();

    // ---- lo que lee la interfaz
    SourcePtr getSource() const { const juce::SpinLock::ScopedLockType l (resultLock); return source; }
    ResultPtr getResult() const { const juce::SpinLock::ScopedLockType l (resultLock); return current; }
    std::vector<float> sourcePeaks() const { const juce::SpinLock::ScopedLockType l (resultLock); return srcPeaks; }
    int regionStart() const { return regA.load(); }
    int regionEnd() const { return regB.load(); }
    int playIndex() const { return playIdx.load(); }        // posición en el reverse (−1 = parado)
    int sourcePlayIndex() const { return srcPlayIdx.load(); }
    bool hasHit() const { return ! std::isnan (hitPpq.load()); }
    juce::String hitText() const;            // «HIT 17.1»
    juce::String durationText() const;       // «1 BAR · 2.000 s»
    juce::String tempoText() const;          // «120 BPM · 4/4»
    juce::String statusText() const;
    double hostBpm() const { return bpm.load(); }
    float outLevel() const { return outPeak.load(); }
    double renderMs() const { return engine.lastMs(); }
    juce::String sourceName;

private:
    void parameterChanged (const juce::String&, float) override;
    void timerCallback() override;
    void run() override;                     // hilo que vacía la captura
    void stopCapture();
    void setSource (std::shared_ptr<Source> s, double ppqAtStart, const juce::String& name, bool keepRegion = false);
    void requestRender();
    void publish (ResultPtr r);
    void saveSourceAsync();
    void loadSourceFile (const juce::File&);
    Tempo currentTempo() const;
    static std::vector<float> computePeaks (const Source&, int bins);

    Engine engine;
    int generation = 0, publishedGeneration = -1;
    std::atomic<bool> dirty { false };
    juce::uint32 dirtySince = 0;
    double renderedBpm = 0.0, renderedBeats = 4.0;

    // fuente y resultado (lectura del hilo de audio con try-lock: nunca espera)
    SourcePtr source;
    ResultPtr current, audioResult;
    SourcePtr audioSource;
    mutable juce::SpinLock resultLock;
    std::vector<std::pair<juce::uint32, std::shared_ptr<const void>>> retired;   // se liberan aquí, no en el audio
    std::vector<float> srcPeaks;
    std::atomic<int> regA { 0 }, regB { 0 };
    std::atomic<double> hitPpq { std::numeric_limits<double>::quiet_NaN() };
    double sourcePpq = std::numeric_limits<double>::quiet_NaN();   // posición musical de la primera muestra capturada
    juce::File sourceFile;

    // captura
    CaptureFifo fifo;
    std::atomic<bool> capturing { false };
    std::atomic<double> capSeconds { 0.0 };
    std::vector<float> capL, capR;
    double capPpq = std::numeric_limits<double>::quiet_NaN();
    bool capSawPlay = false;
    std::mutex capMutex;
    juce::uint32 stopSeenMs = 0;

    // reproducción
    std::atomic<bool> previewing { false }, srcPreviewing { false }, isPlaying { false };
    std::atomic<int> playIdx { -1 }, srcPlayIdx { -1 };
    int previewPos = 0, srcPreviewPos = 0, midiPos = 0;
    bool midiActive = false;
    float midiGain = 1.0f;
    juce::int64 lastIdx = -1;
    float ramp = 0.0f, duckEnv = 0.0f, outGainSmooth = 1.0f;
    std::atomic<double> bpm { 0.0 }, beatsPerBar { 4.0 }, lastPpq { std::numeric_limits<double>::quiet_NaN() };
    std::atomic<int> sigNum { 4 }, sigDen { 4 };
    std::atomic<float> outPeak { 0.0f };
    double sr = 48000.0;

    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverseIvoProcessor)
};
} // namespace reverseivo

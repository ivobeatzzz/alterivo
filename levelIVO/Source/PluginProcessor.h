// levelIVO — PROCESADOR. Hilo de audio: captura la toma con la posición del DAW y, una vez analizada, reproduce el
// resultado sincronizado con la línea de tiempo (sin latencia: todo está calculado de antemano). El análisis, la
// limpieza y la decisión de nivel corren en el motor (segundo plano). La interfaz sólo lee y escribe parámetros.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "core/Params.h"
#include "core/Engine.h"
#include "realtime/RtLeveler.h"
#include "presets/Presets.h"

namespace levelivo
{
// Cola sin bloqueos del hilo de audio al hilo que guarda la captura
class CaptureFifo
{
public:
    struct Header { juce::int64 hostPos; int n; };
    void prepare (int capacitySamples)
    {
        data.setSize (3, capacitySamples); data.clear();
        fifo.setTotalSize (capacitySamples); fifo.reset();
        hfifo.setTotalSize (4096); hfifo.reset(); headers.resize (4096);
    }
    // hilo de audio
    bool push (juce::int64 hostPos, const float* l, const float* r, const float* ref, int n);
    // hilo de guardado
    bool pop (Header& h, juce::AudioBuffer<float>& out);
    void reset() { fifo.reset(); hfifo.reset(); }
    std::atomic<bool> overflow { false };
private:
    juce::AudioBuffer<float> data;
    juce::AbstractFifo fifo { 1 }, hfifo { 1 };
    std::vector<Header> headers;
};

class LevelIvoProcessor : public juce::AudioProcessor,
                          private juce::AudioProcessorValueTreeState::Listener,
                          private juce::Timer,
                          private juce::AsyncUpdater,
                          private juce::Thread
{
public:
    LevelIvoProcessor();
    ~LevelIvoProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "levelIVO"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    void updateTrackProperties (const TrackProperties& p) override { trackName = p.name.value_or (juce::String()); }

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;
    Engine engine;
    PresetBank presets;
    juce::String presetName { "Balanced" };
    std::atomic<float> uiScale { 1.0f };

    // ---- acciones (hilo de mensajes)
    void startCapture();
    void stopCapture (bool analyze = true);
    bool isCapturing() const { return capturing.load(); }
    double capturedSeconds() const;
    void analyzeAgain();
    void apply();
    bool isApplied() const { return applied.load(); }
    void loadFile (const juce::File&);
    void exportFile (const juce::File&, bool float32, std::function<void (juce::String)> done);
    bool isExporting() const { return exporting.load(); }
    void setRideMode (bool on);
    bool isRideMode() const { return rideMode.load(); }
    void setPreview (bool play, int fromTakeSample = -1);
    bool isPreviewing() const { return previewing.load(); }
    void loadPreset (int index);
    void savePreset (const juce::String& name);

    // ---- ediciones (forman parte del estado y del deshacer)
    std::vector<Edit> getEdits() const;
    void addEdit (int kind, int a, int b, float value);
    void clearEdits (int a, int b);

    // ---- lo que lee la interfaz
    ResultPtr getResult() const { return current; }
    juce::int64 playTakePos() const { return playPos.load(); }
    bool isPlaying() const { return playing.load(); }
    bool trackChanged() const { return mismatch.load(); }
    bool sidechainActive() const { return scActive.load(); }
    float inLevel() const { return inPeak.load(); }
    float outLevel() const { return outPeak.load(); }
    float gainNow() const { return grNow.load(); }
    juce::String getTrackName() const { return trackName; }
    juce::String statusText() const;
    juce::String loadError;

private:
    void parameterChanged (const juce::String&, float) override;
    void timerCallback() override;
    void handleAsyncUpdate() override;
    void run() override;   // hilo que vacía la captura
    void publish (ResultPtr r);
    void drainCapture();
    TakePtr buildTake();
    void saveTakeAsync (TakePtr t);
    void loadTakeFile (const juce::File&, std::vector<Region> regions, int voiceCh, bool hasRef, juce::String name);
    void pushLevelParams();
    void pushCleanParams();

    // captura
    CaptureFifo fifo;
    struct CapRegion { juce::int64 hostStart = 0, hostEnd = 0; std::vector<float> ch[2]; std::vector<float> ref; };
    std::vector<CapRegion> capRegions;
    mutable std::mutex capMutex;
    std::atomic<bool> capturing { false }, capturedRef { false };
    std::atomic<double> capSeconds { 0.0 };
    int capChannels = 2;
    juce::uint32 stopSeenMs = 0;

    // resultado para el hilo de audio
    ResultPtr current;
    ResultPtr audioResult;
    juce::SpinLock resultLock;
    std::vector<std::pair<juce::uint32, ResultPtr>> retired;
    AnalysisPtr lastAnalysis;
    std::atomic<bool> applied { false }, rideMode { false }, previewing { false }, exporting { false };
    std::atomic<int> previewPos { 0 };

    // estado de reproducción
    std::atomic<juce::int64> playPos { -1 };
    std::atomic<bool> playing { false }, mismatch { false }, scActive { false };
    std::atomic<float> inPeak { 0 }, outPeak { 0 }, grNow { 0 };
    int mismatchCount = 0, matchCount = 0;
    float rideSmooth = 1.0f;
    bool rideGesture = false;

    // tiempo real
    RtLeveler rt;
    int engineMode = 0;
    double sr = 48000;
    int maxBlock = 512;

    // cambios pendientes de parámetros (se pasan al motor desde el temporizador)
    std::atomic<bool> levelDirty { true }, cleanDirty { true }, modeDirty { false };
    juce::uint32 cleanDirtySince = 0;
    bool applyingMode = false;
    int lastMode = 1;

    juce::String trackName;
    juce::int64 editsHash = 0;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    juce::File takeFile; juce::String takeFileId; bool takeFromFile = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelIvoProcessor)
};
} // namespace levelivo

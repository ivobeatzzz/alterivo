#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>

#include "PitchEngine.h"
#include "TubeDrive.h"

class AlterIvoProcessor : public juce::AudioProcessor
{
public:
    AlterIvoProcessor();
    ~AlterIvoProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Presets de fábrica (también aparecen en el menú de presets del DAW)
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // Para la pantalla de la interfaz
    float getDetectedHz() const noexcept { return engine.getDetectedHz(); }
    float getOutputHz() const noexcept   { return engine.getOutputHz(); }

    // Detector de tonalidad (hilo de mensajes): se inicia, se consulta ~30 veces/s y
    // al aceptar un resultado pone Key y Scale
    struct KeyStatus
    {
        enum State { Idle, Analyzing, Done } state = Idle;
        float progress = 0.0f, confidence = 0.0f;   // 0..1
        int root = 0, scale = 0;                     // raíz 0-11; escala 1 mayor, 2 menor
    };
    void startKeyAnalysis();
    void cancelKeyAnalysis();
    const KeyStatus& updateKeyAnalysis();
    static constexpr float kKeyMinSeconds = 8.0f, kKeyMaxSeconds = 30.0f;

    // Perfil de latencia: 0 LOW, 1 NORMAL (no automatizable; se guarda con la sesión)
    void setLatencyProfile (int profile);
    int getLatencyProfile() const noexcept { return latencyProfile.load(); }

    // CPU por módulo (fracción de tiempo real: 0.02 = 2 %) y estado del patrón de tempo
    std::atomic<float> cpuEngine { 0.0f }, cpuDrive { 0.0f }, cpuTotal { 0.0f };
    std::atomic<bool> patternActive { false };
    int getHeldMidiNote() const noexcept { return heldNote.load(); }
    float getFormantEnvelope (float* dest) const noexcept { return engine.getDisplayEnvelope (dest); }
    static constexpr int kFormantPoints = alterivo::PitchEngine::kEnvPoints;

    // Tamaño de la ventana (se guarda con la sesión)
    std::atomic<float> uiScale { 1.0f };

    // Medidores: pico desde la última lectura (lineal)
    float takeInputPeak() noexcept  { return inPeak.exchange (0.0f); }
    float takeOutputPeak() noexcept { return outPeak.exchange (0.0f); }

private:
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int len);
    void handleMidi (const juce::MidiBuffer& midi);
    void noteOn (int note);
    void noteOff (int note);

    alterivo::PitchEngine engine;
    std::atomic<int> latencyProfile { 1 };
    int preparedProfile = 1;
    float driveCurrent = 0.0f, driveTarget = 0.0f, driveStepPerSample = 1.0e-4f;
    alterivo::TubeDrive drive;
    std::array<alterivo::DcBlocker, 2> dcBlockers;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    // Retardo simple (n muestras; n = 0 deja pasar)
    struct DelayLine
    {
        std::vector<float> buf;
        size_t pos = 0;
        void setSize (int n) { buf.assign ((size_t) juce::jmax (0, n), 0.0f); pos = 0; }
        float process (float x) noexcept
        {
            if (buf.empty()) return x;
            const float y = buf[pos];
            buf[pos] = x;
            if (++pos >= buf.size()) pos = 0;
            return y;
        }
    };

    juce::AudioBuffer<float> wetBuffer, cleanBuffer;
    std::array<DelayLine, 2> dryToEngine, cleanToDrive, dryTotal;
    bool oversamplerActive = false;
    int maxBlock = 512;

    juce::SmoothedValue<float> mixSmoothed, engineGain, driveGain;

    std::array<int, 128> noteStack {};
    int noteCount = 0;
    std::atomic<int> heldNote { -1 };
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };

    int currentPreset = 0;

    std::atomic<float>* pPitch   = nullptr;
    std::atomic<float>* pFormant = nullptr;
    std::atomic<float>* pDrive   = nullptr;
    std::atomic<float>* pMix     = nullptr;
    std::atomic<float>* pMode    = nullptr;
    std::atomic<float>* pLink    = nullptr;
    std::atomic<float>* pKey     = nullptr;
    std::atomic<float>* pScale   = nullptr;
    std::atomic<float>* pSource  = nullptr;
    std::atomic<float>* pIntensity = nullptr;
    std::atomic<float>* pOutput    = nullptr;
    std::atomic<float>* pPattern   = nullptr;
    std::atomic<float>* pCompare   = nullptr;
    juce::SmoothedValue<float> abGain;
    juce::int64 engineTicks = 0, driveTicks = 0;
    juce::SmoothedValue<float> outputGain;

    // Nivel igualado
    float levelCoef = 0.0f, levelIn = 0.0f, levelOut = 0.0f;
    juce::SmoothedValue<float> levelGain;
    // El igualador sólo mide mientras el motor sintetiza; al arrancar parte del último valor bueno
    bool engineRunning = false, engineWasRunning = false;
    float levelCoefFast = 0.0f;
    int levelFastSamples = 0, levelFastLeft = 0;
    double sr = 48000.0;

    // Detección de tonalidad: el audio escribe, la interfaz lee (sin bloqueos)
    std::array<std::atomic<float>, 12> keyHist {};
    std::atomic<float> keySeconds { 0.0f };
    std::atomic<bool> keyResetRequest { true };
    KeyStatus keyStatus;
    int keyEvalCount = 0, keyLastCandidate = -1;
    float keyLastSeconds = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlterIvoProcessor)
};

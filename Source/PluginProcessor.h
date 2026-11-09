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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlterIvoProcessor)
};

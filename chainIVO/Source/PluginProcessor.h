// chainIVO — AUDIO CORE: une los motores, la gestión de ganancia, el análisis y AUTO/PRO.
// AUTO y PRO usan exactamente los mismos motores y parámetros: AUTO sólo escribe valores que en PRO se ven y se editan.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "core/Params.h"
#include "engines/EqEngine.h"
#include "engines/CompEngine.h"
#include "engines/HarmonicsEngine.h"
#include "engines/VocalEngine.h"
#include "engines/SpatialEngine.h"
#include "engines/CreativeEngine.h"
#include "analysis/Analyzer.h"
#include "intelligence/Musical.h"
#include "presets/PresetLibrary.h"
#include "intelligence/Group.h"
#include "intelligence/Profile.h"

class ChainIvoProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    ChainIvoProcessor();
    ~ChainIvoProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "chainIVO"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter (chainivo::pid::bypass); }

    // ---------------------------------------------------------------- Estado (hilo de la interfaz)
    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;
    chainivo::PresetLibrary presets;
    chainivo::Analyzer analyzer;

    bool isPro() const;
    void setPro (bool);
    int getVoiceType() const;                  // tipo elegido (o detectado si está en automático)
    bool isVoiceTypeAuto() const;
    void setVoiceType (int typeOrMinusOneForAuto);
    int getGenre() const;
    void setGenre (int);
    juce::String getPresetName() const;
    void loadFactoryPreset (int recipeIndex);
    void loadUserPreset (const juce::File&);
    void autoPropose();                        // propuesta automática sin preset (tipo + estilo + análisis)
    void reanalyse();
    // Instrumental de referencia (archivo): se analiza en segundo plano; sólo se guarda su espectro
    void loadInstrumental (const juce::File&);
    void clearInstrumental();
    // Respuesta de impulso para la reverb IR (se guarda la ruta del archivo en el proyecto)
    void loadIR (const juce::File&);
    void clearIR();
    juce::String getIRName() const { return irFile.getFileNameWithoutExtension(); }
    bool hasIR() const { return space.hasIR(); }
    juce::String getInstrumentalName() const;
    bool isInstrumentalBusy() const { return instBusy.load(); }
    // Exportar un archivo procesado con la cadena actual (WAV 24 bits o 32 bits float), en segundo plano.
    // Nunca escribe sobre el original. done(mensaje) se llama en el hilo de la interfaz.
    void exportFile (const juce::File& in, const juce::File& out, bool float32, std::function<void (juce::String)> done);
    bool isExporting() const { return exporting.load(); }
    void freezeAuto() { stopTimer(); }   // instancias de exportación: nunca reconstruyen la cadena
    bool saveUserPreset (const juce::String& name);
    std::array<int, 6> getOrder() const;
    void moveEngine (int engine, int delta);
    bool isLocked (int engine) const;
    void setLocked (int engine, bool);
    int getQuality() const { return quality.load(); }
    void setQuality (int);
    int getAbSlot() const { return abSlot; }
    void selectAb (int slot);
    void copyAb();

    chainivo::AnalysisResult getAnalysis() const { return lastAnalysis; }
    chainivo::Classification getClassification() const { return lastClass; }
    std::array<juce::String, 6> getExplanations() const { return explanations; }
    int getEnginesBuilt() const { return enginesBuilt; }

    // ---------------------------------------------------------------- Medidas (atómicas)
    chainivo::PeakMeter inMeter, outMeter;
    std::atomic<float> gainMatchDb { 0.0f }, cpuLoad { 0.0f };
    std::atomic<int> clipCount { 0 };
    std::atomic<bool> leadConnected { false };
    std::atomic<float> correlation { 1.0f };   // correlación L/R de la salida (1 = mono, 0 = muy abierto, <0 = problemas de fase)
    std::atomic<int> soloEngine { -1 };
    std::atomic<int> dspErrors { 0 };          // bloques en los que se detectó un valor no válido (se recuperan solos)
    chainivo::UserProfile profile;        // escucha por módulo: sólo suena ese motor (-1 = todos)
    float eqKey() const { return eq.dynamicKey(); }
    float eqPresNow() const { return eq.currentPresGain(); }
    float compGr() const { return comp.gainReduction(); }
    float deessGr() const { return vocal.deessReduction(); }
    float harshGr() const { return vocal.harshReduction(); }
    float breathGr() const { return vocal.breathReduction(); }
    int phraseEnds() const { return space.phraseEndCount(); }
    bool wordActive() const { return creative.wordActive(); }
    bool wordTargeted() const { return creative.wordTargeted(); }
    int wordsDetected() const { return creative.wordsDetected(); }
    chainivo::EqSettings eqSettingsNow() const;   // para dibujar la curva real
    double getSampleRateSafe() const { return currentSr; }
    std::atomic<float> uiScale { 1.0f };
    std::atomic<double> lastBpm { 120.0 };
    int harmLatency() const { return harm.getLatency(); }

private:
    void timerCallback() override;
    void applyBuild (const chainivo::BuildResult&, const juce::String& presetName);
    void readSettings (chainivo::EqSettings&, chainivo::CompSettings&, chainivo::HarmSettings&, chainivo::VocalSettings&,
                       chainivo::SpaceSettings&, chainivo::CreativeSettings&, float intensity) const;
    juce::ValueTree engineSnapshot() const;
    void restoreSnapshot (const juce::ValueTree&);
    void rebuildForQuality();

    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    chainivo::EqEngine eq;
    chainivo::CompEngine comp;
    chainivo::HarmonicsEngine harm;
    chainivo::VocalEngine vocal;
    chainivo::SpatialEngine space;
    chainivo::CreativeEngine creative;

    double currentSr = 48000.0;
    int maxBlock = 512;
    std::atomic<int> quality { (int) chainivo::Quality::normal };
    std::array<std::atomic<int>, 6> order;
    std::vector<float> sideMono;
    juce::AudioBuffer<float> dryBuf;
    std::array<chainivo::DelayLine, 2> dryDelay;
    chainivo::Smooth inGainS, outGainS, mixS, matchS, bypassS;
    chainivo::EqBand kwDryHp, kwDryHs, kwWetHp, kwWetHs;
    double dryPow = 0.0, wetPow = 0.0;
    float matchTarget = 0.0f;

    // Estado del modo AUTO
    chainivo::AnalysisResult lastAnalysis;
    chainivo::Classification lastClass;
    std::array<juce::String, 6> explanations;
    int enginesBuilt = 0;
    int abSlot = 0;
    std::atomic<bool> instBusy { false }, instReady { false }, instCancel { false }, exporting { false };
    std::array<float, chainivo::kBands> instSpectrum {}, instPending {};
    juce::String instName, instPendingName;
    juce::File irFile;
    std::map<juce::String, float> lastAuto;      // valores que puso AUTO (para aprender lo que el usuario cambia)
    std::map<juce::String, int> stableTicks;
    std::map<juce::String, float> lastSeenVal;
    juce::StringArray learnedThisBuild;
    int loggedErrors = 0;
    juce::SharedResourcePointer<chainivo::GroupRegistry> group;
    int groupId = 0;
    juce::String trackName;
    void fillGroup (chainivo::AnalysisResult&) const;
    void updateTrackProperties (const TrackProperties& p) override { trackName = p.name.value_or (juce::String()); }
    juce::CriticalSection instLock;
    juce::ValueTree abSlots[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChainIvoProcessor)
};

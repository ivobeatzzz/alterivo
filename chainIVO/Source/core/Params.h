// chainIVO — parámetros (automatizables) y su versión. Los IDs no se cambian nunca:
// los proyectos guardados dependen de ellos. Cada parámetro lleva versión 1 (ParameterID).
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace chainivo::pid
{
// Globales
inline constexpr const char* input = "input";        inline constexpr const char* output = "output";
inline constexpr const char* mix = "mix";            inline constexpr const char* intensity = "intensity";
inline constexpr const char* gainMatch = "gainMatch"; inline constexpr const char* clipSafe = "clipSafe";
inline constexpr const char* bypass = "bypass";
// EQ
inline constexpr const char* eqOn = "eqOn"; inline constexpr const char* hpf = "hpf"; inline constexpr const char* hpfSlope = "hpfSlope";
inline constexpr const char* lowFreq = "lowFreq"; inline constexpr const char* lowGain = "lowGain";
inline constexpr const char* midFreq = "midFreq"; inline constexpr const char* midGain = "midGain"; inline constexpr const char* midQ = "midQ";
inline constexpr const char* presFreq = "presFreq"; inline constexpr const char* presGain = "presGain"; inline constexpr const char* presQ = "presQ";
inline constexpr const char* presDyn = "presDyn";
inline constexpr const char* airFreq = "airFreq"; inline constexpr const char* airGain = "airGain";
// COMP
inline constexpr const char* compOn = "compOn"; inline constexpr const char* compType = "compType";
inline constexpr const char* compThresh = "compThresh"; inline constexpr const char* compRatio = "compRatio";
inline constexpr const char* compAttack = "compAttack"; inline constexpr const char* compRelease = "compRelease";
inline constexpr const char* compMakeup = "compMakeup"; inline constexpr const char* compMix = "compMix";
// HARMONICS
inline constexpr const char* harmOn = "harmOn"; inline constexpr const char* harmType = "harmType";
inline constexpr const char* harmDrive = "harmDrive"; inline constexpr const char* harmTone = "harmTone"; inline constexpr const char* harmMix = "harmMix";
// VOCAL
inline constexpr const char* vocOn = "vocOn"; inline constexpr const char* deess = "deess"; inline constexpr const char* deessFreq = "deessFreq";
inline constexpr const char* harsh = "harsh"; inline constexpr const char* breath = "breath";
// SPACE
inline constexpr const char* spaceOn = "spaceOn"; inline constexpr const char* revType = "revType"; inline constexpr const char* revMix = "revMix";
inline constexpr const char* revDecay = "revDecay"; inline constexpr const char* revPredelay = "revPredelay"; inline constexpr const char* revTone = "revTone";
inline constexpr const char* dlyType = "dlyType"; inline constexpr const char* dlyNote = "dlyNote"; inline constexpr const char* dlyFeedback = "dlyFeedback";
inline constexpr const char* dlyMix = "dlyMix"; inline constexpr const char* duck = "duck"; inline constexpr const char* width = "width"; inline constexpr const char* doubler = "doubler";
// CREATIVE
inline constexpr const char* creaOn = "creaOn"; inline constexpr const char* creaType = "creaType";
inline constexpr const char* creaNote = "creaNote"; inline constexpr const char* creaAmount = "creaAmount";

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Parámetros que AUTO puede escribir (todos los de los motores; nunca los globales de nivel)
const juce::StringArray& engineParamIds();
// Motor al que pertenece cada parámetro (0..5) o -1 si es global
int engineOf (const juce::String& id);
} // namespace chainivo::pid

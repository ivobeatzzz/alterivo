// levelIVO — parámetros. Cada uno tiene una función DSP real y comprobable (sin controles decorativos).
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../leveling/Leveler.h"
#include "../cleanup/Cleaner.h"

namespace levelivo::id
{
// GLOBAL
inline constexpr const char* mode = "mode";               // Natural, Balanced, Controlled, Manual
inline constexpr const char* engine = "engine";           // Offline, Realtime
inline constexpr const char* autoApply = "autoApply";     // Auto Apply / Revisar antes
inline constexpr const char* listen = "listen";           // Original, Nivel, Limpieza, Final
inline constexpr const char* matchLoud = "matchLoud";     // A/B con volumen igualado
inline constexpr const char* targetOn = "targetOn";
inline constexpr const char* target = "target";           // LUFS
inline constexpr const char* output = "output";
inline constexpr const char* bypass = "bypass";
inline constexpr const char* rideGain = "rideGain";       // ganancia escrita como automatización
// NIVELACIÓN
inline constexpr const char* phrase = "phrase";
inline constexpr const char* word = "word";
inline constexpr const char* syllable = "syllable";
inline constexpr const char* intensity = "intensity";
inline constexpr const char* sensitivity = "sensitivity";
inline constexpr const char* maxGain = "maxGain";
inline constexpr const char* maxAtt = "maxAtt";
inline constexpr const char* smoothing = "smoothing";
inline constexpr const char* peakProtect = "peakProtect";
inline constexpr const char* consonants = "consonants";
inline constexpr const char* beatFollow = "beatFollow";
inline constexpr const char* rtSpeed = "rtSpeed";          // tiempo real: velocidad (ataque/relajación)
// LIMPIEZA
inline constexpr const char* noiseOn = "noiseOn";   inline constexpr const char* noise = "noise";
inline constexpr const char* bleedOn = "bleedOn";   inline constexpr const char* bleed = "bleed";
inline constexpr const char* otherOn = "otherOn";   inline constexpr const char* other = "other";
inline constexpr const char* hitsOn = "hitsOn";     inline constexpr const char* hits = "hits";
inline constexpr const char* mouthOn = "mouthOn";   inline constexpr const char* mouth = "mouth";
inline constexpr const char* breathOn = "breathOn"; inline constexpr const char* breath = "breath";
inline constexpr const char* clipRepair = "clipRepair";
} // namespace levelivo::id

namespace levelivo
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

LevelParams readLevelParams (const juce::AudioProcessorValueTreeState&);
CleanParams readCleanParams (const juce::AudioProcessorValueTreeState&);

// Valores de cada modo (Natural / Balanced / Controlled) para los controles de nivelación
struct ModeValues { float phrase, word, syllable, sensitivity, maxGain, maxAtt; };
ModeValues modeValues (int mode);

// Parámetros que forman parte de cada grupo (para saber qué etapa repetir al cambiarlos)
bool isLevelParam (const juce::String& pid);
bool isCleanParam (const juce::String& pid);
} // namespace levelivo

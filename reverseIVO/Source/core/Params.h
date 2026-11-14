// reverseIVO — PARÁMETROS. Cada uno tiene una función real en el motor (render/Renderer.cpp) o en la
// reproducción (PluginProcessor.cpp): no hay controles decorativos.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../render/Settings.h"

namespace reverseivo::id
{
// CREATE
inline constexpr const char* duration = "duration";       // 1/4 · 1/2 · 1 BAR · 2 BARS · 4 BARS · FREE
inline constexpr const char* freeTime = "freeTime";       // segundos en FREE (o sin tempo)
inline constexpr const char* smartSync = "smartSync";
inline constexpr const char* fit = "fit";                 // STRETCH · NATURAL
inline constexpr const char* intensity = "intensity";
inline constexpr const char* pitch = "pitch";
inline constexpr const char* character = "character";
inline constexpr const char* genMode = "genMode";         // AUTO · MANUAL
// CUSTOMIZE
inline constexpr const char* volStart = "volStart";   inline constexpr const char* volEnd = "volEnd";
inline constexpr const char* volCurve = "volCurve";   inline constexpr const char* fadeIn = "fadeIn";
inline constexpr const char* pitchStart = "pitchStart"; inline constexpr const char* pitchEnd = "pitchEnd";
inline constexpr const char* pitchCurve = "pitchCurve"; inline constexpr const char* microPitch = "microPitch";
inline constexpr const char* hpStart = "hpStart";     inline constexpr const char* hpEnd = "hpEnd";
inline constexpr const char* lpStart = "lpStart";     inline constexpr const char* lpEnd = "lpEnd";
inline constexpr const char* resonance = "resonance"; inline constexpr const char* filterCurve = "filterCurve";
inline constexpr const char* ending = "ending";       inline constexpr const char* transient = "transient";
inline constexpr const char* mainOn = "mainOn";       inline constexpr const char* mainLevel = "mainLevel"; inline constexpr const char* mainWidth = "mainWidth";
inline constexpr const char* texOn = "texOn";         inline constexpr const char* texLevel = "texLevel";
inline constexpr const char* texPitch = "texPitch";   inline constexpr const char* texWidth = "texWidth"; inline constexpr const char* texSmear = "texSmear";
// FX · SPACE
inline constexpr const char* wash = "wash";
inline constexpr const char* reverbOn = "reverbOn";   inline constexpr const char* revSize = "revSize"; inline constexpr const char* revDecay = "revDecay";
inline constexpr const char* revPredelay = "revPredelay"; inline constexpr const char* revMix = "revMix"; inline constexpr const char* revDamp = "revDamp";
inline constexpr const char* delayOn = "delayOn";     inline constexpr const char* delayTime = "delayTime"; inline constexpr const char* delayFb = "delayFb";
inline constexpr const char* delayMix = "delayMix";   inline constexpr const char* pingPong = "pingPong";
// FX · RHYTHM
inline constexpr const char* tremOn = "tremOn";       inline constexpr const char* tremRate = "tremRate"; inline constexpr const char* tremDepth = "tremDepth";
inline constexpr const char* cutsOn = "cutsOn";       inline constexpr const char* cutsFrom = "cutsFrom"; inline constexpr const char* cutsTo = "cutsTo"; inline constexpr const char* cutsDepth = "cutsDepth";
inline constexpr const char* stutterOn = "stutterOn"; inline constexpr const char* stutterRate = "stutterRate"; inline constexpr const char* stutterLen = "stutterLen";
// FX · CHARACTER
inline constexpr const char* drive = "drive";         inline constexpr const char* crush = "crush"; inline constexpr const char* noise = "noise";
inline constexpr const char* harmonics = "harmonics"; inline constexpr const char* sub = "sub"; inline constexpr const char* air = "air";
// FX · STEREO
inline constexpr const char* width = "width";         inline constexpr const char* autoPan = "autoPan"; inline constexpr const char* panRate = "panRate";
// FX · MOVEMENT
inline constexpr const char* moveReverb = "moveReverb"; inline constexpr const char* moveDelay = "moveDelay"; inline constexpr const char* moveDrive = "moveDrive";
inline constexpr const char* movePan = "movePan";     inline constexpr const char* moveFilter = "moveFilter"; inline constexpr const char* moveWidth = "moveWidth";
inline constexpr const char* moveTrem = "moveTrem";
// SMART
inline constexpr const char* smartEnding = "smartEnding"; inline constexpr const char* preImpact = "preImpact";
inline constexpr const char* tail = "tail";           inline constexpr const char* outFade = "outFade"; inline constexpr const char* clipSafe = "clipSafe";
// Reproducción (tiempo real: no regeneran)
inline constexpr const char* output = "output";       // nivel del reverse
inline constexpr const char* through = "through";     // deja pasar la pista (para usarlo en la propia voz)
inline constexpr const char* ducking = "ducking";     inline constexpr const char* duckRelease = "duckRelease";
} // namespace reverseivo::id

namespace reverseivo
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
RenderSettings readSettings (const juce::AudioProcessorValueTreeState&);
bool isPlaybackParam (const juce::String& pid);       // los que no obligan a generar de nuevo
juce::String helpFor (const juce::String& pid);       // texto de ayuda (tooltip) de cada control
juce::StringArray divisionNames();
} // namespace reverseivo

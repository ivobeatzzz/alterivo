#include "Presets.h"
#include "../core/Params.h"

namespace reverseivo
{
namespace
{
// divisiones: 0 1/16 · 1 1/8T · 2 1/8 · 3 1/8D · 4 1/4T · 5 1/4 · 6 1/4D · 7 1/2 · 8 1 BAR
// duración:   0 1/4 · 1 1/2 · 2 1 BAR · 3 2 BARS · 4 4 BARS · 5 FREE
using V = std::vector<std::pair<juce::String, float>>;
Preset P (const char* cat, const char* name, V v) { Preset p; p.category = cat; p.name = name; p.values = std::move (v); return p; }

std::vector<Preset> makeFactory()
{
    using namespace id;
    return {
        // ---------------------------------------------------------------- CLEAN
        P ("CLEAN", "Clean Vocal Reverse", { { duration, 2 }, { intensity, 0.45f }, { wash, 0.45f }, { character, 0.05f }, { revMix, 0.12f },
                                              { moveReverb, 0.3f }, { volStart, -20.0f }, { preImpact, 0.2f } }),
        P ("CLEAN", "Soft Reverse", { { duration, 2 }, { intensity, 0.3f }, { wash, 0.55f }, { character, 0.0f }, { ending, 0 }, { transient, 0.15f },
                                       { volStart, -24.0f }, { volCurve, 0.3f }, { lpStart, 4000.0f }, { lpEnd, 9000.0f }, { revMix, 0.2f }, { preImpact, 0.1f } }),
        P ("CLEAN", "Natural Reverse", { { duration, 1 }, { fit, 1 }, { intensity, 0.35f }, { wash, 0.2f }, { reverbOn, 0 }, { character, 0.0f },
                                          { volStart, -12.0f }, { volCurve, 0.25f }, { preImpact, 0.0f }, { tail, 0.3f } }),
        // ---------------------------------------------------------------- UNDERGROUND
        P ("UNDERGROUND", "Crank Riser", { { duration, 2 }, { intensity, 0.85f }, { character, 0.6f }, { drive, 0.35f }, { cutsOn, 1 }, { cutsFrom, 5 }, { cutsTo, 0 },
                                            { cutsDepth, 0.7f }, { pitchStart, -5.0f }, { pitchEnd, 0.0f }, { preImpact, 0.6f }, { harmonics, 0.3f },
                                            { ending, 2 }, { transient, 0.7f }, { moveDrive, 0.6f }, { sub, 3.0f } }),
        P ("UNDERGROUND", "Dark Vocal Rise", { { duration, 2 }, { intensity, 0.65f }, { pitch, -5.0f }, { lpStart, 600.0f }, { lpEnd, 5000.0f }, { moveFilter, 0.3f },
                                                { texOn, 1 }, { texPitch, -12.0f }, { texLevel, -10.0f }, { revDecay, 4.0f }, { revMix, 0.25f },
                                                { character, 0.35f }, { sub, 4.0f }, { air, -3.0f } }),
        P ("UNDERGROUND", "Aggressive Reverse", { { duration, 2 }, { intensity, 1.0f }, { drive, 0.55f }, { harmonics, 0.5f }, { ending, 2 }, { transient, 0.9f },
                                                   { preImpact, 0.8f }, { crush, 0.15f }, { delayOn, 1 }, { delayTime, 0 }, { delayFb, 0.3f }, { delayMix, 0.15f },
                                                   { moveDrive, 0.7f }, { character, 0.5f } }),
        P ("UNDERGROUND", "Distorted Vocal Rise", { { duration, 2 }, { intensity, 0.8f }, { drive, 0.8f }, { crush, 0.35f }, { noise, 0.35f }, { character, 0.8f },
                                                     { pitchStart, -12.0f }, { pitchEnd, 0.0f }, { pitchCurve, 0.7f }, { moveDrive, 0.8f }, { ending, 2 } }),
        // ---------------------------------------------------------------- ATMOSPHERIC
        P ("ATMOSPHERIC", "Dreamy Reverse", { { duration, 3 }, { intensity, 0.6f }, { wash, 0.85f }, { revSize, 0.85f }, { revDecay, 5.0f }, { revMix, 0.3f },
                                               { texOn, 1 }, { texPitch, 12.0f }, { texLevel, -12.0f }, { texSmear, 0.8f }, { delayOn, 1 }, { delayTime, 6 },
                                               { delayFb, 0.4f }, { delayMix, 0.18f }, { width, 1.4f }, { moveWidth, 0.6f }, { air, 3.0f }, { microPitch, 0.4f },
                                               { character, 0.1f }, { lpStart, 3000.0f }, { lpEnd, 16000.0f }, { ending, 0 }, { tail, 3.0f } }),
        P ("ATMOSPHERIC", "Wide Reverse", { { duration, 2 }, { width, 1.8f }, { moveWidth, 0.8f }, { autoPan, 0.3f }, { panRate, 5 }, { delayOn, 1 }, { delayTime, 2 },
                                             { pingPong, 1 }, { delayFb, 0.35f }, { delayMix, 0.22f }, { texOn, 1 }, { texWidth, 2.0f }, { texLevel, -12.0f } }),
        P ("ATMOSPHERIC", "Ambient Rise", { { duration, 4 }, { intensity, 0.7f }, { wash, 1.0f }, { texOn, 1 }, { texLevel, -6.0f }, { texPitch, 7.0f }, { texSmear, 1.0f },
                                             { revDecay, 7.0f }, { revMix, 0.35f }, { noise, 0.15f }, { moveFilter, 0.6f }, { lpStart, 1200.0f }, { ending, 0 },
                                             { tail, 4.0f }, { outFade, 1500.0f } }),
        P ("ATMOSPHERIC", "Long Vocal Transition", { { duration, 4 }, { intensity, 0.7f }, { wash, 0.7f }, { pitchStart, -12.0f }, { pitchEnd, 0.0f },
                                                      { pitchCurve, 0.75f }, { moveFilter, 0.5f }, { preImpact, 0.5f }, { tail, 3.0f }, { revMix, 0.25f } }),
        // ---------------------------------------------------------------- TRANSITIONS
        P ("TRANSITIONS", "Short Impact", { { duration, 0 }, { intensity, 0.8f }, { wash, 0.35f }, { ending, 2 }, { transient, 0.9f }, { preImpact, 0.7f },
                                             { tail, 0.6f }, { revDecay, 1.2f }, { volStart, -14.0f } }),
        P ("TRANSITIONS", "Long Build", { { duration, 3 }, { intensity, 0.9f }, { pitchStart, -7.0f }, { pitchEnd, 0.0f }, { moveFilter, 0.6f }, { lpStart, 900.0f },
                                           { preImpact, 0.7f }, { moveReverb, 0.8f }, { noise, 0.2f }, { harmonics, 0.3f } }),
        P ("TRANSITIONS", "Rhythmic Build", { { duration, 3 }, { intensity, 0.75f }, { cutsOn, 1 }, { cutsFrom, 5 }, { cutsTo, 0 }, { cutsDepth, 0.85f },
                                               { delayOn, 1 }, { delayTime, 2 }, { pingPong, 1 }, { delayFb, 0.3f }, { delayMix, 0.15f },
                                               { stutterOn, 1 }, { stutterRate, 0 }, { stutterLen, 0.12f }, { preImpact, 0.5f } }),
        P ("TRANSITIONS", "Final Transition", { { duration, 4 }, { intensity, 1.0f }, { wash, 0.9f }, { texOn, 1 }, { texPitch, 12.0f }, { texLevel, -9.0f },
                                                 { pitchStart, -12.0f }, { pitchEnd, 0.0f }, { moveFilter, 0.7f }, { lpStart, 700.0f }, { noise, 0.25f },
                                                 { sub, 4.0f }, { air, 4.0f }, { preImpact, 1.0f }, { ending, 2 }, { transient, 1.0f }, { tail, 3.5f },
                                                 { revMix, 0.28f }, { delayOn, 1 }, { delayTime, 5 }, { delayFb, 0.35f } }),
    };
}

bool isPresetParam (const juce::String& pid) { return ! isPlaybackParam (pid); }
} // namespace

PresetBank::PresetBank() : factory (makeFactory()) { refreshUser(); }

juce::File PresetBank::userFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("IVO Beatz").getChildFile ("reverseIVO").getChildFile ("Presets");
}

void PresetBank::refreshUser()
{
    list = factory;
    auto dir = userFolder();
    if (! dir.isDirectory()) return;
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    files.sort();
    for (auto& f : files)
        if (auto xml = juce::XmlDocument::parse (f))
        {
            Preset p; p.category = "USER"; p.name = f.getFileNameWithoutExtension(); p.user = true; p.file = f;
            for (auto* e : xml->getChildWithTagNameIterator ("P"))
                p.values.push_back ({ e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("v") });
            list.push_back (std::move (p));
        }
}

int PresetBank::indexOf (const juce::String& name) const
{
    for (int i = 0; i < (int) list.size(); ++i) if (list[(size_t) i].name == name) return i;
    return -1;
}

void PresetBank::resetToDefaults (juce::AudioProcessorValueTreeState& s)
{
    for (auto* p : s.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (isPresetParam (r->getParameterID())) r->setValueNotifyingHost (r->getDefaultValue());
}

void PresetBank::apply (juce::AudioProcessorValueTreeState& s, int index) const
{
    if (index < 0 || index >= (int) list.size()) return;
    resetToDefaults (s);
    for (auto& [pidName, v] : list[(size_t) index].values)
        if (auto* r = s.getParameter (pidName))
            if (isPresetParam (pidName)) r->setValueNotifyingHost (r->convertTo0to1 (v));
}

bool PresetBank::saveUser (const juce::AudioProcessorValueTreeState& s, const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    auto dir = userFolder(); dir.createDirectory();
    juce::XmlElement root ("reverseIVOPreset");
    for (auto* p : s.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (isPresetParam (r->getParameterID()))
            {
                auto* e = root.createNewChildElement ("P");
                e->setAttribute ("id", r->getParameterID());
                e->setAttribute ("v", (double) r->convertFrom0to1 (r->getValue()));
            }
    const bool ok = root.writeTo (dir.getChildFile (clean + ".xml"));
    refreshUser();
    return ok;
}
} // namespace reverseivo

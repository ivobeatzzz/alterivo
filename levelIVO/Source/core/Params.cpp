#include "Params.h"

namespace levelivo
{
using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using NR = juce::NormalisableRange<float>;
using Attr = juce::AudioParameterFloatAttributes;

static juce::ParameterID pid (const char* s) { return { s, 1 }; }
static Attr pct()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
                 .withValueFromStringFunction ([] (const juce::String& t) { return t.retainCharacters ("0123456789.-").getFloatValue() / 100.0f; });
}
static Attr db (int dec = 1)
{
    return Attr().withStringFromValueFunction ([dec] (float v, int) { return (v > 0.04f ? "+" : "") + juce::String (v, dec) + " dB"; })
                 .withValueFromStringFunction ([] (const juce::String& t) { return t.retainCharacters ("0123456789.-").getFloatValue(); });
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<APC> (pid (id::mode), "Mode", juce::StringArray { "Natural", "Balanced", "Controlled", "Manual" }, 1));
    p.push_back (std::make_unique<APC> (pid (id::engine), "Engine", juce::StringArray { "Offline", "Realtime" }, 0));
    p.push_back (std::make_unique<APB> (pid (id::autoApply), "Auto Apply", false));
    p.push_back (std::make_unique<APC> (pid (id::listen), "Listen", juce::StringArray { "Original", "Leveling", "Cleanup", "Final" }, 3));
    p.push_back (std::make_unique<APB> (pid (id::matchLoud), "Loudness Match", true));
    p.push_back (std::make_unique<APB> (pid (id::targetOn), "Target On", false));
    p.push_back (std::make_unique<APF> (pid (id::target), "Target Level", NR (-30.0f, -8.0f, 0.1f), -18.0f,
                                        Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " LUFS"; })));
    p.push_back (std::make_unique<APF> (pid (id::output), "Output", NR (-24.0f, 12.0f, 0.1f), 0.0f, db()));
    p.push_back (std::make_unique<APB> (pid (id::bypass), "Bypass", false));
    p.push_back (std::make_unique<APF> (pid (id::rideGain), "Ride Gain", NR (-24.0f, 24.0f, 0.01f), 0.0f, db (2)));

    p.push_back (std::make_unique<APF> (pid (id::phrase), "Phrase", NR (0.0f, 1.0f, 0.01f), 0.9f, pct()));
    p.push_back (std::make_unique<APF> (pid (id::word), "Word", NR (0.0f, 1.0f, 0.01f), 0.8f, pct()));
    p.push_back (std::make_unique<APF> (pid (id::syllable), "Syllable", NR (0.0f, 1.0f, 0.01f), 0.3f, pct()));
    p.push_back (std::make_unique<APF> (pid (id::intensity), "Intensity", NR (0.0f, 1.5f, 0.01f), 1.0f, pct()));
    p.push_back (std::make_unique<APF> (pid (id::sensitivity), "Sensitivity", NR (0.0f, 1.0f, 0.01f), 0.4f, pct()));
    p.push_back (std::make_unique<APF> (pid (id::maxGain), "Max Gain", NR (0.0f, 18.0f, 0.1f), 9.0f, db()));
    p.push_back (std::make_unique<APF> (pid (id::maxAtt), "Max Attenuation", NR (0.0f, 18.0f, 0.1f), 9.0f, db()));
    p.push_back (std::make_unique<APF> (pid (id::smoothing), "Smoothing", NR (5.0f, 100.0f, 1.0f), 30.0f,
                                        Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; })));
    p.push_back (std::make_unique<APB> (pid (id::peakProtect), "Peak Protection", true));
    p.push_back (std::make_unique<APB> (pid (id::consonants), "Consonant Protection", true));
    p.push_back (std::make_unique<APF> (pid (id::beatFollow), "Follow Beat", NR (0.0f, 1.0f, 0.01f), 0.0f, pct()));
    p.push_back (std::make_unique<APF> (pid (id::rtSpeed), "Realtime Speed", NR (0.0f, 1.0f, 0.01f), 0.5f, pct()));

    p.push_back (std::make_unique<APB> (pid (id::noiseOn), "Background Noise On", true));
    p.push_back (std::make_unique<APF> (pid (id::noise), "Background Noise", NR (0.0f, 30.0f, 0.1f), 9.0f, db()));
    p.push_back (std::make_unique<APB> (pid (id::bleedOn), "Headphone Bleed On", true));
    p.push_back (std::make_unique<APF> (pid (id::bleed), "Headphone Bleed", NR (0.0f, 1.0f, 0.01f), 0.5f, pct()));
    p.push_back (std::make_unique<APB> (pid (id::otherOn), "Other Voices On", false));
    p.push_back (std::make_unique<APF> (pid (id::other), "Other Voices", NR (0.0f, 30.0f, 0.1f), 12.0f, db()));
    p.push_back (std::make_unique<APB> (pid (id::hitsOn), "Microphone Hits On", true));
    p.push_back (std::make_unique<APF> (pid (id::hits), "Microphone Hits", NR (0.0f, 1.0f, 0.01f), 1.0f, pct()));
    p.push_back (std::make_unique<APB> (pid (id::mouthOn), "Mouth Noise On", true));
    p.push_back (std::make_unique<APF> (pid (id::mouth), "Mouth Noise", NR (0.0f, 1.0f, 0.01f), 0.5f, pct()));
    p.push_back (std::make_unique<APB> (pid (id::breathOn), "Breath Control On", true));
    p.push_back (std::make_unique<APF> (pid (id::breath), "Breath Control", NR (0.0f, 24.0f, 0.1f), 6.0f, db()));
    p.push_back (std::make_unique<APB> (pid (id::clipRepair), "Clipping Repair", true));
    return { p.begin(), p.end() };
}

static float v (const juce::AudioProcessorValueTreeState& s, const char* i) { return s.getRawParameterValue (i)->load(); }
static bool b (const juce::AudioProcessorValueTreeState& s, const char* i) { return v (s, i) > 0.5f; }

LevelParams readLevelParams (const juce::AudioProcessorValueTreeState& s)
{
    LevelParams p;
    p.phrase = v (s, id::phrase); p.word = v (s, id::word); p.syllable = v (s, id::syllable);
    p.intensity = v (s, id::intensity); p.sensitivity = v (s, id::sensitivity);
    p.maxGain = v (s, id::maxGain); p.maxAtt = v (s, id::maxAtt); p.smoothingMs = v (s, id::smoothing);
    p.peakProtect = b (s, id::peakProtect); p.consonantProtect = b (s, id::consonants);
    p.targetOn = b (s, id::targetOn); p.targetLufs = v (s, id::target);
    p.beatFollow = v (s, id::beatFollow);
    return p;
}
CleanParams readCleanParams (const juce::AudioProcessorValueTreeState& s)
{
    CleanParams c;
    c.noiseOn = b (s, id::noiseOn); c.noiseDb = v (s, id::noise);
    c.bleedOn = b (s, id::bleedOn); c.bleedAmt = v (s, id::bleed);
    c.otherOn = b (s, id::otherOn); c.otherDb = v (s, id::other);
    c.hitsOn = b (s, id::hitsOn); c.hitsAmt = v (s, id::hits);
    c.mouthOn = b (s, id::mouthOn); c.mouthAmt = v (s, id::mouth);
    c.breathOn = b (s, id::breathOn); c.breathDb = v (s, id::breath);
    c.clipRepair = b (s, id::clipRepair);
    return c;
}
ModeValues modeValues (int m)
{
    switch (m)
    {
        case 0:  return { 0.7f, 0.5f, 0.0f, 0.15f, 6.0f, 6.0f };
        case 2:  return { 1.0f, 1.0f, 0.5f, 0.8f, 12.0f, 12.0f };
        default: return { 0.9f, 0.8f, 0.3f, 0.4f, 9.0f, 9.0f };
    }
}
bool isLevelParam (const juce::String& p)
{
    for (auto* s : { id::phrase, id::word, id::syllable, id::intensity, id::sensitivity, id::maxGain, id::maxAtt, id::smoothing,
                     id::peakProtect, id::consonants, id::targetOn, id::target, id::beatFollow })
        if (p == s) return true;
    return false;
}
bool isCleanParam (const juce::String& p)
{
    for (auto* s : { id::noiseOn, id::noise, id::bleedOn, id::bleed, id::otherOn, id::other, id::hitsOn, id::hits,
                     id::mouthOn, id::mouth, id::breathOn, id::breath, id::clipRepair })
        if (p == s) return true;
    return false;
}
} // namespace levelivo

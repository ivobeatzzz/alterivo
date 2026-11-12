#include "Params.h"
#include "../dsp/DspCore.h"

namespace chainivo::pid
{
namespace
{
using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using NR = juce::NormalisableRange<float>;

NR logRange (float lo, float hi) { NR r (lo, hi); r.setSkewForCentre (std::sqrt (lo * hi)); return r; }
juce::AudioParameterFloatAttributes dbAttr()
{
    return juce::AudioParameterFloatAttributes().withLabel ("dB")
        .withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; });
}
juce::AudioParameterFloatAttributes pctAttr()
{
    return juce::AudioParameterFloatAttributes().withLabel ("%")
        .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; });
}
juce::AudioParameterFloatAttributes hzAttr()
{
    return juce::AudioParameterFloatAttributes().withLabel ("Hz")
        .withStringFromValueFunction ([] (float v, int) { return v >= 1000 ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; });
}
juce::AudioParameterFloatAttributes msAttr()
{
    return juce::AudioParameterFloatAttributes().withLabel ("ms")
        .withStringFromValueFunction ([] (float v, int) { return v < 10 ? juce::String (v, 2) + " ms" : juce::String (juce::roundToInt (v)) + " ms"; });
}
juce::AudioParameterFloatAttributes qAttr()
{
    return juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return "Q " + juce::String (v, 2); });
}
template <typename P, typename... A> void add (juce::AudioProcessorValueTreeState::ParameterLayout& l, const char* id, A&&... args)
{
    l.add (std::make_unique<P> (juce::ParameterID { id, 1 }, std::forward<A> (args)...));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    // Globales
    add<APF> (l, input, "Input", NR (-24.0f, 24.0f, 0.1f), 0.0f, dbAttr());
    add<APF> (l, output, "Output", NR (-24.0f, 24.0f, 0.1f), 0.0f, dbAttr());
    add<APF> (l, mix, "Dry/Wet", NR (0.0f, 100.0f, 0.1f), 100.0f, pctAttr());
    add<APF> (l, intensity, "Intensity", NR (0.0f, 100.0f, 0.1f), 70.0f, pctAttr());
    add<APB> (l, gainMatch, "Gain Match", true);
    add<APB> (l, clipSafe, "Clip Safe", true);
    add<APB> (l, bypass, "Bypass", false);
    // EQ
    add<APB> (l, eqOn, "EQ On", true);
    add<APF> (l, hpf, "EQ High-pass", logRange (20.0f, 500.0f), 90.0f, hzAttr());
    add<APC> (l, hpfSlope, "EQ High-pass Slope", juce::StringArray { "12 dB/oct", "24 dB/oct" }, 1);
    add<APF> (l, lowFreq, "EQ Low Freq", logRange (60.0f, 500.0f), 220.0f, hzAttr());
    add<APF> (l, lowGain, "EQ Low Gain", NR (-12.0f, 12.0f, 0.1f), 0.0f, dbAttr());
    add<APF> (l, midFreq, "EQ Mid Freq", logRange (200.0f, 5000.0f), 1000.0f, hzAttr());
    add<APF> (l, midGain, "EQ Mid Gain", NR (-12.0f, 12.0f, 0.1f), 0.0f, dbAttr());
    add<APF> (l, midQ, "EQ Mid Q", logRange (0.3f, 8.0f), 1.2f, qAttr());
    add<APF> (l, presFreq, "EQ Presence Freq", logRange (1000.0f, 9000.0f), 3200.0f, hzAttr());
    add<APF> (l, presGain, "EQ Presence Gain", NR (-12.0f, 12.0f, 0.1f), 0.0f, dbAttr());
    add<APF> (l, presQ, "EQ Presence Q", logRange (0.3f, 8.0f), 1.2f, qAttr());
    add<APF> (l, presDyn, "EQ Presence Dynamic", NR (0.0f, 100.0f, 0.1f), 0.0f, pctAttr());
    add<APF> (l, airFreq, "EQ Air Freq", logRange (5000.0f, 18000.0f), 10000.0f, hzAttr());
    add<APF> (l, airGain, "EQ Air Gain", NR (-12.0f, 12.0f, 0.1f), 0.0f, dbAttr());
    // COMP
    add<APB> (l, compOn, "Comp On", true);
    add<APC> (l, compType, "Comp Type", juce::StringArray { "FET", "Opto", "VCA", "Vari-Mu", "Clean" }, 2);
    add<APF> (l, compThresh, "Comp Threshold", NR (-60.0f, 0.0f, 0.1f), -18.0f, dbAttr());
    add<APF> (l, compRatio, "Comp Ratio", logRange (1.0f, 20.0f), 3.0f,
              juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + ":1"; }));
    add<APF> (l, compAttack, "Comp Attack", logRange (0.1f, 100.0f), 5.0f, msAttr());
    add<APF> (l, compRelease, "Comp Release", logRange (10.0f, 1000.0f), 120.0f, msAttr());
    add<APF> (l, compMakeup, "Comp Makeup", NR (-12.0f, 24.0f, 0.1f), 0.0f, dbAttr());
    add<APF> (l, compMix, "Comp Mix", NR (0.0f, 100.0f, 0.1f), 100.0f, pctAttr());
    // HARMONICS
    add<APB> (l, harmOn, "Harmonics On", false);
    add<APC> (l, harmType, "Harmonics Type", juce::StringArray { "Tube", "Tape", "Transformer", "Exciter" }, 0);
    add<APF> (l, harmDrive, "Harmonics Drive", NR (0.0f, 100.0f, 0.1f), 25.0f, pctAttr());
    add<APF> (l, harmTone, "Harmonics Tone", NR (-100.0f, 100.0f, 0.1f), 0.0f, pctAttr());
    add<APF> (l, harmMix, "Harmonics Mix", NR (0.0f, 100.0f, 0.1f), 100.0f, pctAttr());
    // VOCAL
    add<APB> (l, vocOn, "Vocal On", true);
    add<APF> (l, deess, "De-ess", NR (0.0f, 100.0f, 0.1f), 30.0f, pctAttr());
    add<APF> (l, deessFreq, "De-ess Freq", logRange (3000.0f, 12000.0f), 6500.0f, hzAttr());
    add<APF> (l, harsh, "Harshness", NR (0.0f, 100.0f, 0.1f), 0.0f, pctAttr());
    add<APF> (l, breath, "Breath", NR (0.0f, 100.0f, 0.1f), 0.0f, pctAttr());
    // SPACE
    add<APB> (l, spaceOn, "Space On", true);
    add<APC> (l, revType, "Reverb Type", juce::StringArray { "Plate", "Hall", "Room", "Chamber", "Ambience" }, 0);
    add<APF> (l, revMix, "Reverb Mix", NR (0.0f, 100.0f, 0.1f), 15.0f, pctAttr());
    add<APF> (l, revDecay, "Reverb Decay", logRange (0.2f, 8.0f), 1.6f,
              juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " s"; }));
    add<APF> (l, revPredelay, "Reverb Pre-delay", NR (0.0f, 200.0f, 0.1f), 20.0f, msAttr());
    add<APF> (l, revTone, "Reverb Tone", NR (0.0f, 100.0f, 0.1f), 60.0f, pctAttr());
    add<APC> (l, dlyType, "Delay Type", juce::StringArray { "Digital", "Analog", "Tape", "Ping-Pong" }, 0);
    add<APC> (l, dlyNote, "Delay Time", chainivo::noteNames(), 5);
    add<APF> (l, dlyFeedback, "Delay Feedback", NR (0.0f, 95.0f, 0.1f), 30.0f, pctAttr());
    add<APF> (l, dlyMix, "Delay Mix", NR (0.0f, 100.0f, 0.1f), 0.0f, pctAttr());
    add<APF> (l, duck, "Ducking", NR (0.0f, 100.0f, 0.1f), 50.0f, pctAttr());
    add<APF> (l, width, "Width", NR (0.0f, 200.0f, 0.1f), 100.0f, pctAttr());
    add<APF> (l, doubler, "Doubler", NR (0.0f, 100.0f, 0.1f), 0.0f, pctAttr());
    // CREATIVE
    add<APB> (l, creaOn, "Creative On", false);
    add<APC> (l, creaType, "Creative Type", juce::StringArray { "Stutter", "Gate", "Reverse" }, 0);
    add<APC> (l, creaNote, "Creative Rate", chainivo::noteNames(), 4);
    add<APF> (l, creaAmount, "Creative Amount", NR (0.0f, 100.0f, 0.1f), 50.0f, pctAttr());
    return l;
}

const juce::StringArray& engineParamIds()
{
    static const juce::StringArray ids {
        eqOn, hpf, hpfSlope, lowFreq, lowGain, midFreq, midGain, midQ, presFreq, presGain, presQ, presDyn, airFreq, airGain,
        compOn, compType, compThresh, compRatio, compAttack, compRelease, compMakeup, compMix,
        harmOn, harmType, harmDrive, harmTone, harmMix,
        vocOn, deess, deessFreq, harsh, breath,
        spaceOn, revType, revMix, revDecay, revPredelay, revTone, dlyType, dlyNote, dlyFeedback, dlyMix, duck, width, doubler,
        creaOn, creaType, creaNote, creaAmount };
    return ids;
}

int engineOf (const juce::String& id)
{
    const int i = engineParamIds().indexOf (id);
    if (i < 0) return -1;
    if (i < 14) return 0;
    if (i < 22) return 1;
    if (i < 27) return 2;
    if (i < 32) return 3;
    if (i < 45) return 4;
    return 5;
}
} // namespace chainivo::pid

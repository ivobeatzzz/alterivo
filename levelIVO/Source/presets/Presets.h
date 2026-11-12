// levelIVO — presets de fábrica (situaciones de trabajo) y de usuario (archivos .levelivo en la carpeta del usuario)
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <map>

namespace levelivo
{
struct Preset
{
    juce::String name, description;
    std::map<juce::String, float> values;   // id de parámetro -> valor real
    bool factory = true;
};

class PresetBank
{
public:
    PresetBank();
    const std::vector<Preset>& all() const { return list; }
    void apply (const Preset&, juce::AudioProcessorValueTreeState&) const;
    bool saveUser (const juce::String& name, const juce::AudioProcessorValueTreeState&);
    void reloadUser();
    static juce::File userFolder();
private:
    std::vector<Preset> factoryList, list;
};
} // namespace levelivo

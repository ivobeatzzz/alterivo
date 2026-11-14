// reverseIVO — PRESETS de fábrica (los del documento maestro) y del usuario.
// Un preset recupera todos los parámetros de generación y de efectos; los de reproducción (OUTPUT, THROUGH,
// DUCKING) se quedan como estén, porque dependen de cómo uses el plugin en tu sesión.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace reverseivo
{
struct Preset
{
    juce::String category, name;
    std::vector<std::pair<juce::String, float>> values;   // valores reales (no normalizados)
    bool user = false;
    juce::File file;
};

class PresetBank
{
public:
    PresetBank();
    void refreshUser();
    const std::vector<Preset>& all() const { return list; }
    int indexOf (const juce::String& name) const;
    // pone por defecto lo que el preset no menciona y aplica sus valores
    void apply (juce::AudioProcessorValueTreeState&, int index) const;
    static void resetToDefaults (juce::AudioProcessorValueTreeState&);
    bool saveUser (const juce::AudioProcessorValueTreeState&, const juce::String& name);
    static juce::File userFolder();
private:
    std::vector<Preset> factory, list;
};
} // namespace reverseivo

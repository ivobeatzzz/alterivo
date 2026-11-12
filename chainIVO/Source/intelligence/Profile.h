// PERFIL PERSONAL — chainIVO aprende de lo que el usuario cambia después de AUTO (opcional, transparente,
// reiniciable y desactivable). Sólo guarda desviaciones medias por tipo de voz en su ordenador; nunca
// modifica los presets IVO. Se aplica sumando esas desviaciones a la siguiente propuesta de AUTO.
#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include "../core/Params.h"

namespace chainivo
{
class UserProfile
{
public:
    static const juce::StringArray& learnable()
    {
        static const juce::StringArray ids { pid::presGain, pid::airGain, pid::lowGain, pid::revMix, pid::dlyMix, pid::compRatio, pid::deess, pid::harmDrive, pid::width };
        return ids;
    }
    static float threshold (const juce::String& id)
    {
        if (id == pid::compRatio) return 0.4f;
        if (id.endsWith ("Gain")) return 0.7f;   // dB
        return 4.0f;                            // %
    }
    bool enabled() const { auto f = file(); return f->getBoolValue ("profileOn", false); }   // desactivado hasta que el usuario lo encienda
    void setEnabled (bool on) { auto f = file(); f->setValue ("profileOn", on); f->saveIfNeeded(); }
    float offset (int type, const juce::String& id) const { auto f = file(); return (float) f->getDoubleValue (key (type, id), 0.0); }
    void learn (int type, const juce::String& id, float diff)
    {
        auto f = file();
        const float old = (float) f->getDoubleValue (key (type, id), 0.0);
        f->setValue (key (type, id), old + 0.35f * (diff - old));   // media móvil: un cambio aislado pesa poco
        f->saveIfNeeded();
    }
    void reset()
    {
        auto f = file();
        for (auto& k : f->getAllProperties().getAllKeys()) if (k.startsWith ("profile.")) f->removeValue (k);
        f->saveIfNeeded();
    }
    juce::String describe (const juce::StringArray& typeNames) const
    {
        auto f = file(); juce::String out;
        for (int t = 0; t < typeNames.size(); ++t)
        {
            juce::String line;
            for (auto& id : learnable())
            {
                const float v = (float) f->getDoubleValue (key (t, id), 0.0);
                if (std::abs (v) >= threshold (id) * 0.5f) line << "  " << id << " " << (v > 0 ? "+" : "") << juce::String (v, 1);
            }
            if (line.isNotEmpty()) out << typeNames[t] << ":" << line << "\n";
        }
        return out.isEmpty() ? juce::String::fromUTF8 ("Todav\xc3\xad" "a no ha aprendido nada.") : out;
    }
private:
    static juce::String key (int type, const juce::String& id) { return "profile." + juce::String (type) + "." + id; }
    static std::unique_ptr<juce::PropertiesFile> file()
    {
        juce::PropertiesFile::Options o; o.applicationName = "chainIVO"; o.folderName = "IVO Beatz"; o.filenameSuffix = ".settings"; o.osxLibrarySubFolder = "Application Support";
        return std::make_unique<juce::PropertiesFile> (o);
    }
};
} // namespace chainivo

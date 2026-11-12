#include "Presets.h"
#include "../core/Params.h"

namespace levelivo
{
namespace
{
using V = std::map<juce::String, float>;
Preset make (const char* name, const char* desc, V v)
{
    Preset p; p.name = juce::String::fromUTF8 (name); p.description = juce::String::fromUTF8 (desc); p.values = std::move (v); return p;
}
V modeV (int m)
{
    const auto mv = modeValues (m);
    return { { id::mode, (float) m }, { id::phrase, mv.phrase }, { id::word, mv.word }, { id::syllable, mv.syllable },
             { id::sensitivity, mv.sensitivity }, { id::maxGain, mv.maxGain }, { id::maxAtt, mv.maxAtt } };
}
V merge (V a, const V& b) { for (auto& kv : b) a[kv.first] = kv.second; return a; }
// valores comunes que todo preset fija (para que cargar uno deje siempre el mismo sonido)
V base()
{
    return { { id::intensity, 1.0f }, { id::smoothing, 30.0f }, { id::peakProtect, 1.0f }, { id::consonants, 1.0f }, { id::beatFollow, 0.0f },
             { id::noiseOn, 1.0f }, { id::noise, 9.0f }, { id::bleedOn, 1.0f }, { id::bleed, 0.5f }, { id::otherOn, 0.0f }, { id::other, 12.0f },
             { id::hitsOn, 1.0f }, { id::hits, 1.0f }, { id::mouthOn, 1.0f }, { id::mouth, 0.5f }, { id::breathOn, 1.0f }, { id::breath, 6.0f },
             { id::clipRepair, 1.0f } };
}
V manual (float ph, float wd, float sy, float se, float mg, float ma, float sm)
{
    return { { id::mode, 3.0f }, { id::phrase, ph }, { id::word, wd }, { id::syllable, sy }, { id::sensitivity, se },
             { id::maxGain, mg }, { id::maxAtt, ma }, { id::smoothing, sm } };
}
} // namespace

PresetBank::PresetBank()
{
    factoryList.push_back (make ("Natural", "Toca poco: sólo errores claros. La expresión queda intacta.",
                                 merge (merge (base(), modeV (0)), { { id::noise, 6.0f }, { id::breath, 4.0f }, { id::mouth, 0.4f } })));
    factoryList.push_back (make ("Balanced", "Por defecto: consistencia sin perder naturalidad.", merge (base(), modeV (1))));
    factoryList.push_back (make ("Controlled", "Más igualado, para voces que tienen que estar siempre delante.",
                                 merge (merge (base(), modeV (2)), { { id::breath, 8.0f } })));
    factoryList.push_back (make ("Rap", "Flow denso: cada palabra se entiende, ataques intactos.",
                                 merge (merge (base(), manual (1.0f, 0.9f, 0.4f, 0.6f, 10.0f, 10.0f, 20.0f)), { { id::breath, 8.0f }, { id::mouth, 0.6f } })));
    factoryList.push_back (make ("Trap", "Melódica con Auto-Tune: frases parejas, finales que no se caen.",
                                 merge (base(), manual (0.9f, 0.75f, 0.25f, 0.45f, 9.0f, 9.0f, 35.0f))));
    factoryList.push_back (make ("Reggaeton", "Voz pegada al beat: sigue un poco la energía de la base.",
                                 merge (merge (base(), manual (0.95f, 0.85f, 0.35f, 0.5f, 9.0f, 9.0f, 25.0f)), { { id::beatFollow, 0.3f } })));
    factoryList.push_back (make ("Ad-Libs", "Respeta la variación (gritos, susurros) y sólo arregla lo que se pierde.",
                                 merge (merge (base(), manual (0.6f, 0.4f, 0.0f, 0.2f, 6.0f, 9.0f, 30.0f)), { { id::breath, 10.0f } })));
    factoryList.push_back (make ("Background", "Coros y dobles: muy parejos para que empasten detrás de la principal.",
                                 merge (merge (base(), manual (1.0f, 1.0f, 0.3f, 0.8f, 12.0f, 12.0f, 40.0f)), { { id::breath, 12.0f } })));
    factoryList.push_back (make ("Clean", "Grabación de estudio limpia: limpieza mínima.",
                                 merge (merge (base(), modeV (1)), { { id::noise, 4.0f }, { id::bleedOn, 0.0f }, { id::mouth, 0.3f }, { id::hits, 0.7f } })));
    factoryList.push_back (make ("Noisy", "Grabación en casa: más ruido de fondo, sangrado y otras voces.",
                                 merge (merge (base(), modeV (1)), { { id::noise, 18.0f }, { id::bleed, 0.8f }, { id::mouth, 0.7f },
                                                                    { id::breath, 10.0f }, { id::otherOn, 1.0f }, { id::other, 12.0f } })));
    reloadUser();
}

juce::File PresetBank::userFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("IVO Beatz").getChildFile ("levelIVO").getChildFile ("Presets");
}

void PresetBank::reloadUser()
{
    list = factoryList;
    for (const auto& f : userFolder().findChildFiles (juce::File::findFiles, false, "*.levelivo"))
    {
        auto xml = juce::XmlDocument::parse (f);
        if (xml == nullptr || ! xml->hasTagName ("LEVELIVO_PRESET")) continue;
        Preset p; p.factory = false; p.name = f.getFileNameWithoutExtension();
        for (auto* e : xml->getChildIterator()) p.values[e->getStringAttribute ("id")] = (float) e->getDoubleAttribute ("v");
        list.push_back (p);
    }
}

void PresetBank::apply (const Preset& p, juce::AudioProcessorValueTreeState& s) const
{
    for (const auto& kv : p.values)
        if (auto* prm = s.getParameter (kv.first))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (kv.second));
            prm->endChangeGesture();
        }
}

bool PresetBank::saveUser (const juce::String& name, const juce::AudioProcessorValueTreeState& s)
{
    auto dir = userFolder();
    if (! dir.createDirectory()) return false;
    juce::XmlElement x ("LEVELIVO_PRESET");
    for (auto* prm : s.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            const auto pidStr = r->getParameterID();
            if (pidStr == id::bypass || pidStr == id::rideGain || pidStr == id::listen || pidStr == id::engine || pidStr == id::autoApply) continue;
            auto* e = x.createNewChildElement ("P");
            e->setAttribute ("id", pidStr);
            e->setAttribute ("v", r->convertFrom0to1 (r->getValue()));
        }
    const bool ok = x.writeTo (dir.getChildFile (juce::File::createLegalFileName (name) + ".levelivo"));
    reloadUser();
    return ok;
}
} // namespace levelivo

#include "Params.h"
#include "../dsp/DspCore.h"
#include <map>

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
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; });
}
juce::AudioParameterFloatAttributes pctAttr()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; });
}
juce::AudioParameterFloatAttributes hzAttr()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return v >= 1000 ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; });
}
juce::AudioParameterFloatAttributes msAttr()
{
    return juce::AudioParameterFloatAttributes()
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
    add<APC> (l, revType, "Reverb Type", juce::StringArray { "Plate", "Hall", "Room", "Chamber", "Ambience", "IR" }, 0);
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
    l.add (std::make_unique<APF> (juce::ParameterID { dlyThrow, 2 }, "Phrase-end Throw", NR (0.0f, 100.0f, 0.1f), 0.0f, pctAttr()));
    l.add (std::make_unique<APF> (juce::ParameterID { revSwell, 2 }, "Phrase-end Swell", NR (0.0f, 100.0f, 0.1f), 0.0f, pctAttr()));
    // CREATIVE
    add<APB> (l, creaOn, "Creative On", false);
    add<APC> (l, creaType, "Creative Type", juce::StringArray { "Stutter", "Gate", "Reverse", "Pitch", "Granular" }, 0);
    add<APC> (l, creaNote, "Creative Rate", chainivo::noteNames(), 4);
    add<APF> (l, creaAmount, "Creative Amount", NR (0.0f, 100.0f, 0.1f), 50.0f, pctAttr());
    l.add (std::make_unique<APF> (juce::ParameterID { creaPitch, 2 }, "Creative Pitch", NR (-24.0f, 24.0f, 1.0f), -12.0f,
                                  juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v)) + " st"; })));
    l.add (std::make_unique<APB> (juce::ParameterID { wordOn, 2 }, "Word FX On", false));
    l.add (std::make_unique<APC> (juce::ParameterID { wordTarget, 2 }, "Word FX Target",
                                  juce::StringArray { "Todas", "1 de 2", "1 de 4", "Fuertes", juce::String::fromUTF8 ("1\xc2\xaa frase"), "Fin 4c", "Fin 8c" }, 2));
    l.add (std::make_unique<APC> (juce::ParameterID { wordFx, 2 }, "Word FX Effect",
                                  juce::StringArray { "Eco", "Stutter", "Filtro", juce::String::fromUTF8 ("Distorsi\xc3\xb3n"), "Pitch" }, 0));
    l.add (std::make_unique<APF> (juce::ParameterID { wordAmount, 2 }, "Word FX Amount", NR (0.0f, 100.0f, 0.1f), 60.0f, pctAttr()));
    return l;
}

namespace
{
const std::array<juce::StringArray, 6>& perEngine()
{
    static const std::array<juce::StringArray, 6> e {
        juce::StringArray { eqOn, hpf, hpfSlope, lowFreq, lowGain, midFreq, midGain, midQ, presFreq, presGain, presQ, presDyn, airFreq, airGain },
        juce::StringArray { compOn, compType, compThresh, compRatio, compAttack, compRelease, compMakeup, compMix },
        juce::StringArray { harmOn, harmType, harmDrive, harmTone, harmMix },
        juce::StringArray { vocOn, deess, deessFreq, harsh, breath },
        juce::StringArray { spaceOn, revType, revMix, revDecay, revPredelay, revTone, dlyType, dlyNote, dlyFeedback, dlyMix, duck, width, doubler, dlyThrow, revSwell },
        juce::StringArray { creaOn, creaType, creaNote, creaAmount, creaPitch, wordOn, wordTarget, wordFx, wordAmount } };
    return e;
}
} // namespace

const juce::StringArray& engineParamIds()
{
    static const juce::StringArray ids = [] { juce::StringArray a; for (auto& e : perEngine()) a.addArray (e); return a; }();
    return ids;
}

int engineOf (const juce::String& id)
{
    for (int e = 0; e < 6; ++e) if (perEngine()[(size_t) e].contains (id)) return e;
    return -1;
}

juce::String helpFor (const juce::String& id)
{
    static const std::map<juce::String, juce::String> h {
        { "wordOn", juce::String::fromUTF8 ("Efectos por palabra o por secci\xc3\xb3n: s\xc3\xb3lo suenan en las palabras (o compases) que elijas. Nunca se activan solos.") },
        { "wordTarget", juce::String::fromUTF8 ("D\xc3\xb3nde: todas las palabras \xc2\xb7 1 de cada 2 \xc2\xb7 1 de cada 4 \xc2\xb7 las m\xc3\xa1s fuertes \xc2\xb7 la 1\xc2\xaa de cada frase \xc2\xb7 el \xc3\xbaltimo comp\xc3\xa1s de cada 4 o de cada 8 (transiciones entre secciones).") },
        { "wordFx", juce::String::fromUTF8 ("Qu\xc3\xa9: Eco (al tempo de RATE) \xc2\xb7 Stutter (repite el principio de la palabra) \xc2\xb7 Filtro tel\xc3\xa9" "fono \xc2\xb7 Distorsi\xc3\xb3n \xc2\xb7 Pitch (capa con el tono de PITCH).") },
        { "wordAmount", juce::String::fromUTF8 ("Cantidad del efecto en esas palabras.") },
        { "dlyThrow", juce::String::fromUTF8 ("Automatizaci\xc3\xb3n: al acabar cada frase, la \xc3\xbaltima palabra se manda al delay (remate o \"throw\"). 0 = desactivada.") },
        { "revSwell", juce::String::fromUTF8 ("Automatizaci\xc3\xb3n: la reverb crece justo al acabar cada frase y vuelve a bajar cuando entra la voz. 0 = desactivada.") },
        { "creaPitch", juce::String::fromUTF8 ("Pitch creativo: capa con otra afinaci\xc3\xb3n (s\xc3\xb3lo si eliges Pitch; nunca se activa sola).") },
        { "input", juce::String::fromUTF8 ("Nivel que entra en la cadena. Útil si la toma llega muy floja o muy fuerte.") },
        { "output", juce::String::fromUTF8 ("Nivel final, después de todo (y del Gain Match).") },
        { "mix", juce::String::fromUTF8 ("Mezcla entre la voz original y la procesada (alineadas, sin desfase).") },
        { "intensity", juce::String::fromUTF8 ("Cuánto tratamiento: 0 % = la voz tal cual, 100 % = la cadena completa. Afecta a todos los motores.") },
        { "hpf", juce::String::fromUTF8 ("Paso alto: quita graves que sobran en una voz secundaria (ruido, golpes, bola).") },
        { "hpfSlope", juce::String::fromUTF8 ("Pendiente del paso alto: 12 dB/oct más suave, 24 dB/oct más limpio.") },
        { "lowGain", juce::String::fromUTF8 ("Estante de graves: quita o da cuerpo.") },
        { "lowFreq", juce::String::fromUTF8 ("Frecuencia del estante de graves.") },
        { "midGain", juce::String::fromUTF8 ("Campana media: quita nasalidad o \"caja\" (o realza).") },
        { "midFreq", juce::String::fromUTF8 ("Frecuencia de la campana media.") },
        { "midQ", juce::String::fromUTF8 ("Anchura de la campana media (más Q = más estrecha).") },
        { "presGain", juce::String::fromUTF8 ("Presencia: acerca o aleja la voz (2-6 kHz).") },
        { "presFreq", juce::String::fromUTF8 ("Frecuencia de la presencia.") },
        { "presQ", juce::String::fromUTF8 ("Anchura de la presencia.") },
        { "presDyn", juce::String::fromUTF8 ("Cuánto de la presencia es dinámico: con voz principal por sidechain, sólo actúa cuando ella canta.") },
        { "airGain", juce::String::fromUTF8 ("Aire: brillo arriba del todo, sin dureza.") },
        { "airFreq", juce::String::fromUTF8 ("Dónde empieza el aire.") },
        { "compType", juce::String::fromUTF8 ("Tipo de compresor: FET rápido y con mordida · Opto suave y musical · VCA firme · Vari-Mu redondo · Clean transparente.") },
        { "compThresh", juce::String::fromUTF8 ("A partir de qué nivel comprime.") },
        { "compRatio", juce::String::fromUTF8 ("Cuánto comprime lo que pasa del umbral.") },
        { "compAttack", juce::String::fromUTF8 ("Lo rápido que empieza a comprimir (más lento deja pasar el golpe de las consonantes).") },
        { "compRelease", juce::String::fromUTF8 ("Lo rápido que suelta.") },
        { "compMakeup", juce::String::fromUTF8 ("Ganancia para recuperar el nivel comprimido.") },
        { "compMix", juce::String::fromUTF8 ("Compresión en paralelo: mezcla con la señal sin comprimir.") },
        { "harmType", juce::String::fromUTF8 ("Tipo de saturación: Tube (pares, cálida) · Tape (cinta) · Transformer (cuerpo en graves) · Exciter (brillo).") },
        { "harmDrive", juce::String::fromUTF8 ("Cuánto satura.") },
        { "harmTone", juce::String::fromUTF8 ("Inclinación del tono después de saturar (en Exciter: dónde empieza el brillo).") },
        { "harmMix", juce::String::fromUTF8 ("Mezcla de la saturación con la señal limpia (en fase).") },
        { "deess", juce::String::fromUTF8 ("De-esser: cuánto baja las eses. Sólo actúa en las eses, no en las vocales.") },
        { "deessFreq", juce::String::fromUTF8 ("Dónde están las eses de esta voz (AUTO lo mide).") },
        { "harsh", juce::String::fromUTF8 ("Dureza: baja de forma dinámica la zona de 2.5-5 kHz cuando molesta.") },
        { "breath", juce::String::fromUTF8 ("Baja las respiraciones (tramos flojos y ruidosos de más de 80 ms).") },
        { "revType", juce::String::fromUTF8 ("Tipo de reverb: Plate densa y brillante · Hall grande · Room corta · Chamber media · Ambience casi sin cola · IR = convolución con la respuesta de impulso que cargues (BEAT · EXPORTAR → Cargar IR).") },
        { "revMix", juce::String::fromUTF8 ("Cantidad de reverb.") },
        { "revDecay", juce::String::fromUTF8 ("Duración de la cola.") },
        { "revPredelay", juce::String::fromUTF8 ("Separación entre la voz y la reverb (más = la voz queda delante).") },
        { "revTone", juce::String::fromUTF8 ("Brillo de la cola.") },
        { "dlyType", juce::String::fromUTF8 ("Tipo de delay: Digital limpio · Analog oscuro · Tape con vaivén · Ping-Pong de lado a lado.") },
        { "dlyNote", juce::String::fromUTF8 ("Tiempo del eco, sincronizado al tempo del proyecto.") },
        { "dlyFeedback", juce::String::fromUTF8 ("Cuántas repeticiones.") },
        { "dlyMix", juce::String::fromUTF8 ("Cantidad de delay.") },
        { "duck", juce::String::fromUTF8 ("Ducking: la reverb y el delay bajan mientras la voz suena y vuelven en los huecos.") },
        { "width", juce::String::fromUTF8 ("Anchura estéreo (sólo abre los lados: en mono la voz no se pierde).") },
        { "doubler", juce::String::fromUTF8 ("Doblador: dos copias con micro-afinación a los lados.") },
        { "creaType", juce::String::fromUTF8 ("Efecto creativo: Stutter repite · Gate corta · Reverse eco al revés · Pitch capa afinada · Granular nube de granos. Nunca se activa solo.") },
        { "creaNote", juce::String::fromUTF8 ("Tamaño de cada división, al tempo.") },
        { "creaAmount", juce::String::fromUTF8 ("Cantidad del efecto.") },
        { "gainMatch", juce::String::fromUTF8 ("Iguala el volumen al de la voz original para comparar de forma justa.") },
        { "clipSafe", juce::String::fromUTF8 ("Limitador de salida: la voz nunca pasa de -1 dBFS, sin recortar ni distorsionar.") },
        { "bypass", juce::String::fromUTF8 ("Escuchar la voz original (con la misma latencia).") },
    };
    auto it = h.find (id);
    return it != h.end() ? it->second : juce::String();
}
} // namespace chainivo::pid

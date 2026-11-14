#include "Params.h"

namespace reverseivo
{
using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using NR = juce::NormalisableRange<float>;
using Attr = juce::AudioParameterFloatAttributes;

static juce::ParameterID pid (const char* s) { return { s, 1 }; }
static float num (const juce::String& t) { return t.retainCharacters ("0123456789.-").getFloatValue(); }
static Attr pct()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
                 .withValueFromStringFunction ([] (const juce::String& t) { return num (t) / 100.0f; });
}
static Attr db (int dec = 1)
{
    return Attr().withStringFromValueFunction ([dec] (float v, int) { return (v > 0.04f ? "+" : "") + juce::String (v, dec) + " dB"; })
                 .withValueFromStringFunction ([] (const juce::String& t) { return num (t); });
}
static Attr st()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return (v > 0.04f ? "+" : "") + juce::String (v, 1) + " st"; })
                 .withValueFromStringFunction ([] (const juce::String& t) { return num (t); });
}
static Attr hz()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; })
                 .withValueFromStringFunction ([] (const juce::String& t) { const float v = num (t); return t.containsIgnoreCase ("k") ? v * 1000.0f : v; });
}
static Attr ms()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; })
                 .withValueFromStringFunction ([] (const juce::String& t) { return num (t); });
}
static Attr sec (int dec = 2)
{
    return Attr().withStringFromValueFunction ([dec] (float v, int) { return juce::String (v, dec) + " s"; })
                 .withValueFromStringFunction ([] (const juce::String& t) { return num (t); });
}
static NR freqRange() { NR r (20.0f, 20000.0f, 1.0f); r.setSkewForCentre (1000.0f); return r; }

juce::StringArray divisionNames() { return { "1/16", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1 BAR" }; }

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    const RenderSettings d;   // los valores por defecto viven en un solo sitio
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto F = [&] (const char* i, const char* name, NR r, float def, Attr a) { p.push_back (std::make_unique<APF> (pid (i), name, r, def, a)); };
    auto B = [&] (const char* i, const char* name, bool def) { p.push_back (std::make_unique<APB> (pid (i), name, def)); };
    auto C = [&] (const char* i, const char* name, juce::StringArray c, int def) { p.push_back (std::make_unique<APC> (pid (i), name, c, def)); };

    // CREATE
    C (id::duration, "Duration", { "1/4", "1/2", "1 BAR", "2 BARS", "4 BARS", "FREE" }, d.durationIndex);
    { NR r (0.1f, 12.0f, 0.001f); r.setSkewForCentre (2.0f); F (id::freeTime, "Free Time", r, (float) d.freeSeconds, sec (3)); }
    B (id::smartSync, "Smart Sync", d.smartSync);
    C (id::fit, "Fit", { "STRETCH", "NATURAL" }, (int) d.fit);
    F (id::intensity, "Intensity", NR (0.0f, 1.0f, 0.01f), d.intensity, pct());
    F (id::pitch, "Pitch", NR (-24.0f, 24.0f, 0.1f), d.pitch, st());
    F (id::character, "Character", NR (0.0f, 1.0f, 0.01f), d.character, pct());
    C (id::genMode, "Generate Mode", { "AUTO", "MANUAL" }, 0);
    // CUSTOMIZE · volumen
    F (id::volStart, "Start Level", NR (-60.0f, 0.0f, 0.1f), d.volStartDb, db());
    F (id::volEnd, "End Level", NR (-24.0f, 6.0f, 0.1f), d.volEndDb, db());
    F (id::volCurve, "Volume Curve", NR (0.0f, 1.0f, 0.01f), d.volCurve, pct());
    F (id::fadeIn, "Fade In", NR (0.0f, 1.0f, 0.01f), d.fadeIn, pct());
    // pitch
    F (id::pitchStart, "Pitch Start", NR (-24.0f, 24.0f, 0.1f), d.pitchStart, st());
    F (id::pitchEnd, "Pitch End", NR (-24.0f, 24.0f, 0.1f), d.pitchEnd, st());
    F (id::pitchCurve, "Pitch Curve", NR (0.0f, 1.0f, 0.01f), d.pitchCurve, pct());
    F (id::microPitch, "Micro Pitch", NR (0.0f, 1.0f, 0.01f), d.microPitch, pct());
    // filtro
    F (id::hpStart, "HP Start", freqRange(), d.hpStart, hz());
    F (id::hpEnd, "HP End", freqRange(), d.hpEnd, hz());
    F (id::lpStart, "LP Start", freqRange(), d.lpStart, hz());
    F (id::lpEnd, "LP End", freqRange(), d.lpEnd, hz());
    F (id::resonance, "Resonance", NR (0.0f, 1.0f, 0.01f), d.resonance, pct());
    F (id::filterCurve, "Filter Curve", NR (0.0f, 1.0f, 0.01f), d.filterCurve, pct());
    // transitorio y capas
    C (id::ending, "Ending", { "SOFT", "NATURAL", "HARD" }, (int) d.ending);
    F (id::transient, "Transient", NR (0.0f, 1.0f, 0.01f), d.transient, pct());
    B (id::mainOn, "Main Layer", d.mainOn);
    F (id::mainLevel, "Main Level", NR (-24.0f, 6.0f, 0.1f), d.mainDb, db());
    F (id::mainWidth, "Main Width", NR (0.0f, 2.0f, 0.01f), d.mainWidth, pct());
    B (id::texOn, "Texture Layer", d.texOn);
    F (id::texLevel, "Texture Level", NR (-36.0f, 6.0f, 0.1f), d.texDb, db());
    F (id::texPitch, "Texture Pitch", NR (-24.0f, 24.0f, 0.1f), d.texPitch, st());
    F (id::texWidth, "Texture Width", NR (0.0f, 2.0f, 0.01f), d.texWidth, pct());
    F (id::texSmear, "Texture Smear", NR (0.0f, 1.0f, 0.01f), d.texSmear, pct());
    // FX · espacio
    F (id::wash, "Reverse Wash", NR (0.0f, 1.0f, 0.01f), d.wash, pct());
    B (id::reverbOn, "Reverb", d.reverbOn);
    F (id::revSize, "Reverb Size", NR (0.0f, 1.0f, 0.01f), d.revSize, pct());
    { NR r (0.2f, 12.0f, 0.01f); r.setSkewForCentre (2.5f); F (id::revDecay, "Reverb Decay", r, d.revDecay, sec (1)); }
    F (id::revPredelay, "Pre-Delay", NR (0.0f, 200.0f, 1.0f), d.revPredelay, ms());
    F (id::revMix, "Reverb Mix", NR (0.0f, 1.0f, 0.01f), d.revMix, pct());
    F (id::revDamp, "Reverb Damping", NR (0.0f, 1.0f, 0.01f), d.revDamp, pct());
    B (id::delayOn, "Delay", d.delayOn);
    C (id::delayTime, "Delay Time", divisionNames(), d.delayDiv);
    F (id::delayFb, "Delay Feedback", NR (0.0f, 0.92f, 0.01f), d.delayFb, pct());
    F (id::delayMix, "Delay Mix", NR (0.0f, 1.0f, 0.01f), d.delayMix, pct());
    B (id::pingPong, "Ping-Pong", d.pingPong);
    // FX · ritmo
    B (id::tremOn, "Tremolo", d.tremOn);
    C (id::tremRate, "Tremolo Rate", divisionNames(), d.tremDiv);
    F (id::tremDepth, "Tremolo Depth", NR (0.0f, 1.0f, 0.01f), d.tremDepth, pct());
    B (id::cutsOn, "Progressive Cuts", d.cutsOn);
    C (id::cutsFrom, "Cuts From", divisionNames(), d.cutsFrom);
    C (id::cutsTo, "Cuts To", divisionNames(), d.cutsTo);
    F (id::cutsDepth, "Cuts Depth", NR (0.0f, 1.0f, 0.01f), d.cutsDepth, pct());
    B (id::stutterOn, "Stutter", d.stutterOn);
    C (id::stutterRate, "Stutter Rate", divisionNames(), d.stutterDiv);
    F (id::stutterLen, "Stutter Length", NR (0.05f, 0.6f, 0.01f), d.stutterLen, pct());
    // FX · carácter
    F (id::drive, "Drive", NR (0.0f, 1.0f, 0.01f), d.drive, pct());
    F (id::crush, "Bitcrush", NR (0.0f, 1.0f, 0.01f), d.crush, pct());
    F (id::noise, "Noise", NR (0.0f, 1.0f, 0.01f), d.noise, pct());
    F (id::harmonics, "Harmonics", NR (0.0f, 1.0f, 0.01f), d.harmonics, pct());
    F (id::sub, "Sub Boost", NR (-6.0f, 12.0f, 0.1f), d.subDb, db());
    F (id::air, "Air", NR (-6.0f, 12.0f, 0.1f), d.airDb, db());
    // FX · estéreo
    F (id::width, "Width", NR (0.0f, 2.0f, 0.01f), d.width, pct());
    F (id::autoPan, "Auto-Pan", NR (0.0f, 1.0f, 0.01f), d.autoPan, pct());
    C (id::panRate, "Pan Rate", divisionNames(), d.panDiv);
    // FX · movimiento
    F (id::moveReverb, "Move Reverb", NR (0.0f, 1.0f, 0.01f), d.moveReverb, pct());
    F (id::moveDelay, "Move Delay", NR (0.0f, 1.0f, 0.01f), d.moveDelay, pct());
    F (id::moveDrive, "Move Drive", NR (0.0f, 1.0f, 0.01f), d.moveDrive, pct());
    F (id::movePan, "Move Pan", NR (0.0f, 1.0f, 0.01f), d.movePan, pct());
    F (id::moveFilter, "Move Filter", NR (0.0f, 1.0f, 0.01f), d.moveFilter, pct());
    F (id::moveWidth, "Move Width", NR (0.0f, 1.0f, 0.01f), d.moveWidth, pct());
    F (id::moveTrem, "Move Tremolo", NR (0.0f, 1.0f, 0.01f), d.moveTrem, pct());
    // inteligentes
    B (id::smartEnding, "Smart Ending", d.smartEnding);
    F (id::preImpact, "Pre-Impact", NR (0.0f, 1.0f, 0.01f), d.preImpact, pct());
    F (id::tail, "Tail", NR (0.0f, 6.0f, 0.01f), d.tailSeconds, sec (1));
    F (id::outFade, "Output Fade", NR (10.0f, 3000.0f, 1.0f), d.outFadeMs, ms());
    B (id::clipSafe, "Clip Safe", d.clipSafe);
    // reproducción
    F (id::output, "Output", NR (-36.0f, 12.0f, 0.1f), 0.0f, db());
    B (id::through, "Through", true);
    F (id::ducking, "Ducking", NR (0.0f, 1.0f, 0.01f), 0.0f, pct());
    F (id::duckRelease, "Duck Release", NR (20.0f, 1000.0f, 1.0f), 180.0f, ms());
    return { p.begin(), p.end() };
}

RenderSettings readSettings (const juce::AudioProcessorValueTreeState& s)
{
    auto f = [&] (const char* i) { return s.getRawParameterValue (i)->load(); };
    auto b = [&] (const char* i) { return f (i) > 0.5f; };
    auto c = [&] (const char* i) { return (int) std::lround (f (i)); };
    RenderSettings r;
    r.durationIndex = c (id::duration); r.freeSeconds = f (id::freeTime); r.smartSync = b (id::smartSync);
    r.fit = (Fit) c (id::fit); r.intensity = f (id::intensity); r.pitch = f (id::pitch); r.character = f (id::character);
    r.volStartDb = f (id::volStart); r.volEndDb = f (id::volEnd); r.volCurve = f (id::volCurve); r.fadeIn = f (id::fadeIn);
    r.pitchStart = f (id::pitchStart); r.pitchEnd = f (id::pitchEnd); r.pitchCurve = f (id::pitchCurve); r.microPitch = f (id::microPitch);
    r.hpStart = f (id::hpStart); r.hpEnd = f (id::hpEnd); r.lpStart = f (id::lpStart); r.lpEnd = f (id::lpEnd);
    r.resonance = f (id::resonance); r.filterCurve = f (id::filterCurve);
    r.ending = (Ending) c (id::ending); r.transient = f (id::transient);
    r.mainOn = b (id::mainOn); r.mainDb = f (id::mainLevel); r.mainWidth = f (id::mainWidth);
    r.texOn = b (id::texOn); r.texDb = f (id::texLevel); r.texPitch = f (id::texPitch); r.texWidth = f (id::texWidth); r.texSmear = f (id::texSmear);
    r.wash = f (id::wash);
    r.reverbOn = b (id::reverbOn); r.revSize = f (id::revSize); r.revDecay = f (id::revDecay); r.revPredelay = f (id::revPredelay);
    r.revMix = f (id::revMix); r.revDamp = f (id::revDamp);
    r.delayOn = b (id::delayOn); r.delayDiv = c (id::delayTime); r.delayFb = f (id::delayFb); r.delayMix = f (id::delayMix); r.pingPong = b (id::pingPong);
    r.tremOn = b (id::tremOn); r.tremDiv = c (id::tremRate); r.tremDepth = f (id::tremDepth);
    r.cutsOn = b (id::cutsOn); r.cutsFrom = c (id::cutsFrom); r.cutsTo = c (id::cutsTo); r.cutsDepth = f (id::cutsDepth);
    r.stutterOn = b (id::stutterOn); r.stutterDiv = c (id::stutterRate); r.stutterLen = f (id::stutterLen);
    r.drive = f (id::drive); r.crush = f (id::crush); r.noise = f (id::noise); r.harmonics = f (id::harmonics);
    r.subDb = f (id::sub); r.airDb = f (id::air);
    r.width = f (id::width); r.autoPan = f (id::autoPan); r.panDiv = c (id::panRate);
    r.moveReverb = f (id::moveReverb); r.moveDelay = f (id::moveDelay); r.moveDrive = f (id::moveDrive); r.movePan = f (id::movePan);
    r.moveFilter = f (id::moveFilter); r.moveWidth = f (id::moveWidth); r.moveTrem = f (id::moveTrem);
    r.smartEnding = b (id::smartEnding); r.preImpact = f (id::preImpact); r.tailSeconds = f (id::tail); r.outFadeMs = f (id::outFade);
    r.clipSafe = b (id::clipSafe);
    return r;
}

bool isPlaybackParam (const juce::String& p)
{
    return p == id::output || p == id::through || p == id::ducking || p == id::duckRelease || p == id::genMode;
}

juce::String helpFor (const juce::String& p)
{
    static const std::map<juce::String, const char*> h {
        { id::duration, "Duración del reverse. Con Smart Sync se ajusta al tempo y al compás del DAW." },
        { id::freeTime, "Duración en segundos para FREE (y cuando el DAW no da tempo)." },
        { id::smartSync, "Lee el tempo del DAW: el reverse dura justo lo elegido y termina exactamente en el golpe." },
        { id::fit, "STRETCH encaja toda la región en la duración (tono intacto). NATURAL la usa a su velocidad." },
        { id::intensity, "Cuánto lavado de reverb crece hacia el golpe y lo agresiva que es la subida." },
        { id::pitch, "Transporta todo el reverse en semitonos." },
        { id::character, "Personalidad: de limpio a sucio (saturación, armónicos y textura)." },
        { id::genMode, "AUTO genera solo al tocar un control. MANUAL espera a GENERATE. Mismo motor y calidad." },
        { id::volStart, "Nivel al principio del reverse." }, { id::volEnd, "Nivel justo antes del golpe." },
        { id::volCurve, "Forma de la subida: más alto concentra el volumen al final." }, { id::fadeIn, "Entrada suave (porción de la duración)." },
        { id::pitchStart, "Tono al principio (se suma a PITCH)." }, { id::pitchEnd, "Tono al llegar al golpe." },
        { id::pitchCurve, "Cómo se mueve el tono del principio al final." }, { id::microPitch, "Pequeñas variaciones de tono: más orgánico." },
        { id::hpStart, "Pasa-altos al principio." }, { id::hpEnd, "Pasa-altos al final." },
        { id::lpStart, "Pasa-bajos al principio." }, { id::lpEnd, "Pasa-bajos al final." },
        { id::resonance, "Resonancia de los filtros." }, { id::filterCurve, "Forma del barrido del filtro." },
        { id::ending, "Cómo llega al golpe: SOFT redondo, NATURAL, HARD con ataque marcado." },
        { id::transient, "Fuerza del ataque final." },
        { id::texOn, "Capa de textura: la misma voz difuminada, como un pad." }, { id::texPitch, "Tono de la capa de textura." },
        { id::texSmear, "Cuánto se difumina la textura." },
        { id::wash, "Reverb ANTES de invertir: el «swell» clásico de los reverses vocales." },
        { id::revMix, "Reverb después de invertir (con FX MOVEMENT puede crecer hacia el golpe)." },
        { id::delayTime, "Tiempo del delay sincronizado al tempo." },
        { id::cutsOn, "Cortes rítmicos que aceleran hacia el golpe." }, { id::stutterOn, "Repite el último fragmento antes del golpe." },
        { id::drive, "Saturación (procesada a 4x para no generar aliasing)." }, { id::crush, "Reducción de bits." },
        { id::noise, "Ruido que sigue a la voz: risers más cinematográficos." }, { id::harmonics, "Realce armónico: más tensión y brillo." },
        { id::sub, "Refuerzo de graves." }, { id::air, "Presencia y brillo en agudos." },
        { id::width, "Anchura estéreo." }, { id::autoPan, "Paneo automático sincronizado." },
        { id::moveReverb, "La reverb crece a lo largo del reverse." }, { id::moveDelay, "La realimentación del delay crece." },
        { id::moveDrive, "La saturación crece hacia el golpe." }, { id::movePan, "El sonido viaja de izquierda a derecha." },
        { id::moveFilter, "El filtro se abre a lo largo del reverse." }, { id::moveWidth, "La imagen estéreo se abre hacia el golpe." },
        { id::moveTrem, "El tremolo se intensifica hacia el golpe." },
        { id::smartEnding, "Llega al golpe sin clic ni corte brusco." }, { id::preImpact, "Tensión extra justo antes del golpe." },
        { id::tail, "Cola de reverb y delay después del golpe." }, { id::outFade, "Desvanecimiento del final de la cola." },
        { id::clipSafe, "Limitador suave: el reverse nunca satura la salida." },
        { id::output, "Volumen del reverse (en tiempo real, no regenera)." },
        { id::through, "Deja pasar el audio de la pista. Apágalo para imprimir sólo el reverse en otra pista." },
        { id::ducking, "Baja la cola cuando entra la voz después del golpe." }, { id::duckRelease, "Rapidez con la que vuelve la cola." },
    };
    auto it = h.find (p);
    return it != h.end() ? juce::String::fromUTF8 (it->second) : juce::String();
}
} // namespace reverseivo

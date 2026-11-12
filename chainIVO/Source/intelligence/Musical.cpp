#include "Musical.h"
#include "../core/Params.h"

namespace chainivo
{
namespace { inline juce::String T (const char* s) { return juce::String::fromUTF8 (s); } }
const juce::StringArray& voiceTypeNames()
{
    static const juce::StringArray n = juce::StringArray::fromTokens (juce::String (juce::CharPointer_UTF8 (
        "Ad-lib|Coro|Doble|Segunda voz|Armon\xc3\xad" "a|Respuesta|Intro / outro|Chop|Susurro|Grito|Hablada|Textura|Ambiente|Efecto")), "|", "");
    return n;
}
juce::String voiceTypeName (VoiceType t) { return voiceTypeNames()[(int) t]; }

// ----------------------------------------------------------------- Estilos
// Puntos de partida razonados por estética; se validan de oído con voces reales (ver ARQUITECTURA.md).
namespace
{
const GenreProfile kGenres[] = {
    //  nombre                         familia        bright grit  armónico                 reverb             size  rev   delay width press
    { "Crank",                        "Trap",          3.0f, 0.55f, HarmType::transformer, RevType::plate,    0.8f, 0.8f, 0.7f, 0.5f, 3.0f },
    { "Plugg",                        "Trap",          1.5f, 0.15f, HarmType::tape,        RevType::hall,     1.3f, 1.3f, 1.3f, 0.8f, 1.0f },
    { "PluggnB",                      "Trap",          2.0f, 0.15f, HarmType::tape,        RevType::hall,     1.4f, 1.4f, 1.2f, 0.9f, 1.0f },
    { "Rage",                         "Trap",          3.5f, 0.6f,  HarmType::transformer, RevType::plate,    1.1f, 1.0f, 1.0f, 0.7f, 3.0f },
    { "Dark Trap",                    "Trap",         -1.5f, 0.35f, HarmType::tube,        RevType::hall,     1.6f, 1.2f, 1.1f, 0.6f, 2.0f },
    { "Trap experimental",            "Trap",          1.0f, 0.5f,  HarmType::transformer, RevType::chamber,  1.2f, 1.1f, 1.2f, 0.7f, 2.0f },
    { "Trap mel\xc3\xb3" "dico",      "Trap",          2.0f, 0.2f,  HarmType::tape,        RevType::plate,    1.2f, 1.2f, 1.1f, 0.6f, 1.5f },
    { "Trap comercial",               "Trap",          2.5f, 0.2f,  HarmType::exciter,     RevType::plate,    1.0f, 1.0f, 0.9f, 0.5f, 2.0f },
    { "Trap alternativo",             "Trap",          1.0f, 0.4f,  HarmType::tape,        RevType::chamber,  1.1f, 1.0f, 1.0f, 0.5f, 1.5f },
    { "Reggaet\xc3\xb3n old school",  "Reggaet\xc3\xb3n", 1.0f, 0.3f, HarmType::tube,      RevType::room,     0.8f, 0.9f, 1.0f, 0.4f, 2.5f },
    { "Est\xc3\xa9tica La Bendici\xc3\xb3n", "Reggaet\xc3\xb3n", 1.5f, 0.3f, HarmType::tube, RevType::plate, 0.9f, 1.0f, 1.0f, 0.5f, 2.5f },
    { "La Obsesi\xc3\xb3n / Factory", "Reggaet\xc3\xb3n", 2.0f, 0.25f, HarmType::exciter,  RevType::plate,    0.9f, 0.9f, 1.1f, 0.5f, 2.5f },
    { "Reggaet\xc3\xb3n underground", "Reggaet\xc3\xb3n", 0.5f, 0.45f, HarmType::transformer, RevType::room,  0.9f, 1.0f, 1.0f, 0.5f, 2.0f },
    { "Reggaet\xc3\xb3n moderno",     "Reggaet\xc3\xb3n", 2.5f, 0.2f, HarmType::exciter,   RevType::plate,    1.0f, 0.9f, 1.0f, 0.6f, 2.5f },
    { "Reggaet\xc3\xb3n comercial",   "Reggaet\xc3\xb3n", 2.5f, 0.15f, HarmType::exciter,  RevType::plate,    0.9f, 0.8f, 0.8f, 0.5f, 3.0f },
    { "Reggaet\xc3\xb3n + trap",      "Reggaet\xc3\xb3n", 2.0f, 0.35f, HarmType::tape,     RevType::plate,    1.1f, 1.0f, 1.1f, 0.6f, 2.5f },
    { "Rap de los 2000",              "Rap",           0.0f, 0.4f,  HarmType::tape,        RevType::room,     0.7f, 0.7f, 0.8f, 0.3f, 3.0f },
    { "Hip-hop cl\xc3\xa1sico",       "Rap",          -0.5f, 0.45f, HarmType::tape,        RevType::room,     0.7f, 0.7f, 0.7f, 0.3f, 3.0f },
    { "Street rap",                   "Rap",           0.5f, 0.4f,  HarmType::transformer, RevType::ambience, 0.6f, 0.6f, 0.7f, 0.3f, 3.5f },
    { "Rap mel\xc3\xb3" "dico",       "Rap",           1.5f, 0.2f,  HarmType::tape,        RevType::plate,    1.0f, 1.0f, 1.0f, 0.5f, 2.0f },
    { "Hip-hop contempor\xc3\xa1neo", "Rap",           1.5f, 0.3f,  HarmType::tube,        RevType::chamber,  0.9f, 0.9f, 0.9f, 0.4f, 2.5f },
    { "Indie rock",                   "Alternativo",   0.5f, 0.35f, HarmType::tube,        RevType::room,     1.0f, 1.0f, 0.8f, 0.5f, 2.0f },
    { "Alternative rock",             "Alternativo",   1.0f, 0.5f,  HarmType::transformer, RevType::chamber,  1.0f, 0.9f, 0.8f, 0.5f, 3.0f },
    { "Indie experimental",           "Alternativo",   0.0f, 0.45f, HarmType::tape,        RevType::hall,     1.5f, 1.3f, 1.3f, 0.8f, 1.5f },
    { "Rock + electr\xc3\xb3nica + urbano", "Alternativo", 1.5f, 0.5f, HarmType::transformer, RevType::plate, 1.1f, 1.0f, 1.1f, 0.6f, 2.5f },
    { "R&B",                          "Otros",         2.0f, 0.15f, HarmType::tube,        RevType::hall,     1.2f, 1.2f, 1.0f, 0.7f, 1.5f },
    { "Electr\xc3\xb3nica",           "Otros",         2.0f, 0.3f,  HarmType::exciter,     RevType::hall,     1.3f, 1.2f, 1.3f, 0.8f, 2.0f },
    { "Pop",                          "Otros",         2.5f, 0.15f, HarmType::exciter,     RevType::plate,    1.0f, 1.0f, 0.9f, 0.5f, 2.5f },
    { "Comercial",                    "Otros",         2.5f, 0.15f, HarmType::exciter,     RevType::plate,    0.9f, 0.9f, 0.8f, 0.5f, 3.0f },
    { "Experimental",                 "Otros",         0.0f, 0.6f,  HarmType::transformer, RevType::hall,     1.8f, 1.4f, 1.4f, 0.9f, 1.5f },
};
} // namespace

int genreCount() { return (int) (sizeof (kGenres) / sizeof (kGenres[0])); }
const GenreProfile& genre (int i) { return kGenres[juce::jlimit (0, genreCount() - 1, i)]; }
int genreIndex (const juce::String& name)
{
    for (int i = 0; i < genreCount(); ++i) if (name == juce::String (juce::CharPointer_UTF8 (kGenres[i].name))) return i;
    return -1;
}

// --------------------------------------------------- Recetas base por tipo de voz
Recipe defaultRecipe (VoiceType t)
{
    Recipe r; r.type = t; r.name = voiceTypeName (t);
    if (t == VoiceType::adlib) r.throwAmt = 0.3f;
    if (t == VoiceType::respuesta) r.throwAmt = 0.4f;
    if (t == VoiceType::coro || t == VoiceType::armonia) r.swell = 0.2f;
    if (t == VoiceType::introOutro) r.swell = 0.5f; r.intention = T ("Propuesta autom\xc3\xa1tica");
    switch (t)
    {
        case VoiceType::adlib:      r.hpfMul = 1.3f; r.presence = 2.0f; r.air = 2.0f; r.comp = CompType::fet; r.grTarget = 6; r.harmOn = true; r.drive = 0.25f;
                                    r.rev = RevType::plate; r.revMix = 0.2f; r.decay = 1.4f; r.dlyMix = 0.18f; r.dlyNote = 5; r.dlyFb = 0.3f; r.duck = 0.6f; r.width = 1.3f; r.breath = 0.5f; break;
        case VoiceType::coro:       r.hpfMul = 1.6f; r.presence = 0.5f; r.air = 2.5f; r.body = -2.0f; r.comp = CompType::opto; r.grTarget = 4; r.ratio = 3;
                                    r.rev = RevType::hall; r.revMix = 0.25f; r.decay = 2.2f; r.width = 1.6f; r.doubler = 0.25f; r.breath = 0.6f; r.deessScale = 1.3f; break;
        case VoiceType::doble:      r.hpfMul = 1.4f; r.presence = -0.5f; r.air = 1.0f; r.body = -1.5f; r.comp = CompType::vca; r.grTarget = 6; r.ratio = 4;
                                    r.rev = RevType::plate; r.revMix = 0.1f; r.width = 1.4f; r.breath = 0.7f; r.deessScale = 1.4f; break;
        case VoiceType::segunda:    r.hpfMul = 1.4f; r.presence = 0.0f; r.air = 1.5f; r.body = -1.0f; r.comp = CompType::opto; r.grTarget = 5;
                                    r.rev = RevType::plate; r.revMix = 0.16f; r.width = 1.3f; r.breath = 0.6f; break;
        case VoiceType::armonia:    r.hpfMul = 1.5f; r.presence = 0.0f; r.air = 2.5f; r.body = -1.5f; r.comp = CompType::opto; r.grTarget = 4;
                                    r.rev = RevType::hall; r.revMix = 0.22f; r.decay = 2.0f; r.width = 1.5f; r.doubler = 0.15f; r.breath = 0.6f; r.deessScale = 1.2f; break;
        case VoiceType::respuesta:  r.hpfMul = 1.2f; r.presence = 1.5f; r.air = 1.5f; r.comp = CompType::vca; r.grTarget = 5;
                                    r.rev = RevType::plate; r.revMix = 0.16f; r.dlyMix = 0.2f; r.dlyNote = 4; r.duck = 0.4f; r.width = 1.2f; break;
        case VoiceType::introOutro: r.hpfMul = 1.0f; r.presence = 1.0f; r.air = 2.0f; r.comp = CompType::opto; r.grTarget = 4;
                                    r.rev = RevType::hall; r.revMix = 0.3f; r.decay = 2.8f; r.dlyMix = 0.2f; r.dlyNote = 7; r.dlyFb = 0.4f; r.duck = 0.3f; r.width = 1.4f; break;
        case VoiceType::chop:       r.hpfMul = 1.2f; r.presence = 1.0f; r.air = 1.5f; r.comp = CompType::fet; r.grTarget = 5; r.attack = 2;
                                    r.rev = RevType::room; r.revMix = 0.12f; r.decay = 0.8f; r.harmOn = true; r.drive = 0.3f; r.breath = 0.3f; break;
        case VoiceType::susurro:    r.hpfMul = 1.0f; r.presence = 1.0f; r.air = 1.0f; r.comp = CompType::opto; r.grTarget = 4; r.deessScale = 1.6f;
                                    r.rev = RevType::plate; r.revMix = 0.18f; r.width = 1.3f; r.breath = 0.0f; break;
        case VoiceType::grito:      r.hpfMul = 1.1f; r.presence = -1.0f; r.air = 0.5f; r.comp = CompType::fet; r.grTarget = 8; r.ratio = 6; r.harsh = 0.6f;
                                    r.harmOn = true; r.harm = HarmType::transformer; r.drive = 0.35f; r.rev = RevType::room; r.revMix = 0.14f; break;
        case VoiceType::hablada:    r.hpfMul = 0.9f; r.presence = 1.5f; r.air = 1.0f; r.comp = CompType::vca; r.grTarget = 5; r.rev = RevType::ambience; r.revMix = 0.12f; r.width = 1.0f; break;
        case VoiceType::textura:    r.hpfMul = 2.0f; r.presence = -1.0f; r.air = 3.0f; r.body = -3.0f; r.comp = CompType::opto; r.grTarget = 3;
                                    r.rev = RevType::hall; r.revMix = 0.45f; r.decay = 4.0f; r.width = 1.8f; r.doubler = 0.3f; r.duck = 0.2f; break;
        case VoiceType::ambiente:   r.hpfMul = 2.2f; r.presence = -2.0f; r.air = 2.0f; r.body = -3.0f; r.comp = CompType::opto; r.grTarget = 3;
                                    r.rev = RevType::hall; r.revMix = 0.55f; r.decay = 5.0f; r.revTone = 0.4f; r.width = 1.9f; r.duck = 0.1f; break;
        case VoiceType::efecto:     r.hpfMul = 1.3f; r.presence = 1.0f; r.air = 1.0f; r.comp = CompType::fet; r.grTarget = 6; r.harmOn = true; r.harm = HarmType::transformer; r.drive = 0.5f;
                                    r.rev = RevType::plate; r.revMix = 0.2f; r.dlyMix = 0.25f; r.dly = DlyType::pingpong; r.width = 1.4f; break;
        case VoiceType::count: break;
    }
    return r;
}

// ------------------------------------------------------------- Clasificación
// Reglas sobre medidas reales (no un modelo entrenado). Se muestra la confianza y el motivo, y el usuario
// puede cambiar el tipo con un clic.
Classification classify (const AnalysisResult& a)
{
    Classification c;
    if (a.activeSeconds < 2.0f) { c.reason = "Analizando..."; return c; }
    std::array<float, (int) VoiceType::count> s {};
    auto clamp01 = [] (float v) { return juce::jlimit (0.0f, 1.0f, v); };
    const float sparse = clamp01 ((0.45f - a.activity) / 0.3f);
    const float shortPh = clamp01 ((1.8f - a.phraseMedian) / 1.2f);
    const float longPh = clamp01 ((a.phraseMedian - 1.5f) / 1.5f);

    s[(int) VoiceType::chop] = clamp01 ((a.phraseShortRatio - 0.35f) / 0.3f);
    s[(int) VoiceType::susurro] = clamp01 ((0.35f - a.voicedRatio) / 0.25f) * clamp01 ((a.sibilance + 18.0f) / 8.0f);
    s[(int) VoiceType::grito] = clamp01 ((a.centroid - 2200.0f) / 1200.0f) * clamp01 ((9.0f - a.dynamicSpread) / 5.0f) * clamp01 ((a.harsh + 11.0f) / 4.0f);
    s[(int) VoiceType::coro] = longPh * clamp01 (std::max ((0.65f - a.voicedRatio) / 0.3f, (a.sideRatio - 0.08f) / 0.15f)) * clamp01 (a.activity / 0.4f);
    s[(int) VoiceType::adlib] = sparse * shortPh * (1.0f - s[(int) VoiceType::chop] * 0.7f);
    s[(int) VoiceType::hablada] = clamp01 ((a.voicedRatio - 0.3f) / 0.2f) * clamp01 ((0.6f - a.voicedRatio) / 0.2f) * 0.6f;

    if (a.hasLead)
    {
        const float follows = clamp01 ((a.envelopeCorrelation - 0.35f) / 0.35f) * clamp01 ((a.overlapWhenLead - 0.45f) / 0.35f);
        s[(int) VoiceType::doble] = follows * clamp01 ((a.pitchAgree - 0.4f) / 0.35f);
        s[(int) VoiceType::armonia] = follows * clamp01 ((0.5f - a.pitchAgree) / 0.35f) * clamp01 ((std::abs (a.pitchOffsetSemis) - 1.5f) / 1.5f);
        s[(int) VoiceType::respuesta] = clamp01 ((a.overlapWhenSilent - a.overlapWhenLead) / 0.3f) * clamp01 ((a.overlapWhenSilent - 0.15f) / 0.2f);
        // Un ad-lib suena a ratos y casi siempre encima o entre frases de la principal
        s[(int) VoiceType::adlib] *= 1.0f - 0.6f * s[(int) VoiceType::doble];
    }
    else c.needsLead = true;

    int best = 0; float b1 = -1, b2 = -1;
    for (int i = 0; i < (int) s.size(); ++i)
    {
        if (s[(size_t) i] > b1) { b2 = b1; b1 = s[(size_t) i]; best = i; }
        else if (s[(size_t) i] > b2) b2 = s[(size_t) i];
    }
    if (b1 < 0.15f)
    {
        // Nada claro: ad-lib si suena a ratos, segunda voz si suena casi siempre
        best = (int) (a.activity < 0.4f ? VoiceType::adlib : VoiceType::segunda);
        b1 = 0.3f; b2 = 0.2f;
    }
    c.type = (VoiceType) best;
    const float margin = clamp01 ((b1 - std::max (0.0f, b2)) / 0.5f);
    c.confidence = clamp01 (0.45f + 0.35f * margin + 0.2f * b1) * clamp01 (0.4f + 0.6f * a.progress);

    const juce::String pct = juce::String (juce::roundToInt (a.activity * 100.0f)) + " %";
    switch (c.type)
    {
        case VoiceType::adlib:     c.reason = "Suena a ratos (" + pct + " del tiempo) y en frases cortas"; break;
        case VoiceType::coro:      c.reason = T("Frases largas y sonido de varias voces o abierto en est\xc3\xa9reo"); break;
        case VoiceType::doble:     c.reason = "Sigue a la voz principal: mismo ritmo y misma nota"; break;
        case VoiceType::armonia:   c.reason = "Sigue a la voz principal en otra nota (" + juce::String (a.pitchOffsetSemis, 1) + " st)"; break;
        case VoiceType::respuesta: c.reason = "Entra sobre todo cuando la voz principal calla"; break;
        case VoiceType::chop:      c.reason = "Golpes vocales muy cortos"; break;
        case VoiceType::susurro:   c.reason = "Casi sin tono y con mucho aire"; break;
        case VoiceType::grito:     c.reason = T("Brillante, fuerte y con poca din\xc3\xa1mica"); break;
        case VoiceType::hablada:   c.reason = "Voz hablada: tono a medias"; break;
        default:                   c.reason = "Suena casi todo el tiempo"; break;
    }
    return c;
}

// ------------------------------------------------------------- Construcción de la cadena
namespace
{
juce::String hz (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz"; }
juce::String db (float v) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; }
const char* revName (RevType t) { static const char* n[] = { "plate", "hall", "room", "chamber", "ambience", "IR" }; return n[(int) t]; }
const char* compName (CompType t) { static const char* n[] = { "FET", "Opto", "VCA", "Vari-Mu", "Clean" }; return n[(int) t]; }
const char* harmName (HarmType t) { static const char* n[] = { "v\xc3\xa1lvula", "cinta", "transformador", "excitador" }; return n[(int) t]; }
} // namespace

BuildResult buildChain (const Recipe& r, const AnalysisResult& a, int genreIdx)
{
    BuildResult out;
    auto set = [&] (const char* id, float v) { out.values.push_back ({ id, v }); };
    const bool haveGenre = genreIdx >= 0;
    const GenreProfile g = haveGenre ? genre (genreIdx) : GenreProfile { "", "", 0, 0, HarmType::tube, r.rev, 1, 1, 1, 0, 0 };
    const bool measured = a.activeSeconds >= 2.0f;

    // ---------------- EQ
    static const float baseHpf[] = { 150, 200, 170, 170, 190, 140, 120, 160, 160, 140, 100, 250, 300, 180 };
    float hpf = baseHpf[(int) r.type] * r.hpfMul / 1.2f;
    juce::String why = "Paso alto en " + hz (hpf);
    float lowGain = r.body;
    if (measured && a.mud > -10.0f) { lowGain += -juce::jlimit (0.0f, 4.0f, (a.mud + 10.0f) * 0.6f); hpf *= 1.15f; why += ", graves cargados: " + db (lowGain) + " en 250 Hz"; }
    float midGain = 0.0f;
    if (measured && a.nasal > -9.0f) { midGain = -juce::jlimit (0.0f, 4.0f, (a.nasal + 9.0f) * 0.7f); why += ", algo nasal: " + db (midGain) + " en 1 kHz"; }
    float presF = 3500.0f, presG = r.presence + g.bright * 0.5f, presDyn = 0.0f;
    if (measured && a.harsh > -9.0f) presG -= juce::jlimit (0.0f, 2.0f, (a.harsh + 9.0f) * 0.5f);
    if (a.hasLead && a.conflictBand >= 0 && a.conflictAmount > 0.15f)
    {
        presF = Analyzer::bandCentre (a.conflictBand);
        presG = -(1.0f + 3.0f * a.conflictAmount);
        presDyn = 85.0f;
        why = "Choca con la voz principal en " + hz (presF) + ": baja " + juce::String (-presG, 1) + T(" dB ah\xc3\xad s\xc3\xb3lo cuando canta la principal");
    }
    else if ((a.hasInstrumental || a.groupHasSpectrum) && presG > -0.5f)
    {
        // Sin choque con la principal: la presencia va donde el beat y las otras capas vocales dejan hueco (2-6 kHz)
        auto norm = [] (const std::array<float, kBands>& v, int b) { float m = 0; for (auto x : v) m += x; return v[(size_t) b] - m / (float) kBands; };
        int best = -1; float bestV = 1.0e9f;
        for (int b = 0; b < kBands; ++b)
        {
            const float f = Analyzer::bandCentre (b);
            if (f < 2000.0f || f > 6000.0f) continue;
            const float crowd = (a.hasInstrumental ? norm (a.instrumentalSpectrum, b) : 0.0f) + (a.groupHasSpectrum ? norm (a.groupSpectrum, b) : 0.0f);
            if (crowd < bestV) { bestV = crowd; best = b; }
        }
        if (best >= 0)
        {
            presF = Analyzer::bandCentre (best); presG = std::max (presG, 0.0f) + 1.0f;
            why += T (", presencia ") + db (presG) + T (" en ") + hz (presF) + T (a.hasInstrumental && a.groupHasSpectrum ? ", donde el beat y las otras capas dejan hueco"
                                                                                 : a.hasInstrumental ? ", donde el instrumental deja hueco" : ", donde las otras capas vocales no se acumulan");
        }
    }
    else if (std::abs (presG) > 0.2f) why += ", presencia " + db (presG) + " en " + hz (presF);
    if (a.hasInstrumental)
    {
        float mean = 0; for (auto v : a.instrumentalSpectrum) mean += v; mean /= (float) kBands;
        const float lows = (a.instrumentalSpectrum[0] + a.instrumentalSpectrum[1] + a.instrumentalSpectrum[2]) / 3.0f - mean;
        if (lows > 6.0f) { hpf *= 1.15f; why += T (", el beat tiene graves fuertes: paso alto m\xc3\xa1s arriba"); }
    }
    float airG = r.air + g.bright * 0.5f;
    if (measured) airG += juce::jlimit (-2.0f, 2.0f, (-26.0f - a.air) * 0.25f);
    airG = juce::jlimit (-3.0f, 6.0f, airG);
    hpf = juce::jlimit (40.0f, 400.0f, hpf);
    set (pid::eqOn, 1); set (pid::hpf, hpf); set (pid::hpfSlope, 1); set (pid::lowFreq, 250); set (pid::lowGain, lowGain);
    set (pid::midFreq, 1000); set (pid::midGain, midGain); set (pid::midQ, 1.4f);
    set (pid::presFreq, presF); set (pid::presGain, presG); set (pid::presQ, presDyn > 0 ? 1.6f : 1.0f); set (pid::presDyn, presDyn);
    set (pid::airFreq, 11000); set (pid::airGain, airG);
    out.why[0] = why + ".";

    // ---------------- COMP: umbral relativo a cómo canta (p90), para conseguir la reducción buscada
    const float gr = r.grTarget + (haveGenre ? g.press * 0.5f : 0.0f);
    const float thr = measured ? juce::jlimit (-50.0f, -3.0f, a.levelP90 - gr * 1.4f) : -20.0f;
    set (pid::compOn, 1); set (pid::compType, (float) (int) r.comp); set (pid::compThresh, thr); set (pid::compRatio, r.ratio);
    set (pid::compAttack, r.attack); set (pid::compRelease, r.release); set (pid::compMakeup, gr * 0.5f); set (pid::compMix, r.compMix * 100.0f);
    out.why[1] = juce::String (compName (r.comp)) + " para unos " + juce::String (juce::roundToInt (gr)) + " dB de control"
               + (measured && a.dynamicSpread > 14.0f ? ": la toma tiene mucha diferencia entre partes flojas y fuertes" : "");

    // ---------------- HARMONICS
    const bool harmOn = r.harmOn || (haveGenre && g.grit > 0.4f);
    const HarmType ht = r.harmOn ? r.harm : g.harm;
    const float drive = juce::jlimit (0.0f, 1.0f, (r.harmOn ? r.drive : 0.15f) + (haveGenre ? g.grit * 0.3f : 0.0f));
    set (pid::harmOn, harmOn ? 1.0f : 0.0f); set (pid::harmType, (float) (int) ht); set (pid::harmDrive, drive * 100.0f);
    set (pid::harmTone, r.harmTone * 100.0f); set (pid::harmMix, r.harmMix * 100.0f);
    out.why[2] = harmOn ? T("Saturaci\xc3\xb3n de ") + T (harmName (ht)) + " al " + juce::String (juce::roundToInt (drive * 100.0f)) + " %, con sobremuestreo"
                        : T("Sin saturaci\xc3\xb3n: la voz ya tiene car\xc3\xa1" "cter para este estilo");

    // ---------------- VOCAL
    const float deessAmt = juce::jlimit (0.05f, 1.0f, (measured ? (a.sibilance + 20.0f) / 10.0f : 0.4f) * r.deessScale);
    const float harshAmt = juce::jlimit (0.0f, 0.9f, r.harsh + (measured ? (a.harsh + 12.0f) / 10.0f : 0.0f));
    set (pid::vocOn, 1); set (pid::deess, deessAmt * 100.0f); set (pid::deessFreq, measured ? juce::jlimit (4000.0f, 10000.0f, a.sibilantFreq) : 6500.0f);
    set (pid::harsh, harshAmt * 100.0f); set (pid::breath, r.breath * 100.0f);
    out.why[3] = "De-esser en " + hz (measured ? a.sibilantFreq : 6500.0f) + (measured && a.sibilance > -14.0f ? " (sibilancia alta)" : "")
               + (r.breath > 0.05f ? ", respiraciones -" + juce::String (juce::roundToInt (r.breath * 12.0f)) + " dB" : "");

    // ---------------- SPACE
    const RevType rt = haveGenre && r.genre < 0 ? g.rev : r.rev;
    const float revMix = juce::jlimit (0.0f, 0.8f, r.revMix * (haveGenre ? g.revAmt : 1.0f));
    const float decay = juce::jlimit (0.3f, 8.0f, r.decay * (haveGenre ? g.revSize : 1.0f));
    const float dlyMix = juce::jlimit (0.0f, 0.6f, r.dlyMix * (haveGenre ? g.delayAmt : 1.0f));
    float width = r.width + (haveGenre ? g.width * 0.2f : 0.0f);
    if (measured && a.sideRatio > 0.2f) width = std::min (width, 1.2f);   // ya viene abierta: no exagerar
    set (pid::spaceOn, 1); set (pid::revType, (float) (int) rt); set (pid::revMix, revMix * 100.0f); set (pid::revDecay, decay);
    // Grupo: cada capa con un pre-delay distinto para que las colas no se fundan en una sola
    const float predelay = std::min (200.0f, r.predelay + 8.0f * (float) a.groupIndex);
    set (pid::revPredelay, predelay); set (pid::revTone, r.revTone * 100.0f);
    set (pid::dlyType, (float) (int) r.dly); set (pid::dlyNote, (float) r.dlyNote); set (pid::dlyFeedback, r.dlyFb * 100.0f);
    set (pid::dlyMix, dlyMix * 100.0f); set (pid::duck, r.duck * 100.0f); set (pid::width, juce::jlimit (0.0f, 2.0f, width) * 100.0f);
    set (pid::doubler, r.doubler * 100.0f);
    set (pid::dlyThrow, r.throwAmt * 100.0f); set (pid::revSwell, r.swell * 100.0f);
    out.why[4] = "Reverb " + juce::String (revName (rt)) + " de " + juce::String (decay, 1) + " s"
               + (dlyMix > 0.01f ? ", delay " + chainivo::noteNames()[r.dlyNote] : juce::String())
               + (r.duck > 0.3f ? ", con ducking para que no tape la voz" : "")
               + (r.throwAmt > 0.01f ? T (", eco de remate al final de cada frase") : juce::String())
               + (r.swell > 0.01f ? T (", la cola crece al acabar cada frase") : juce::String())
               + (a.groupCount > 0 ? T (", pre-delay ") + juce::String (juce::roundToInt (predelay)) + T (" ms para separarla de las otras ") + juce::String (a.groupCount) + T (" capas") : juce::String());

    // ---------------- CREATIVE (sólo si la receta lo pide)
    set (pid::creaOn, r.creaOn ? 1.0f : 0.0f); set (pid::creaType, (float) (int) r.crea); set (pid::creaNote, (float) r.creaNote); set (pid::creaAmount, r.creaAmount * 100.0f); set (pid::creaPitch, r.creaPitch);
    out.why[5] = r.creaOn ? T("Efecto r\xc3\xadtmico elegido por el preset") : "Apagado: nunca se activa solo";

    out.enginesOn = 4 + (harmOn ? 1 : 0) + (r.creaOn ? 1 : 0);
    return out;
}
} // namespace chainivo

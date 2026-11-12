#include "PresetLibrary.h"

namespace chainivo
{
namespace
{
juce::String T (const char* s) { return juce::String::fromUTF8 (s); }

juce::PropertiesFile::Options settingsOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "chainIVO"; o.folderName = "IVO Beatz"; o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    return o;
}

// Atajo para escribir recetas: parte de la receta base del tipo y cambia lo que define el preset
struct R
{
    Recipe r;
    R (VoiceType t, const char* name, const char* genreName, const char* intention, const char* tags)
    {
        r = defaultRecipe (t);
        r.name = T (name); r.intention = T (intention); r.tags = T (tags);
        r.genre = genreIndex (T (genreName));
    }
    R& eq (float hpfMul, float presence, float air, float body = 0) { r.hpfMul = hpfMul; r.presence = presence; r.air = air; r.body = body; return *this; }
    R& comp (CompType t, float gr, float ratio, float att, float rel, float mix = 1) { r.comp = t; r.grTarget = gr; r.ratio = ratio; r.attack = att; r.release = rel; r.compMix = mix; return *this; }
    R& harm (HarmType t, float drive, float mix = 1, float tone = 0) { r.harmOn = true; r.harm = t; r.drive = drive; r.harmMix = mix; r.harmTone = tone; return *this; }
    R& noHarm() { r.harmOn = false; return *this; }
    R& voc (float deess, float harsh, float breath) { r.deessScale = deess; r.harsh = harsh; r.breath = breath; return *this; }
    R& rev (RevType t, float mix, float decay, float pre = 20, float tone = 0.6f) { r.rev = t; r.revMix = mix; r.decay = decay; r.predelay = pre; r.revTone = tone; return *this; }
    R& dly (DlyType t, int note, float mix, float fb, float duck = 0.5f) { r.dly = t; r.dlyNote = note; r.dlyMix = mix; r.dlyFb = fb; r.duck = duck; return *this; }
    R& stereo (float width, float doubler = 0) { r.width = width; r.doubler = doubler; return *this; }
    R& crea (CreaType t, int note, float amount) { r.creaOn = true; r.crea = t; r.creaNote = note; r.creaAmount = amount; return *this; }
};

// Notas: 0 1/32 · 1 1/16T · 2 1/16 · 3 1/8T · 4 1/8 · 5 1/8D · 6 1/4T · 7 1/4 · 8 1/4D · 9 1/2 · 10 compás
std::vector<Recipe> makeFactory()
{
    using V = VoiceType; using C = CompType; using H = HarmType; using RV = RevType; using D = DlyType; using CR = CreaType;
    std::vector<R> p;
    // ---------------- AD-LIBS
    p.push_back (R (V::adlib, "Ad-lib Glow", "Rage", "Ad-lib brillante que vuela por encima con delay corto", "brillante,delay,rage")
        .eq (1.3f, 2.5f, 3.0f).comp (C::fet, 7, 6, 1.5f, 80).harm (H::transformer, 0.35f).rev (RV::plate, 0.18f, 1.2f).dly (D::digital, 5, 0.2f, 0.3f, 0.7f).stereo (1.4f));
    p.push_back (R (V::adlib, "Ad-lib Crank Dirt", "Crank", "Sucio y agresivo, saturado, casi seco", "sucio,saturado,seco")
        .eq (1.4f, 3.0f, 2.0f).comp (C::fet, 9, 8, 0.8f, 60).harm (H::transformer, 0.6f).rev (RV::room, 0.1f, 0.7f).dly (D::analog, 4, 0.12f, 0.2f).stereo (1.2f));
    p.push_back (R (V::adlib, "Ad-lib Plugg Air", "Plugg", "Suave y aireado, lejos, con eco largo", "aire,suave,lejano")
        .eq (1.5f, 0.5f, 3.5f, -1.5f).comp (C::opto, 4, 3, 10, 200).noHarm().rev (RV::hall, 0.3f, 2.8f, 35).dly (D::tape, 7, 0.25f, 0.45f, 0.6f).stereo (1.6f));
    p.push_back (R (V::adlib, "Ad-lib Dark Echo", "Dark Trap", "Oscuro, con eco en ping-pong y cola grande", "oscuro,pingpong,espacio")
        .eq (1.3f, 0.0f, -1.0f).comp (C::vca, 6, 4, 4, 120).harm (H::tube, 0.3f).rev (RV::hall, 0.25f, 3.2f, 30, 0.35f).dly (D::pingpong, 5, 0.25f, 0.4f, 0.6f).stereo (1.5f));
    p.push_back (R (V::adlib, "Ad-lib Street", "Street rap", "Seco y al frente, como en un directo de calle", "seco,frente,rap")
        .eq (1.2f, 2.0f, 1.0f).comp (C::fet, 8, 6, 2, 90).harm (H::tape, 0.35f).rev (RV::ambience, 0.12f, 0.5f).dly (D::digital, 4, 0.0f, 0.2f).stereo (1.0f));
    p.push_back (R (V::adlib, "Ad-lib Perreo", "Reggaet\xc3\xb3n moderno", "Ad-lib de reggaetón: presente, con plate corto y eco a corcheas", "reggaeton,plate,corchea")
        .eq (1.3f, 2.0f, 2.5f).comp (C::vca, 6, 4, 3, 100).harm (H::exciter, 0.3f).rev (RV::plate, 0.15f, 1.0f).dly (D::digital, 4, 0.18f, 0.25f, 0.6f).stereo (1.3f));
    p.push_back (R (V::adlib, "Ad-lib 2000s Tape", "Rap de los 2000", "Color de cinta y eco de cinta, sabor dosmilero", "cinta,2000,vintage")
        .eq (1.3f, 1.0f, -0.5f).comp (C::opto, 6, 4, 10, 150).harm (H::tape, 0.45f, 1, -0.3f).rev (RV::room, 0.12f, 0.9f).dly (D::tape, 5, 0.2f, 0.35f));
    p.push_back (R (V::adlib, "Ad-lib Radio", "Trap experimental", "Banda estrecha tipo radio, para que se distinga sin subir volumen", "radio,lofi,filtro")
        .eq (3.0f, 4.0f, -4.0f, -3.0f).comp (C::fet, 8, 8, 1, 70).harm (H::transformer, 0.55f).rev (RV::room, 0.1f, 0.6f).dly (D::analog, 4, 0.15f, 0.25f));
    p.push_back (R (V::adlib, "Ad-lib Stutter", "Rage", "Ad-lib que se repite al tempo (beat repeat a semicorcheas)", "stutter,ritmo,efecto")
        .eq (1.3f, 2.5f, 2.5f).comp (C::fet, 7, 6, 1.5f, 80).harm (H::transformer, 0.3f).rev (RV::plate, 0.14f, 1.0f).crea (CR::stutter, 2, 0.8f));
    // ---------------- COROS
    p.push_back (R (V::coro, "Coro Wide Hall", "R&B", "Coro ancho y suave detrás de la voz principal", "ancho,hall,suave")
        .eq (1.6f, 0.0f, 3.0f, -2.0f).comp (C::opto, 4, 3, 10, 250).noHarm().rev (RV::hall, 0.28f, 2.6f, 30).stereo (1.7f, 0.25f));
    p.push_back (R (V::coro, "Coro Stack Pop", "Pop", "Coro brillante y compacto, pegado al ritmo", "brillante,compacto,pop")
        .eq (1.5f, 1.0f, 3.5f, -1.5f).comp (C::vca, 6, 4, 3, 100).harm (H::exciter, 0.25f).rev (RV::plate, 0.2f, 1.6f).stereo (1.6f, 0.2f));
    p.push_back (R (V::coro, "Coro Church", "Experimental", "Coro grande, oscuro y lejano, como en una nave", "grande,oscuro,catedral")
        .eq (1.8f, -1.0f, 1.0f, -2.0f).comp (C::varimu, 4, 3, 20, 400).harm (H::tube, 0.2f).rev (RV::hall, 0.5f, 5.0f, 45, 0.35f).stereo (1.8f, 0.3f));
    p.push_back (R (V::coro, "Coro Plugg Clouds", "PluggnB", "Coro etéreo con eco de cinta y mucho aire", "etereo,cinta,aire")
        .eq (1.7f, 0.0f, 4.0f, -2.5f).comp (C::opto, 4, 3, 10, 300).noHarm().rev (RV::hall, 0.35f, 3.5f, 40).dly (D::tape, 7, 0.2f, 0.4f, 0.5f).stereo (1.8f, 0.3f));
    p.push_back (R (V::coro, "Coro Reggaet\xc3\xb3n", "Est\xc3\xa9tica La Bendici\xc3\xb3n", "Coro de estribillo de reggaetón, ancho pero firme", "reggaeton,estribillo,firme")
        .eq (1.5f, 1.0f, 2.5f, -1.0f).comp (C::vca, 6, 4, 4, 120).harm (H::tube, 0.25f).rev (RV::plate, 0.2f, 1.4f).stereo (1.6f, 0.2f));
    p.push_back (R (V::coro, "Coro Indie Room", "Indie rock", "Coro de banda en una sala, natural", "sala,natural,indie")
        .eq (1.4f, 0.5f, 1.5f).comp (C::varimu, 4, 3, 15, 300).harm (H::tube, 0.3f).rev (RV::chamber, 0.25f, 1.6f).stereo (1.5f, 0.15f));
    p.push_back (R (V::coro, "Coro Gang", "Street rap", "Coro de grupo, gritado y seco, al frente", "grupo,seco,energia")
        .eq (1.3f, 2.0f, 1.0f).comp (C::fet, 8, 6, 2, 90).harm (H::transformer, 0.4f).voc (1.0f, 0.6f, 0.3f).rev (RV::room, 0.14f, 0.8f).stereo (1.6f, 0.2f));
    p.push_back (R (V::coro, "Coro Dark Pad", "Dark Trap", "Coro convertido en colchón oscuro", "colchon,oscuro,pad")
        .eq (2.0f, -2.0f, 0.0f, -3.0f).comp (C::opto, 5, 4, 10, 400).harm (H::tape, 0.3f, 1, -0.5f).rev (RV::hall, 0.45f, 4.5f, 30, 0.3f).stereo (1.9f, 0.3f));
    // ---------------- DOBLES Y SEGUNDAS
    p.push_back (R (V::doble, "Doble Tight", "Trap comercial", "Doble pegado a la principal, más estrecho y sin pisarla", "doble,apretado")
        .eq (1.4f, -1.0f, 1.0f, -1.5f).comp (C::vca, 7, 5, 3, 90).noHarm().voc (1.5f, 0.4f, 0.8f).rev (RV::plate, 0.08f, 1.0f).stereo (1.5f));
    p.push_back (R (V::doble, "Doble Wide L/R", "Rage", "Doble abierto a los lados con doblador", "doble,ancho")
        .eq (1.4f, 0.0f, 2.0f, -1.5f).comp (C::fet, 7, 6, 2, 80).harm (H::transformer, 0.25f).voc (1.4f, 0.4f, 0.8f).rev (RV::plate, 0.1f, 1.0f).stereo (1.8f, 0.3f));
    p.push_back (R (V::doble, "Doble Shadow", "Dark Trap", "Doble oscuro y bajo, como una sombra de la principal", "doble,oscuro")
        .eq (1.6f, -2.0f, -1.5f, -1.0f).comp (C::opto, 6, 4, 10, 200).harm (H::tube, 0.25f, 1, -0.4f).voc (1.6f, 0.4f, 0.9f).rev (RV::hall, 0.15f, 2.0f).stereo (1.5f));
    p.push_back (R (V::doble, "Doble 2000s", "Hip-hop cl\xc3\xa1sico", "Doble de rap clásico, con color de cinta", "doble,cinta,rap")
        .eq (1.3f, 0.5f, 0.0f).comp (C::fet, 8, 6, 2, 100).harm (H::tape, 0.4f).voc (1.4f, 0.4f, 0.7f).rev (RV::room, 0.08f, 0.7f).stereo (1.3f));
    p.push_back (R (V::segunda, "Segunda Sweet", "Trap mel\xc3\xb3" "dico", "Segunda voz dulce, un paso por detrás", "segunda,suave")
        .eq (1.4f, 0.0f, 2.0f, -1.0f).comp (C::opto, 5, 3, 10, 200).noHarm().rev (RV::plate, 0.18f, 1.6f).dly (D::digital, 7, 0.1f, 0.3f).stereo (1.4f, 0.1f));
    p.push_back (R (V::segunda, "Segunda Rock", "Alternative rock", "Segunda voz con cuerpo y algo de saturación", "segunda,rock")
        .eq (1.3f, 1.0f, 1.0f).comp (C::varimu, 6, 4, 15, 250).harm (H::tube, 0.4f).rev (RV::chamber, 0.16f, 1.4f).stereo (1.3f));
    // ---------------- ARMONÍAS
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Silk", "R&B", "Armonía sedosa, aire arriba y nada de dureza", "armonia,seda,aire")
        .eq (1.5f, -0.5f, 3.0f, -1.5f).comp (C::opto, 4, 3, 10, 250).noHarm().voc (1.2f, 0.6f, 0.6f).rev (RV::hall, 0.24f, 2.2f, 30).stereo (1.6f, 0.15f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Stack", "Pop", "Armonías apiladas, brillantes y unidas", "armonia,pila,brillo")
        .eq (1.5f, 0.5f, 3.5f, -1.5f).comp (C::vca, 5, 4, 4, 120).harm (H::exciter, 0.2f).rev (RV::plate, 0.2f, 1.8f).stereo (1.7f, 0.2f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Ghost", "PluggnB", "Armonía fantasma, muy lejos y con eco", "armonia,lejos,fantasma")
        .eq (1.9f, -2.0f, 2.5f, -2.5f).comp (C::opto, 4, 3, 10, 300).noHarm().rev (RV::hall, 0.45f, 4.0f, 50, 0.45f).dly (D::tape, 8, 0.2f, 0.45f, 0.4f).stereo (1.8f, 0.25f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Vintage", "Hip-hop contempor\xc3\xa1neo", "Armonía con color de válvula y cámara", "armonia,valvula,vintage")
        .eq (1.4f, 0.0f, 0.5f).comp (C::varimu, 5, 4, 15, 300).harm (H::tube, 0.35f, 1, -0.2f).rev (RV::chamber, 0.2f, 1.8f).stereo (1.4f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Wide Air", "Electr\xc3\xb3nica", "Armonía muy abierta, con aire y doblador", "armonia,ancho,aire")
        .eq (1.6f, 0.0f, 4.0f, -2.0f).comp (C::clean, 5, 3, 5, 150).harm (H::exciter, 0.25f).rev (RV::hall, 0.3f, 2.8f, 30).stereo (1.9f, 0.35f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Room", "Indie experimental", "Armonía en sala pequeña, íntima", "armonia,sala,intima")
        .eq (1.3f, 0.5f, 1.0f).comp (C::opto, 4, 3, 10, 200).harm (H::tape, 0.25f).rev (RV::room, 0.22f, 1.0f).stereo (1.3f));
    // ---------------- EFECTOS
    p.push_back (R (V::efecto, "FX Reverse Swell", "Experimental", "Eco invertido que anticipa cada frase", "reverse,efecto")
        .eq (1.5f, 0.0f, 2.0f).comp (C::opto, 5, 4, 10, 200).noHarm().rev (RV::hall, 0.3f, 3.0f).crea (CR::reverse, 7, 0.7f).stereo (1.6f));
    p.push_back (R (V::efecto, "FX Gate 1/16", "Electr\xc3\xb3nica", "Puerta rítmica a semicorcheas", "gate,ritmo")
        .eq (1.3f, 1.5f, 2.0f).comp (C::vca, 6, 4, 3, 100).harm (H::exciter, 0.2f).rev (RV::plate, 0.2f, 1.5f).crea (CR::gate, 2, 0.9f).stereo (1.4f));
    p.push_back (R (V::efecto, "FX Stutter 1/8", "Rage", "Repetición a corcheas, para finales de frase", "stutter,ritmo")
        .eq (1.3f, 2.0f, 2.0f).comp (C::fet, 7, 6, 2, 80).harm (H::transformer, 0.4f).rev (RV::plate, 0.15f, 1.2f).crea (CR::stutter, 4, 0.85f));
    p.push_back (R (V::efecto, "FX Telephone", "Trap experimental", "Teléfono: banda estrecha y saturada", "telefono,lofi")
        .eq (4.0f, 5.0f, -6.0f, -4.0f).comp (C::fet, 10, 10, 1, 60).harm (H::transformer, 0.7f).rev (RV::room, 0.08f, 0.5f).stereo (1.0f));
    p.push_back (R (V::efecto, "FX Tape Wobble", "Indie experimental", "Eco de cinta inestable y saturado", "cinta,wobble")
        .eq (1.4f, 0.0f, -1.0f).comp (C::opto, 5, 4, 10, 200).harm (H::tape, 0.55f, 1, -0.3f).rev (RV::chamber, 0.2f, 1.6f).dly (D::tape, 5, 0.35f, 0.55f, 0.3f));
    p.push_back (R (V::efecto, "FX Ping-Pong Throw", "Reggaet\xc3\xb3n + trap", "Eco que salta de lado a lado para remates", "pingpong,eco")
        .eq (1.4f, 1.5f, 2.0f).comp (C::vca, 6, 4, 3, 100).noHarm().rev (RV::plate, 0.12f, 1.2f).dly (D::pingpong, 4, 0.35f, 0.5f, 0.5f).stereo (1.5f));
    p.push_back (R (V::efecto, "FX Distant Ghost", "Dark Trap", "Voz lejana, casi solo reverb", "lejos,reverb")
        .eq (2.2f, -3.0f, 1.0f, -3.0f).comp (C::opto, 4, 3, 10, 300).noHarm().rev (RV::hall, 0.6f, 5.5f, 60, 0.3f).stereo (1.9f, 0.3f));
    p.push_back (R (V::efecto, "FX Megaphone", "Alternative rock", "Megáfono: medios duros y distorsión", "megafono,distorsion")
        .eq (3.5f, 6.0f, -5.0f, -3.0f).comp (C::fet, 10, 12, 0.5f, 50).harm (H::transformer, 0.85f).rev (RV::ambience, 0.1f, 0.4f));
    p.push_back (R (V::textura, "Texture Clouds", "Experimental", "La voz convertida en textura de fondo", "textura,ambiente")
        .eq (2.2f, -2.0f, 3.0f, -3.0f).comp (C::opto, 3, 3, 10, 400).noHarm().rev (RV::hall, 0.65f, 6.0f, 40, 0.5f).dly (D::tape, 9, 0.2f, 0.5f, 0.2f).stereo (2.0f, 0.35f));

    std::vector<Recipe> out;
    for (auto& r : p) out.push_back (r.r);
    return out;
}
} // namespace

PresetLibrary::PresetLibrary() : factory (makeFactory()) { loadFavourites(); }

juce::StringArray PresetLibrary::categories()
{
    return { "Ad-libs", "Coros", "Dobles y segundas", T ("Armon\xc3\xad" "as"), "Efectos", "Usuario" };
}

static juce::String categoryOf (VoiceType t)
{
    switch (t)
    {
        case VoiceType::adlib: case VoiceType::respuesta: case VoiceType::chop: case VoiceType::grito: case VoiceType::hablada: return "Ad-libs";
        case VoiceType::coro: return "Coros";
        case VoiceType::doble: case VoiceType::segunda: return "Dobles y segundas";
        case VoiceType::armonia: return T ("Armon\xc3\xad" "as");
        default: return "Efectos";
    }
}

std::vector<PresetInfo> PresetLibrary::all() const
{
    std::vector<PresetInfo> v;
    for (int i = 0; i < (int) factory.size(); ++i)
    {
        const auto& r = factory[(size_t) i];
        PresetInfo p; p.name = r.name; p.category = categoryOf (r.type); p.intention = r.intention; p.tags = r.tags;
        p.genreName = r.genre >= 0 ? T (genre (r.genre).name) : juce::String(); p.recipeIndex = i;
        v.push_back (p);
    }
    for (auto& f : userFolder().findChildFiles (juce::File::findFiles, false, "*.chainivo"))
    {
        PresetInfo p; p.name = f.getFileNameWithoutExtension(); p.category = "Usuario"; p.factory = false; p.file = f;
        if (auto xml = juce::XmlDocument::parse (f)) { p.intention = xml->getStringAttribute ("intention"); p.genreName = xml->getStringAttribute ("genre"); }
        v.push_back (p);
    }
    return v;
}

std::vector<PresetInfo> PresetLibrary::search (const juce::String& text, const juce::String& category, bool favouritesOnly) const
{
    std::vector<PresetInfo> v;
    for (auto& p : all())
    {
        if (category.isNotEmpty() && p.category != category) continue;
        if (favouritesOnly && ! isFavourite (p.name)) continue;
        if (text.isNotEmpty())
        {
            const auto hay = (p.name + " " + p.category + " " + p.genreName + " " + p.tags + " " + p.intention).toLowerCase();
            bool ok = true;
            for (auto& w : juce::StringArray::fromTokens (text.toLowerCase(), " ", "")) if (w.isNotEmpty() && ! hay.contains (w)) ok = false;
            if (! ok) continue;
        }
        v.push_back (p);
    }
    return v;
}

juce::File PresetLibrary::userFolder()
{
    auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("IVO Beatz").getChildFile ("chainIVO").getChildFile ("Presets");
    f.createDirectory();
    return f;
}

bool PresetLibrary::saveUser (const juce::String& name, const juce::ValueTree& state, const juce::String& category)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    auto xml = state.createXml();
    if (xml == nullptr) return false;
    xml->setAttribute ("presetName", name); xml->setAttribute ("category", category);
    xml->setAttribute ("format", 1);
    return xml->writeTo (userFolder().getChildFile (clean + ".chainivo"));
}

juce::ValueTree PresetLibrary::loadUser (const juce::File& f)
{
    if (auto xml = juce::XmlDocument::parse (f)) return juce::ValueTree::fromXml (*xml);
    return {};
}

bool PresetLibrary::importFile (const juce::File& f)
{
    if (! f.existsAsFile() || juce::XmlDocument::parse (f) == nullptr) return false;
    return f.copyFileTo (userFolder().getNonexistentChildFile (f.getFileNameWithoutExtension(), ".chainivo", false));
}

bool PresetLibrary::exportFile (const juce::String& name, const juce::File& dest) const
{
    const auto src = userFolder().getChildFile (juce::File::createLegalFileName (name) + ".chainivo");
    return src.existsAsFile() && src.copyFileTo (dest);
}

bool PresetLibrary::isFavourite (const juce::String& name) const { return favourites.contains (name); }
void PresetLibrary::toggleFavourite (const juce::String& name)
{
    if (favourites.contains (name)) favourites.removeString (name); else favourites.add (name);
    saveFavourites();
}
void PresetLibrary::loadFavourites()
{
    juce::PropertiesFile f (settingsOptions());
    favourites = juce::StringArray::fromTokens (f.getValue ("favourites"), "\n", "");
    favourites.removeEmptyStrings();
}
void PresetLibrary::saveFavourites() const
{
    juce::PropertiesFile f (settingsOptions());
    f.setValue ("favourites", favourites.joinIntoString ("\n"));
    f.saveIfNeeded();
}
} // namespace chainivo

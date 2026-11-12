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
    R& pitch (float st) { r.creaPitch = st; return *this; }
    R& words (int target, int fx, float amount) { r.wordOn = true; r.wordTarget = target; r.wordFx = fx; r.wordAmount = amount; return *this; }
    R& autom (float throwAmt, float swell) { r.throwAmt = throwAmt; r.swell = swell; return *this; }
};

// Notas: 0 1/32 · 1 1/16T · 2 1/16 · 3 1/8T · 4 1/8 · 5 1/8D · 6 1/4T · 7 1/4 · 8 1/4D · 9 1/2 · 10 compás
std::vector<Recipe> makeFactory()
{
    using V = VoiceType; using C = CompType; using H = HarmType; using RV = RevType; using D = DlyType; using CR = CreaType;
    std::vector<R> p;
    // ---------------- AD-LIBS
    p.push_back (R (V::adlib, "Ad-lib Glow", "Rage", "Ad-lib brillante que vuela por encima con delay corto", "brillante,delay,rage")
        .eq (1.3f, 2.5f, 3.0f).comp (C::fet, 7, 6, 1.5f, 80).harm (H::transformer, 0.35f).rev (RV::plate, 0.18f, 1.2f).dly (D::digital, 5, 0.2f, 0.3f, 0.7f).stereo (1.4f).autom (0.5f, 0.0f));
    p.push_back (R (V::adlib, "Ad-lib Crank Dirt", "Crank", "Sucio y agresivo, saturado, casi seco", "sucio,saturado,seco")
        .eq (1.4f, 3.0f, 2.0f).comp (C::fet, 9, 8, 0.8f, 60).harm (H::transformer, 0.6f).rev (RV::room, 0.1f, 0.7f).dly (D::analog, 4, 0.12f, 0.2f).stereo (1.2f));
    p.push_back (R (V::adlib, "Ad-lib Plugg Air", "Plugg", "Suave y aireado, lejos, con eco largo", "aire,suave,lejano")
        .eq (1.5f, 0.5f, 3.5f, -1.5f).comp (C::opto, 4, 3, 10, 200).noHarm().rev (RV::hall, 0.3f, 2.8f, 35).dly (D::tape, 7, 0.25f, 0.45f, 0.6f).stereo (1.6f).autom (0.4f, 0.3f));
    p.push_back (R (V::adlib, "Ad-lib Dark Echo", "Dark Trap", "Oscuro, con eco en ping-pong y cola grande", "oscuro,pingpong,espacio")
        .eq (1.3f, 0.0f, -1.0f).comp (C::vca, 6, 4, 4, 120).harm (H::tube, 0.3f).rev (RV::hall, 0.25f, 3.2f, 30, 0.35f).dly (D::pingpong, 5, 0.25f, 0.4f, 0.6f).stereo (1.5f).autom (0.4f, 0.4f));
    p.push_back (R (V::adlib, "Ad-lib Street", "Street rap", "Seco y al frente, como en un directo de calle", "seco,frente,rap")
        .eq (1.2f, 2.0f, 1.0f).comp (C::fet, 8, 6, 2, 90).harm (H::tape, 0.35f).rev (RV::ambience, 0.12f, 0.5f).dly (D::digital, 4, 0.0f, 0.2f).stereo (1.0f));
    p.push_back (R (V::adlib, "Ad-lib Perreo", "Reggaet\xc3\xb3n moderno", "Ad-lib de reggaetón: presente, con plate corto y eco a corcheas", "reggaeton,plate,corchea")
        .eq (1.3f, 2.0f, 2.5f).comp (C::vca, 6, 4, 3, 100).harm (H::exciter, 0.3f).rev (RV::plate, 0.15f, 1.0f).dly (D::digital, 4, 0.18f, 0.25f, 0.6f).stereo (1.3f).autom (0.5f, 0.0f));
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
        .eq (1.7f, 0.0f, 4.0f, -2.5f).comp (C::opto, 4, 3, 10, 300).noHarm().rev (RV::hall, 0.35f, 3.5f, 40).dly (D::tape, 7, 0.2f, 0.4f, 0.5f).stereo (1.8f, 0.3f).autom (0.0f, 0.5f));
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
        .eq (1.4f, 1.5f, 2.0f).comp (C::vca, 6, 4, 3, 100).noHarm().rev (RV::plate, 0.12f, 1.2f).dly (D::pingpong, 4, 0.35f, 0.5f, 0.5f).stereo (1.5f).autom (0.8f, 0.0f));
    p.push_back (R (V::efecto, "FX Distant Ghost", "Dark Trap", "Voz lejana, casi solo reverb", "lejos,reverb")
        .eq (2.2f, -3.0f, 1.0f, -3.0f).comp (C::opto, 4, 3, 10, 300).noHarm().rev (RV::hall, 0.6f, 5.5f, 60, 0.3f).stereo (1.9f, 0.3f));
    p.push_back (R (V::efecto, "FX Megaphone", "Alternative rock", "Megáfono: medios duros y distorsión", "megafono,distorsion")
        .eq (3.5f, 6.0f, -5.0f, -3.0f).comp (C::fet, 10, 12, 0.5f, 50).harm (H::transformer, 0.85f).rev (RV::ambience, 0.1f, 0.4f));
    p.push_back (R (V::textura, "Texture Clouds", "Experimental", "La voz convertida en textura de fondo", "textura,ambiente")
        .eq (2.2f, -2.0f, 3.0f, -3.0f).comp (C::opto, 3, 3, 10, 400).noHarm().rev (RV::hall, 0.65f, 6.0f, 40, 0.5f).dly (D::tape, 9, 0.2f, 0.5f, 0.2f).stereo (2.0f, 0.35f));

    // ======================= AMPLIACIÓN A 120 (30 ad-libs · 30 coros · 20 dobles/segundas · 20 armonías · 20 efectos)
    // ---------------- AD-LIBS (+21)
    p.push_back (R (V::adlib, "Ad-lib Crank Siren", "Crank", "Ad-lib chillón con distorsión y eco a semicorcheas", "agresivo,distorsion,eco")
        .eq (1.5f, 3.5f, 2.5f, -1.0f).comp (C::fet, 10, 10, 0.5f, 50).harm (H::transformer, 0.7f).rev (RV::plate, 0.08f, 0.8f).dly (D::digital, 2, 0.2f, 0.35f, 0.7f).stereo (1.3f));
    p.push_back (R (V::adlib, "Ad-lib Rage Wide", "Rage", "Ad-lib abierto a los lados, saturado y brillante", "ancho,brillante,rage")
        .eq (1.3f, 2.5f, 3.5f).comp (C::fet, 8, 6, 1, 70).harm (H::transformer, 0.45f).rev (RV::plate, 0.15f, 1.3f).dly (D::pingpong, 4, 0.18f, 0.3f, 0.6f).stereo (1.8f, 0.3f));
    p.push_back (R (V::adlib, "Ad-lib PluggnB Silk", "PluggnB", "Suave y melódico, flotando detrás", "suave,melodico,cinta")
        .eq (1.5f, 0.5f, 3.0f, -1.5f).comp (C::opto, 5, 3, 10, 250).noHarm().voc (1.3f, 0.5f, 0.6f).rev (RV::hall, 0.28f, 2.6f, 30).dly (D::tape, 7, 0.22f, 0.4f, 0.6f).stereo (1.5f).autom (0.4f, 0.3f));
    p.push_back (R (V::adlib, "Ad-lib Dark Whisper", "Dark Trap", "Ad-lib susurrado y oscuro, muy de fondo", "oscuro,susurro,fondo")
        .eq (1.4f, -1.5f, 0.0f, -1.0f).comp (C::opto, 6, 4, 10, 300).harm (H::tube, 0.2f, 1, -0.5f).voc (1.6f, 0.5f, 0.2f).rev (RV::hall, 0.35f, 3.8f, 40, 0.3f).stereo (1.6f, 0.2f));
    p.push_back (R (V::adlib, "Ad-lib Melodic Trap", "Trap mel\xc3\xb3" "dico", "Ad-lib cantado, limpio y con plate", "melodico,limpio")
        .eq (1.3f, 1.5f, 2.5f).comp (C::vca, 5, 4, 4, 120).harm (H::tape, 0.2f).rev (RV::plate, 0.22f, 1.8f).dly (D::digital, 7, 0.15f, 0.3f, 0.6f).stereo (1.4f));
    p.push_back (R (V::adlib, "Ad-lib Commercial Shine", "Trap comercial", "Pulido y brillante, listo para radio", "brillo,pulido,radio")
        .eq (1.3f, 2.0f, 3.5f).comp (C::vca, 6, 4, 3, 100).harm (H::exciter, 0.3f).voc (1.2f, 0.5f, 0.5f).rev (RV::plate, 0.16f, 1.2f).dly (D::digital, 5, 0.12f, 0.25f, 0.7f).stereo (1.4f));
    p.push_back (R (V::adlib, "Ad-lib Alt Grit", "Trap alternativo", "Granulado y con cuerpo, poco espacio", "grano,cuerpo")
        .eq (1.2f, 1.5f, 0.5f, 1.0f).comp (C::fet, 7, 5, 2, 90).harm (H::tape, 0.5f, 0.8f).rev (RV::chamber, 0.12f, 1.0f).stereo (1.2f));
    p.push_back (R (V::adlib, "Ad-lib Old School Perreo", "Reggaet\xc3\xb3n old school", "Ad-lib de reggaetón clásico: room corto y eco seco", "reggaeton,clasico,room")
        .eq (1.2f, 1.5f, 1.0f).comp (C::vca, 7, 5, 3, 90).harm (H::tube, 0.3f).rev (RV::room, 0.15f, 0.9f).dly (D::analog, 4, 0.15f, 0.25f, 0.5f).stereo (1.2f));
    p.push_back (R (V::adlib, "Ad-lib Bendici\xc3\xb3n", "Est\xc3\xa9tica La Bendici\xc3\xb3n", "Ad-lib de estribillo con plate y eco con puntillo", "reggaeton,plate,eco")
        .eq (1.3f, 2.0f, 2.0f).comp (C::vca, 6, 4, 3, 110).harm (H::tube, 0.25f).rev (RV::plate, 0.2f, 1.4f).dly (D::digital, 5, 0.2f, 0.3f, 0.6f).stereo (1.4f));
    p.push_back (R (V::adlib, "Ad-lib Factory", "La Obsesi\xc3\xb3n / Factory", "Ad-lib brillante y delante, con eco corto", "brillante,delante,reggaeton")
        .eq (1.3f, 2.5f, 3.0f).comp (C::fet, 7, 6, 1.5f, 80).harm (H::exciter, 0.35f).rev (RV::plate, 0.12f, 1.0f).dly (D::digital, 2, 0.15f, 0.2f, 0.7f).stereo (1.3f));
    p.push_back (R (V::adlib, "Ad-lib Underground Room", "Reggaet\xc3\xb3n underground", "Crudo y sucio, como grabado en el cuarto", "crudo,sucio,room")
        .eq (1.1f, 1.0f, -0.5f, 1.0f).comp (C::fet, 8, 6, 2, 90).harm (H::transformer, 0.5f).rev (RV::room, 0.18f, 0.8f).stereo (1.1f));
    p.push_back (R (V::adlib, "Ad-lib Reggaet\xc3\xb3n Pop", "Reggaet\xc3\xb3n comercial", "Ad-lib pulido de reggaetón comercial", "pulido,comercial")
        .eq (1.3f, 2.0f, 3.0f).comp (C::vca, 6, 4, 3, 100).harm (H::exciter, 0.4f).rev (RV::room, 0.12f, 0.8f).dly (D::pingpong, 4, 0.15f, 0.3f, 0.6f).stereo (1.5f, 0.15f));
    p.push_back (R (V::adlib, "Ad-lib Boom Bap Hype", "Hip-hop cl\xc3\xa1sico", "Hype man de los 90: seco, con cinta", "hype,seco,90s")
        .eq (1.2f, 1.5f, 0.0f, 0.5f).comp (C::fet, 9, 8, 1, 80).harm (H::tape, 0.45f).rev (RV::ambience, 0.1f, 0.4f).stereo (1.0f));
    p.push_back (R (V::adlib, "Ad-lib Melodic Rap", "Rap mel\xc3\xb3" "dico", "Ad-lib de rap melódico con eco de negras", "rap,melodico,eco")
        .eq (1.3f, 1.5f, 2.0f).comp (C::vca, 6, 4, 4, 120).harm (H::tape, 0.25f).rev (RV::plate, 0.18f, 1.5f).dly (D::digital, 7, 0.18f, 0.35f, 0.6f).stereo (1.3f));
    p.push_back (R (V::adlib, "Ad-lib Modern Hip-hop", "Hip-hop contempor\xc3\xa1neo", "Cámara media y color de válvula", "camara,valvula")
        .eq (1.2f, 1.5f, 1.5f).comp (C::vca, 6, 4, 4, 110).harm (H::tube, 0.3f).rev (RV::chamber, 0.15f, 1.3f).dly (D::analog, 5, 0.12f, 0.3f, 0.6f).stereo (1.3f));
    p.push_back (R (V::respuesta, "Respuesta Call", "Street rap", "Respuesta al verso, seca y contundente", "respuesta,seco")
        .eq (1.2f, 2.0f, 1.0f).comp (C::fet, 8, 6, 2, 90).harm (H::transformer, 0.35f).rev (RV::room, 0.1f, 0.7f).stereo (1.2f));
    p.push_back (R (V::respuesta, "Respuesta Echo", "Trap mel\xc3\xb3" "dico", "Respuesta que se queda en el aire con eco", "respuesta,eco")
        .eq (1.3f, 1.0f, 2.5f).comp (C::opto, 5, 4, 10, 200).noHarm().rev (RV::plate, 0.2f, 1.6f).dly (D::pingpong, 7, 0.25f, 0.4f, 0.3f).stereo (1.5f).autom (0.7f, 0.3f));
    p.push_back (R (V::chop, "Chop Tight", "Electr\xc3\xb3nica", "Chop vocal seco y rítmico", "chop,seco,ritmo")
        .eq (1.3f, 2.0f, 2.0f).comp (C::fet, 7, 6, 0.5f, 60).harm (H::exciter, 0.3f).rev (RV::ambience, 0.1f, 0.4f).stereo (1.2f));
    p.push_back (R (V::chop, "Chop Gate Bounce", "Electr\xc3\xb3nica", "Chop con puerta a corcheas", "chop,gate")
        .eq (1.3f, 1.5f, 2.5f).comp (C::vca, 6, 4, 2, 80).harm (H::exciter, 0.2f).rev (RV::plate, 0.15f, 1.0f).crea (CR::gate, 4, 0.8f).stereo (1.4f));
    p.push_back (R (V::grito, "Grito Rage", "Rage", "Grito controlado: duro pero sin pinchar", "grito,energia")
        .eq (1.2f, -1.0f, 1.0f).comp (C::fet, 10, 10, 0.5f, 60).harm (H::transformer, 0.5f).voc (1.2f, 0.8f, 0.2f).rev (RV::plate, 0.12f, 1.0f).stereo (1.3f));
    p.push_back (R (V::hablada, "Hablada Intro", "Hip-hop contempor\xc3\xa1neo", "Frase hablada clara, cercana", "hablada,cerca")
        .eq (0.9f, 1.5f, 1.0f).comp (C::vca, 5, 3, 5, 120).noHarm().voc (1.0f, 0.4f, 0.5f).rev (RV::ambience, 0.12f, 0.5f).stereo (1.0f));
    // ---------------- COROS (+22)
    p.push_back (R (V::coro, "Coro Rage Anthem", "Rage", "Coro de himno: ancho, saturado y brillante", "himno,ancho,saturado")
        .eq (1.5f, 1.5f, 3.0f, -1.5f).comp (C::fet, 7, 6, 2, 90).harm (H::transformer, 0.4f).rev (RV::plate, 0.22f, 1.6f).stereo (1.8f, 0.3f));
    p.push_back (R (V::coro, "Coro Crank Wall", "Crank", "Pared de voces distorsionadas", "pared,distorsion")
        .eq (1.6f, 2.0f, 2.0f, -2.0f).comp (C::fet, 9, 8, 1, 70).harm (H::transformer, 0.65f).rev (RV::room, 0.15f, 1.0f).stereo (1.9f, 0.35f));
    p.push_back (R (V::coro, "Coro Plugg Dream", "Plugg", "Coro de sueño: lejos, con eco de cinta", "sueño,lejos,cinta")
        .eq (1.8f, -0.5f, 3.5f, -2.5f).comp (C::opto, 4, 3, 10, 350).noHarm().rev (RV::hall, 0.4f, 4.0f, 45).dly (D::tape, 8, 0.2f, 0.45f, 0.4f).stereo (1.8f, 0.25f));
    p.push_back (R (V::coro, "Coro Melodic Hook", "Trap mel\xc3\xb3" "dico", "Estribillo melódico, ancho y unido", "estribillo,melodico")
        .eq (1.5f, 0.5f, 3.0f, -1.5f).comp (C::vca, 5, 4, 4, 130).harm (H::tape, 0.2f).rev (RV::plate, 0.22f, 1.8f).stereo (1.7f, 0.2f));
    p.push_back (R (V::coro, "Coro Commercial Lift", "Trap comercial", "Coro que levanta el estribillo sin tapar", "levanta,comercial")
        .eq (1.5f, 1.0f, 3.0f, -1.5f).comp (C::opto, 6, 4, 10, 200).harm (H::tape, 0.3f).voc (1.3f, 0.5f, 0.6f).rev (RV::hall, 0.25f, 2.2f).dly (D::digital, 7, 0.12f, 0.3f, 0.6f).stereo (1.7f, 0.2f));
    p.push_back (R (V::coro, "Coro Alt Haze", "Trap alternativo", "Coro brumoso y oscuro con cámara", "bruma,oscuro")
        .eq (1.7f, -1.0f, 1.0f, -2.0f).comp (C::opto, 5, 4, 10, 300).harm (H::tape, 0.35f, 1, -0.4f).rev (RV::chamber, 0.3f, 2.4f).stereo (1.7f, 0.25f));
    p.push_back (R (V::coro, "Coro Experimental Smear", "Trap experimental", "Coro emborronado con eco largo", "experimental,eco")
        .eq (1.8f, -1.0f, 2.0f, -2.0f).comp (C::opto, 5, 4, 10, 300).harm (H::transformer, 0.3f).rev (RV::hall, 0.35f, 3.5f).dly (D::tape, 9, 0.25f, 0.55f, 0.3f).stereo (1.9f, 0.3f));
    p.push_back (R (V::coro, "Coro Old School", "Reggaet\xc3\xb3n old school", "Coro de reggaetón clásico, compacto", "clasico,compacto")
        .eq (1.4f, 1.5f, 1.5f, -1.0f).comp (C::vca, 6, 5, 3, 100).harm (H::tube, 0.3f).rev (RV::room, 0.18f, 1.0f).stereo (1.5f, 0.15f));
    p.push_back (R (V::coro, "Coro Factory Shine", "La Obsesi\xc3\xb3n / Factory", "Coro brillante de estribillo", "brillante,estribillo")
        .eq (1.5f, 2.0f, 3.5f, -1.5f).comp (C::fet, 7, 6, 1.5f, 80).harm (H::exciter, 0.5f).rev (RV::plate, 0.14f, 0.9f).dly (D::digital, 2, 0.12f, 0.2f, 0.7f).stereo (1.7f, 0.2f));
    p.push_back (R (V::coro, "Coro Underground Room", "Reggaet\xc3\xb3n underground", "Coro crudo de sala", "crudo,sala")
        .eq (1.3f, 1.0f, 0.5f).comp (C::fet, 7, 5, 2, 90).harm (H::tape, 0.4f, 1, -0.4f).rev (RV::room, 0.25f, 1.1f).stereo (1.5f, 0.15f));
    p.push_back (R (V::coro, "Coro Moderno", "Reggaet\xc3\xb3n moderno", "Coro moderno, aire arriba y graves limpios", "moderno,aire")
        .eq (1.6f, 1.0f, 3.5f, -2.0f).comp (C::vca, 5, 4, 3, 110).harm (H::exciter, 0.25f).rev (RV::plate, 0.2f, 1.5f).dly (D::digital, 7, 0.1f, 0.25f, 0.7f).stereo (1.7f, 0.2f));
    p.push_back (R (V::coro, "Coro Reggaet\xc3\xb3n Trap", "Reggaet\xc3\xb3n + trap", "Coro híbrido: cinta y plate", "hibrido,cinta")
        .eq (1.5f, 1.0f, 2.5f, -1.5f).comp (C::vca, 6, 4, 3, 110).harm (H::tape, 0.35f).rev (RV::plate, 0.22f, 1.6f).stereo (1.7f, 0.2f));
    p.push_back (R (V::coro, "Coro 2000s Hook", "Rap de los 2000", "Estribillo de rap dosmilero, con cinta", "2000,cinta")
        .eq (1.4f, 0.5f, 0.5f).comp (C::opto, 6, 4, 10, 200).harm (H::tape, 0.4f, 1, -0.3f).rev (RV::room, 0.15f, 0.9f).stereo (1.5f, 0.15f));
    p.push_back (R (V::coro, "Coro Soul Sample", "Hip-hop cl\xc3\xa1sico", "Coro con color de sample de soul", "soul,sample,vintage")
        .eq (2.2f, 2.0f, -3.0f, -2.0f).comp (C::varimu, 6, 4, 20, 300).harm (H::tape, 0.5f, 1, -0.5f).rev (RV::chamber, 0.2f, 1.4f).stereo (1.4f));
    p.push_back (R (V::coro, "Coro Melodic Rap", "Rap mel\xc3\xb3" "dico", "Coro de rap melódico, suave", "rap,suave")
        .eq (1.5f, 0.5f, 2.5f, -1.5f).comp (C::opto, 5, 3, 10, 250).noHarm().rev (RV::hall, 0.25f, 2.4f).stereo (1.7f, 0.35f));
    p.push_back (R (V::coro, "Coro Hip-hop Chamber", "Hip-hop contempor\xc3\xa1neo", "Coro con cámara y válvula", "camara,valvula")
        .eq (1.5f, 0.5f, 1.5f, -1.0f).comp (C::varimu, 5, 4, 20, 300).harm (H::tube, 0.3f).rev (RV::chamber, 0.24f, 1.8f).stereo (1.6f, 0.2f));
    p.push_back (R (V::coro, "Coro Alt Rock", "Alternative rock", "Coro de rock: medios, saturación y sala", "rock,medios")
        .eq (1.3f, 3.0f, 1.0f).comp (C::fet, 8, 6, 2, 100).harm (H::transformer, 0.55f).rev (RV::chamber, 0.2f, 1.4f).stereo (1.6f, 0.15f));
    p.push_back (R (V::coro, "Coro Indie Haze", "Indie experimental", "Coro difuso y lejano", "difuso,lejano")
        .eq (1.7f, -1.0f, 2.0f, -2.0f).comp (C::opto, 4, 3, 10, 400).harm (H::tape, 0.3f).rev (RV::hall, 0.45f, 4.5f, 40, 0.4f).stereo (1.9f, 0.3f));
    p.push_back (R (V::coro, "Coro Hybrid Rock", "Rock + electr\xc3\xb3nica + urbano", "Coro híbrido con eco ping-pong", "hibrido,pingpong")
        .eq (1.4f, 1.5f, 2.0f).comp (C::fet, 7, 6, 2, 90).harm (H::transformer, 0.4f).rev (RV::plate, 0.18f, 1.4f).dly (D::pingpong, 5, 0.15f, 0.3f, 0.5f).stereo (1.7f, 0.2f));
    p.push_back (R (V::coro, "Coro Electro Lift", "Electr\xc3\xb3nica", "Coro electrónico ancho, con excitador", "electronica,ancho")
        .eq (1.7f, 0.5f, 4.0f, -2.0f).comp (C::clean, 6, 4, 3, 120).harm (H::exciter, 0.35f).rev (RV::hall, 0.3f, 2.5f).stereo (1.9f, 0.3f));
    p.push_back (R (V::coro, "Coro Commercial Clean", "Comercial", "Coro limpio y transparente", "limpio,transparente")
        .eq (1.5f, 0.5f, 3.0f, -1.5f).comp (C::clean, 5, 3, 5, 150).noHarm().voc (1.3f, 0.6f, 0.7f).rev (RV::chamber, 0.2f, 1.3f).stereo (1.4f));
    p.push_back (R (V::coro, "Coro Gospel Hall", "R&B", "Coro grande tipo gospel, sala amplia", "gospel,hall")
        .eq (1.5f, 0.5f, 2.5f, -1.0f).comp (C::varimu, 5, 3, 20, 350).harm (H::tube, 0.2f).rev (RV::hall, 0.35f, 3.2f, 35).stereo (1.8f, 0.25f));
    // ---------------- DOBLES Y SEGUNDAS (+14)
    p.push_back (R (V::doble, "Doble Crank Grit", "Crank", "Doble sucio que da cuerpo al verso", "doble,sucio")
        .eq (1.5f, 0.5f, 1.0f, -1.5f).comp (C::fet, 9, 8, 1, 70).harm (H::transformer, 0.55f).voc (1.5f, 0.5f, 0.9f).rev (RV::room, 0.06f, 0.6f).stereo (1.5f));
    p.push_back (R (V::doble, "Doble Plugg Soft", "Plugg", "Doble suave y abierto", "doble,suave")
        .eq (1.5f, -1.0f, 2.0f, -2.0f).comp (C::opto, 6, 4, 10, 250).noHarm().voc (1.6f, 0.5f, 0.9f).rev (RV::hall, 0.12f, 2.0f).stereo (1.7f, 0.2f));
    p.push_back (R (V::doble, "Doble Melodic Tuck", "Trap mel\xc3\xb3" "dico", "Doble escondido debajo de la principal", "doble,escondido")
        .eq (1.6f, -2.0f, 0.5f, -2.0f).comp (C::vca, 7, 5, 3, 100).noHarm().voc (1.6f, 0.5f, 0.9f).rev (RV::plate, 0.08f, 1.2f).stereo (1.6f));
    p.push_back (R (V::doble, "Doble Reggaet\xc3\xb3n", "Reggaet\xc3\xb3n moderno", "Doble de reggaetón: firme y centrado", "doble,firme")
        .eq (1.4f, 0.5f, 1.0f, -1.0f).comp (C::vca, 7, 5, 3, 100).harm (H::tube, 0.35f).voc (1.4f, 0.4f, 0.8f).rev (RV::room, 0.1f, 0.8f).stereo (1.0f));
    p.push_back (R (V::doble, "Doble Street", "Street rap", "Doble de rap callejero, muy pegado", "doble,rap,seco")
        .eq (1.3f, 1.5f, 0.0f).comp (C::fet, 10, 10, 0.5f, 70).harm (H::transformer, 0.6f).voc (1.4f, 0.5f, 0.8f).rev (RV::ambience, 0.06f, 0.4f).stereo (1.0f));
    p.push_back (R (V::doble, "Doble Rock", "Indie rock", "Doble de banda con válvula", "doble,rock")
        .eq (1.3f, 0.5f, 0.5f).comp (C::varimu, 6, 4, 20, 250).harm (H::tube, 0.35f).voc (1.3f, 0.5f, 0.7f).rev (RV::room, 0.1f, 0.9f).stereo (1.4f));
    p.push_back (R (V::doble, "Doble Lo-Fi", "Trap experimental", "Doble filtrado tipo cassette", "doble,lofi")
        .eq (2.5f, 2.0f, -4.0f, -2.0f).comp (C::fet, 8, 6, 2, 90).harm (H::tape, 0.55f, 1, -0.6f).voc (1.2f, 0.4f, 0.8f).rev (RV::room, 0.08f, 0.7f).stereo (1.3f));
    p.push_back (R (V::doble, "Doble Pop Clean", "Pop", "Doble limpio y transparente", "doble,limpio")
        .eq (1.4f, 0.0f, 2.0f, -1.5f).comp (C::clean, 6, 4, 4, 120).noHarm().voc (1.5f, 0.5f, 0.9f).rev (RV::hall, 0.1f, 1.8f).stereo (1.6f, 0.25f));
    p.push_back (R (V::segunda, "Segunda Plugg", "PluggnB", "Segunda voz aireada y lejana", "segunda,aire")
        .eq (1.6f, -0.5f, 3.0f, -2.0f).comp (C::opto, 5, 3, 10, 300).noHarm().rev (RV::hall, 0.25f, 2.8f, 35).dly (D::tape, 7, 0.12f, 0.35f, 0.5f).stereo (1.6f, 0.2f));
    p.push_back (R (V::segunda, "Segunda Reggaet\xc3\xb3n", "Est\xc3\xa9tica La Bendici\xc3\xb3n", "Segunda voz de reggaetón con plate", "segunda,reggaeton")
        .eq (1.4f, 0.5f, 2.0f, -1.0f).comp (C::vca, 6, 4, 3, 110).harm (H::tube, 0.25f).rev (RV::plate, 0.18f, 1.4f).stereo (1.4f));
    p.push_back (R (V::segunda, "Segunda Dark", "Dark Trap", "Segunda voz grave y oscura", "segunda,oscuro")
        .eq (1.4f, -1.5f, -1.0f).comp (C::opto, 6, 4, 10, 300).harm (H::tube, 0.3f, 1, -0.5f).rev (RV::hall, 0.25f, 3.0f, 30, 0.35f).stereo (1.5f));
    p.push_back (R (V::segunda, "Segunda R&B", "R&B", "Segunda voz de R&B, sedosa", "segunda,seda")
        .eq (1.5f, -0.5f, 3.0f, -1.0f).comp (C::opto, 5, 3, 10, 250).noHarm().voc (1.2f, 0.6f, 0.6f).rev (RV::hall, 0.22f, 2.2f).stereo (1.5f, 0.15f));
    p.push_back (R (V::segunda, "Segunda Rap 2000", "Rap de los 2000", "Segunda voz de rap con cinta", "segunda,cinta")
        .eq (1.3f, 0.5f, 0.0f).comp (C::opto, 6, 4, 10, 200).harm (H::tape, 0.35f).rev (RV::chamber, 0.18f, 1.3f).dly (D::tape, 5, 0.12f, 0.3f, 0.5f).stereo (1.3f));
    p.push_back (R (V::segunda, "Segunda Electro", "Electr\xc3\xb3nica", "Segunda voz brillante con delay", "segunda,delay")
        .eq (1.5f, 1.0f, 3.5f, -1.0f).comp (C::clean, 6, 4, 3, 110).harm (H::exciter, 0.3f).rev (RV::plate, 0.18f, 1.6f).dly (D::digital, 5, 0.18f, 0.35f, 0.6f).stereo (1.6f));
    // ---------------- ARMONÍAS (+14)
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Rage Air", "Rage", "Armonía brillante y abierta sobre el beat", "armonia,brillante")
        .eq (1.5f, 0.5f, 3.5f, -1.5f).comp (C::vca, 6, 4, 3, 110).harm (H::transformer, 0.3f).rev (RV::plate, 0.22f, 1.6f).stereo (1.8f, 0.25f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Plugg Tape", "Plugg", "Armonía con cinta y eco largo", "armonia,cinta")
        .eq (1.6f, -1.0f, 3.0f, -2.0f).comp (C::opto, 4, 3, 10, 300).harm (H::tape, 0.25f).rev (RV::hall, 0.3f, 3.0f, 35).dly (D::tape, 7, 0.2f, 0.45f, 0.4f).stereo (1.7f, 0.2f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Dark Choir", "Dark Trap", "Armonía de coro oscuro", "armonia,oscuro,coro")
        .eq (1.7f, -2.0f, 0.0f, -2.0f).comp (C::opto, 5, 4, 10, 350).harm (H::tube, 0.25f, 1, -0.5f).rev (RV::hall, 0.4f, 4.2f, 40, 0.3f).stereo (1.8f, 0.3f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Melodic", "Trap mel\xc3\xb3" "dico", "Armonía melódica, unida a la principal", "armonia,unida")
        .eq (1.5f, 0.0f, 2.5f, -1.5f).comp (C::vca, 5, 4, 4, 130).noHarm().rev (RV::plate, 0.22f, 1.8f).stereo (1.6f, 0.2f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Commercial", "Trap comercial", "Armonía pulida y brillante", "armonia,pulida")
        .eq (1.5f, 0.5f, 3.5f, -1.5f).comp (C::vca, 6, 4, 3, 110).harm (H::exciter, 0.3f).voc (1.2f, 0.6f, 0.6f).rev (RV::hall, 0.24f, 2.0f).dly (D::digital, 7, 0.1f, 0.3f, 0.6f).stereo (1.7f, 0.25f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Reggaet\xc3\xb3n", "Reggaet\xc3\xb3n comercial", "Armonía de estribillo de reggaetón", "armonia,reggaeton")
        .eq (1.5f, 1.0f, 3.0f, -1.5f).comp (C::vca, 6, 4, 3, 110).harm (H::exciter, 0.25f).rev (RV::plate, 0.18f, 1.4f).stereo (1.7f, 0.2f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Old School", "Reggaet\xc3\xb3n old school", "Armonía clásica, room corto", "armonia,clasica")
        .eq (1.4f, 1.0f, 1.5f, -1.0f).comp (C::vca, 6, 5, 3, 100).harm (H::tube, 0.3f).rev (RV::room, 0.18f, 1.0f).stereo (1.5f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Soul", "Hip-hop cl\xc3\xa1sico", "Armonía soul con cinta", "armonia,soul")
        .eq (1.8f, 1.5f, -2.0f, -1.5f).comp (C::varimu, 5, 4, 20, 300).harm (H::tape, 0.45f, 1, -0.4f).rev (RV::chamber, 0.22f, 1.5f).stereo (1.4f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Melodic Rap", "Rap mel\xc3\xb3" "dico", "Armonía suave para rap melódico", "armonia,rap")
        .eq (1.5f, 0.0f, 2.5f, -1.5f).comp (C::opto, 5, 3, 10, 250).harm (H::tape, 0.25f).rev (RV::chamber, 0.2f, 1.6f).dly (D::digital, 7, 0.1f, 0.3f, 0.6f).stereo (1.6f, 0.2f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Indie", "Indie rock", "Armonía indie con válvula y sala", "armonia,indie")
        .eq (1.4f, 0.5f, 1.0f).comp (C::varimu, 5, 4, 20, 250).harm (H::tube, 0.35f).rev (RV::room, 0.2f, 1.2f).stereo (1.5f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Alt Rock", "Alternative rock", "Armonía de rock con medios", "armonia,rock")
        .eq (1.3f, 2.0f, 1.0f).comp (C::fet, 7, 6, 2, 100).harm (H::transformer, 0.4f).rev (RV::chamber, 0.18f, 1.4f).stereo (1.5f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Hybrid", "Rock + electr\xc3\xb3nica + urbano", "Armonía híbrida con eco ping-pong", "armonia,hibrida")
        .eq (1.5f, 1.0f, 2.5f, -1.0f).comp (C::fet, 6, 5, 2, 100).harm (H::transformer, 0.35f).rev (RV::plate, 0.2f, 1.6f).dly (D::pingpong, 5, 0.15f, 0.35f, 0.5f).stereo (1.7f, 0.2f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Pop Air", "Comercial", "Armonía pop con mucho aire", "armonia,pop,aire")
        .eq (1.6f, 0.0f, 4.0f, -2.0f).comp (C::clean, 5, 3, 5, 150).harm (H::exciter, 0.4f).voc (1.3f, 0.6f, 0.7f).rev (RV::hall, 0.28f, 2.6f, 30).stereo (1.9f, 0.35f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Ethereal", "Experimental", "Armonía etérea, casi un pad", "armonia,eterea,pad")
        .eq (2.0f, -2.0f, 3.0f, -3.0f).comp (C::opto, 4, 3, 10, 400).noHarm().rev (RV::hall, 0.55f, 5.5f, 50, 0.45f).stereo (2.0f, 0.35f));
    // ---------------- EFECTOS (+10)
    p.push_back (R (V::efecto, "FX Reverse Rage", "Rage", "Eco invertido a negras, saturado", "reverse,rage")
        .eq (1.4f, 1.5f, 2.0f).comp (C::fet, 7, 6, 2, 80).harm (H::transformer, 0.4f).rev (RV::plate, 0.15f, 1.2f).crea (CR::reverse, 7, 0.6f).stereo (1.5f));
    p.push_back (R (V::efecto, "FX Stutter 1/16T", "Trap experimental", "Repetición en tresillos de semicorchea", "stutter,tresillo")
        .eq (1.3f, 2.0f, 2.0f).comp (C::fet, 7, 6, 2, 80).harm (H::transformer, 0.35f).rev (RV::room, 0.12f, 0.8f).crea (CR::stutter, 1, 0.85f));
    p.push_back (R (V::efecto, "FX Gate Half", "Electr\xc3\xb3nica", "Puerta lenta a corcheas con plate", "gate,lento")
        .eq (1.3f, 1.0f, 2.5f).comp (C::vca, 6, 4, 3, 100).harm (H::exciter, 0.2f).rev (RV::plate, 0.25f, 2.0f).crea (CR::gate, 4, 0.7f).stereo (1.5f));
    p.push_back (R (V::efecto, "FX Radio Ghost", "Dark Trap", "Radio lejana con reverb oscura", "radio,lejos")
        .eq (3.0f, 4.0f, -5.0f, -3.0f).comp (C::fet, 9, 8, 1, 70).harm (H::transformer, 0.55f).rev (RV::hall, 0.35f, 3.5f, 40, 0.3f).stereo (1.5f));
    p.push_back (R (V::efecto, "FX Tape Echo Throw", "Rap de los 2000", "Eco de cinta para remates de frase", "cinta,eco,remate")
        .eq (1.4f, 0.5f, -0.5f).comp (C::opto, 5, 4, 10, 200).harm (H::tape, 0.45f).rev (RV::room, 0.1f, 0.8f).dly (D::tape, 7, 0.4f, 0.55f, 0.4f).autom (0.8f, 0.2f));
    p.push_back (R (V::efecto, "FX Analog Dub", "Reggaet\xc3\xb3n underground", "Eco analógico tipo dub", "dub,analogico")
        .eq (1.5f, 1.0f, -1.0f).comp (C::vca, 6, 4, 3, 100).harm (H::tube, 0.35f).rev (RV::chamber, 0.15f, 1.5f).dly (D::analog, 5, 0.4f, 0.65f, 0.3f).stereo (1.4f));
    p.push_back (R (V::introOutro, "Intro Big Hall", "R&B", "Intro con mucho espacio y eco a negras", "intro,hall")
        .eq (1.2f, 0.5f, 2.5f).comp (C::opto, 4, 3, 10, 300).noHarm().rev (RV::hall, 0.4f, 3.5f, 40).dly (D::digital, 7, 0.22f, 0.45f, 0.3f).stereo (1.6f, 0.2f).autom (0.3f, 0.5f));
    p.push_back (R (V::introOutro, "Outro Fade Space", "Plugg", "Outro que se pierde en el espacio", "outro,espacio")
        .eq (1.5f, -1.0f, 2.0f, -1.5f).comp (C::opto, 4, 3, 10, 400).harm (H::tape, 0.25f).rev (RV::hall, 0.55f, 5.5f, 50, 0.4f).dly (D::tape, 9, 0.25f, 0.5f, 0.2f).stereo (1.8f, 0.25f).autom (0.3f, 0.6f));
    p.push_back (R (V::ambiente, "Ambient Wash", "Experimental", "La voz como lavado ambiental", "ambiente,lavado")
        .eq (2.4f, -3.0f, 2.0f, -3.0f).comp (C::opto, 3, 3, 10, 500).noHarm().rev (RV::hall, 0.7f, 7.0f, 60, 0.35f).stereo (2.0f, 0.35f));
    p.push_back (R (V::susurro, "Whisper Close", "Trap alternativo", "Susurro cercano, sin eses molestas", "susurro,cerca")
        .eq (1.0f, 1.0f, 1.5f).comp (C::opto, 5, 4, 10, 200).noHarm().voc (1.8f, 0.4f, 0.0f).rev (RV::ambience, 0.15f, 0.5f).stereo (1.2f));

    p.push_back (R (V::efecto, "FX Doubler Spread", "Pop", "Doblador extremo: la voz ocupa todo el estéreo", "doblador,ancho")
        .eq (1.4f, 0.5f, 2.5f).comp (C::clean, 5, 3, 5, 150).noHarm().rev (RV::plate, 0.12f, 1.2f).stereo (2.0f, 0.6f));
    p.push_back (R (V::efecto, "FX Octave Down Layer", "Dark Trap", "Capa una octava abajo debajo del ad-lib (efecto, no afinación)", "pitch,octava,oscuro")
        .eq (1.3f, 1.0f, 1.0f).comp (C::fet, 7, 6, 2, 80).harm (H::tube, 0.3f).rev (RV::hall, 0.2f, 2.2f).crea (CR::pitch, 4, 0.55f).pitch (-12.0f).stereo (1.4f));
    p.push_back (R (V::textura, "FX Grain Cloud", "Experimental", "La voz deshecha en una nube de granos", "granular,textura,nube")
        .eq (1.8f, -1.0f, 2.5f, -2.0f).comp (C::opto, 4, 3, 10, 400).noHarm().rev (RV::hall, 0.4f, 4.0f, 30, 0.45f).crea (CR::granular, 5, 0.7f).stereo (1.9f, 0.2f));
    p.push_back (R (V::adlib, "FX Word Echo", "Trap mel\xc3\xb3" "dico", "Eco sólo en una de cada cuatro palabras", "eco,palabras")
        .eq (1.3f, 1.5f, 2.5f).comp (C::vca, 6, 4, 3, 100).harm (H::tape, 0.2f).rev (RV::plate, 0.16f, 1.4f).words (2, 0, 0.7f).stereo (1.4f));
    p.push_back (R (V::efecto, "FX Section Fill", "Rage", "Stutter en el último compás de cada 8: remate de sección", "stutter,seccion,transicion")
        .eq (1.3f, 2.0f, 2.0f).comp (C::fet, 7, 6, 2, 80).harm (H::transformer, 0.35f).rev (RV::plate, 0.14f, 1.1f).words (6, 1, 0.9f).stereo (1.4f));
    // ======================= AMPLIACION: voz principal y estilos de club / urbano nuevos (generado por genpresets.py)
    p.push_back (R (V::principal, "Lead Club Underground", "Tech house underground", "Voz cantada de tech house underground: cerca, c\xc3\xa1lida y con eco con puntillo que respira con el groove", "principal,club,tech house,underground,cantada,noscope,yyy891")
        .eq (1.0f, 2.0f, 2.0f, 0.5f).comp (C::vca, 6, 4, 10, 90).harm (H::tape, 0.2f, 0.6f).voc (1.1f, 0.4f, 0.3f).rev (RV::room, 0.1f, 0.9f, 30).dly (D::digital, 5, 0.1f, 0.3f, 0.85f).stereo (1.0f).autom (0.25f, 0.0f));
    p.push_back (R (V::principal, "Lead Tech House", "Tech house", "Voz principal de tech house: brillante y al frente, con plate y eco a negras que se aparta cuando canta", "principal,tech house,club,brillante")
        .eq (1.0f, 2.5f, 3.0f, 0.0f).comp (C::vca, 6, 4, 8, 90).harm (H::tape, 0.15f, 0.6f).rev (RV::plate, 0.12f, 1.2f, 40).dly (D::digital, 7, 0.1f, 0.3f, 0.85f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead House Diva", "House", "Voz de house con aire arriba, hall amplio y eco con puntillo", "principal,house,diva,aire")
        .eq (1.0f, 2.5f, 2.5f, 0.0f).comp (C::vca, 7, 4, 6, 90).harm (H::tube, 0.25f, 0.7f).rev (RV::hall, 0.2f, 2.6f, 60).dly (D::digital, 9, 0.12f, 0.4f, 0.8f).stereo (1.0f).autom (0.3f, 0.4f));
    p.push_back (R (V::principal, "Lead Deep Warm", "Deep house", "Voz c\xc3\xa1lida y suave para deep house, sin brillo agresivo", "principal,deep house,calida,suave")
        .eq (1.0f, 1.0f, 1.5f, 1.0f).comp (C::opto, 5, 3, 15, 200).harm (H::tape, 0.2f, 0.7f, -0.3f).rev (RV::hall, 0.18f, 2.4f, 45, 0.45f).dly (D::tape, 5, 0.12f, 0.35f, 0.8f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Afro Air", "Afro house", "Voz de afro house: aire, plate y eco con puntillo", "principal,afro house,aire")
        .eq (1.0f, 1.5f, 3.0f, 0.5f).comp (C::vca, 6, 4, 8, 100).harm (H::tube, 0.15f, 0.6f).rev (RV::plate, 0.14f, 1.6f, 40).dly (D::digital, 5, 0.14f, 0.35f, 0.8f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Garage Bright", "UK garage", "Voz de UK garage: brillante, r\xc3\xa1pida y con eco en tresillo", "principal,garage,brillante")
        .eq (1.1f, 2.5f, 3.5f, 0.0f).comp (C::fet, 7, 5, 4, 80).harm (H::exciter, 0.2f, 0.6f).rev (RV::plate, 0.1f, 1.0f, 30).dly (D::digital, 3, 0.1f, 0.25f, 0.85f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Jersey Bounce", "Jersey club", "Voz de jersey club: dura, presente y casi seca", "principal,jersey,club,seco")
        .eq (1.1f, 3.0f, 2.5f, 0.0f).comp (C::fet, 8, 6, 3, 70).harm (H::transformer, 0.25f, 0.7f).rev (RV::room, 0.08f, 0.7f).dly (D::digital, 4, 0.1f, 0.25f, 0.85f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Club Pop", "Club cantado", "Voz cantada de club con brillo pop, pulida pero viva", "principal,club,cantada,pop,yyy891")
        .eq (1.0f, 2.5f, 3.5f, 0.0f).comp (C::vca, 7, 4, 6, 90).harm (H::exciter, 0.2f, 0.6f).voc (1.2f, 0.5f, 0.3f).rev (RV::plate, 0.14f, 1.5f, 40).dly (D::digital, 7, 0.1f, 0.3f, 0.85f).stereo (1.0f).autom (0.25f, 0.2f));
    p.push_back (R (V::principal, "Lead Neoperreo", "Neoperreo", "Voz de neoperreo: sucia, delante y con plate corto", "principal,neoperreo,reggaeton,underground,ladiferencia2006,yung beef")
        .eq (1.1f, 2.5f, 2.0f, 0.5f).comp (C::fet, 8, 6, 3, 80).harm (H::transformer, 0.35f, 0.8f).rev (RV::plate, 0.12f, 1.2f, 30).dly (D::digital, 4, 0.12f, 0.3f, 0.8f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Trap Underground ES", "Trap underground espa\xc3\xb1ol", "Voz de trap underground espa\xc3\xb1ol: cinta, cuerpo y eco con puntillo", "principal,trap,underground,espanol,gloosito,kaydy cain")
        .eq (1.0f, 2.0f, 2.0f, 0.5f).comp (C::vca, 7, 5, 5, 90).harm (H::tape, 0.3f, 0.7f).rev (RV::plate, 0.12f, 1.3f, 35).dly (D::digital, 5, 0.12f, 0.3f, 0.8f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Jerk", "Jerk", "Voz de jerk: brillante, muy comprimida y saturada", "principal,jerk,brillante,saturada")
        .eq (1.1f, 3.0f, 3.0f, 0.0f).comp (C::fet, 9, 8, 2, 70).harm (H::transformer, 0.4f, 0.8f).rev (RV::plate, 0.1f, 1.0f).dly (D::digital, 2, 0.1f, 0.25f, 0.85f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Drill", "Drill", "Voz de drill: seca, dura y con cuerpo", "principal,drill,seco,duro")
        .eq (1.0f, 2.5f, 1.0f, 1.0f).comp (C::fet, 9, 8, 2, 70).harm (H::transformer, 0.3f, 0.8f).rev (RV::room, 0.06f, 0.6f).dly (D::digital, 4, 0.08f, 0.25f, 0.85f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Crank", "Crank", "Voz de crank: agresiva, saturada y al frente", "principal,crank,agresiva,saturada")
        .eq (1.1f, 3.0f, 2.5f, 0.0f).comp (C::fet, 10, 10, 1, 60).harm (H::transformer, 0.55f, 0.9f).rev (RV::room, 0.07f, 0.7f).dly (D::analog, 4, 0.1f, 0.25f, 0.85f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Rage", "Rage", "Voz de rage: brillante, saturada y con plate", "principal,rage,brillante")
        .eq (1.0f, 3.0f, 3.5f, 0.0f).comp (C::fet, 8, 6, 2, 80).harm (H::transformer, 0.4f, 0.8f).rev (RV::plate, 0.12f, 1.2f).dly (D::digital, 5, 0.12f, 0.3f, 0.85f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Plugg Soft", "Plugg", "Voz de plugg: suave, lejana y con eco de cinta", "principal,plugg,suave,cinta")
        .eq (1.0f, 1.0f, 3.0f, 0.0f).comp (C::opto, 5, 3, 15, 200).harm (H::tape, 0.15f, 0.6f).rev (RV::hall, 0.18f, 2.2f, 45).dly (D::tape, 7, 0.15f, 0.4f, 0.75f).stereo (1.0f).autom (0.3f, 0.2f));
    p.push_back (R (V::principal, "Lead Melodic Trap", "Trap mel\xc3\xb3" "dico", "Voz de trap mel\xc3\xb3" "dico: limpia, con aire y plate", "principal,trap,melodico,limpia")
        .eq (1.0f, 2.0f, 3.0f, 0.5f).comp (C::vca, 6, 4, 6, 100).harm (H::tape, 0.2f, 0.6f).rev (RV::plate, 0.14f, 1.6f, 40).dly (D::digital, 7, 0.12f, 0.35f, 0.85f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Reggaet\xc3\xb3n", "Reggaet\xc3\xb3n moderno", "Voz de reggaet\xc3\xb3n moderno: brillante, firme y con plate", "principal,reggaeton,brillante")
        .eq (1.0f, 2.5f, 3.0f, 0.5f).comp (C::vca, 7, 5, 5, 90).harm (H::exciter, 0.2f, 0.6f).rev (RV::plate, 0.12f, 1.2f, 35).dly (D::digital, 4, 0.1f, 0.3f, 0.85f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Dembow", "Dembow", "Voz de dembow: presente, seca y con pegada", "principal,dembow,seco")
        .eq (1.0f, 2.5f, 2.0f, 0.5f).comp (C::fet, 8, 6, 3, 80).harm (H::tube, 0.25f, 0.7f).rev (RV::room, 0.08f, 0.8f).dly (D::digital, 4, 0.08f, 0.25f, 0.85f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Rap 2000", "Rap de los 2000", "Voz de rap dosmilero: cuerpo, cinta y poco espacio", "principal,rap,2000,cinta")
        .eq (1.0f, 1.5f, 0.5f, 1.0f).comp (C::fet, 8, 6, 3, 90).harm (H::tape, 0.3f, 0.8f, -0.2f).rev (RV::room, 0.07f, 0.7f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Hyperpop", "Hyperpop / digicore", "Voz de hyperpop: muy brillante, apretada y con excitador", "principal,hyperpop,digicore,brillante")
        .eq (1.2f, 3.0f, 4.0f, -1.0f).comp (C::fet, 9, 8, 1, 60).harm (H::exciter, 0.35f, 0.8f).rev (RV::plate, 0.12f, 1.2f).dly (D::digital, 2, 0.12f, 0.25f, 0.85f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Phonk", "Phonk", "Voz de phonk: oscura, saturada y con cinta", "principal,phonk,oscura,cinta")
        .eq (1.1f, 1.0f, -1.0f, 1.0f).comp (C::fet, 9, 8, 2, 80).harm (H::tape, 0.5f, 0.9f, -0.4f).rev (RV::room, 0.1f, 0.9f).dly (D::tape, 4, 0.12f, 0.3f, 0.8f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Emo Rap", "Emo rap", "Voz de emo rap: cerca, triste y con hall largo", "principal,emo,hall,triste")
        .eq (1.0f, 1.5f, 2.5f, 0.5f).comp (C::vca, 6, 4, 6, 100).harm (H::tape, 0.25f, 0.7f).rev (RV::hall, 0.18f, 2.4f, 45).dly (D::digital, 7, 0.15f, 0.4f, 0.8f).stereo (1.0f).autom (0.3f, 0.3f));
    p.push_back (R (V::principal, "Lead R&B Silk", "R&B", "Voz de R&B sedosa, aire arriba y sin eses", "principal,rnb,seda,aire")
        .eq (1.0f, 1.5f, 3.5f, 0.5f).comp (C::opto, 6, 3, 10, 150).harm (H::tube, 0.15f, 0.6f).voc (1.2f, 0.5f, 0.3f).rev (RV::hall, 0.16f, 2.2f, 50).dly (D::digital, 8, 0.1f, 0.35f, 0.85f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Pop Clean", "Pop", "Voz pop limpia y transparente, lista para radio", "principal,pop,limpia,radio")
        .eq (1.0f, 1.5f, 3.5f, 0.0f).comp (C::clean, 5, 3, 10, 120).noHarm().voc (1.3f, 0.5f, 0.2f).rev (RV::hall, 0.14f, 2.0f, 55).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Afrobeats", "Afrobeats", "Voz de afrobeats: brillante, suave y con eco con puntillo", "principal,afrobeats,brillante")
        .eq (1.0f, 2.0f, 3.0f, 0.5f).comp (C::vca, 6, 4, 8, 90).harm (H::tube, 0.15f).rev (RV::plate, 0.12f, 1.3f).dly (D::digital, 5, 0.12f, 0.3f, 0.85f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Techno Dark", "Techno", "Voz de techno: oscura, saturada y con hall", "principal,techno,oscura")
        .eq (1.1f, 1.0f, 1.5f, 0.0f).comp (C::fet, 8, 6, 3, 80).harm (H::transformer, 0.35f, 0.8f).rev (RV::hall, 0.16f, 2.2f, 40, 0.4f).dly (D::digital, 5, 0.14f, 0.4f, 0.8f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Guaracha", "Guaracha / aleteo", "Voz de guaracha: brillante, r\xc3\xa1pida y con pegada", "principal,guaracha,aleteo,brillante")
        .eq (1.1f, 2.5f, 4.0f, -0.5f).comp (C::vca, 7, 5, 4, 90).harm (H::exciter, 0.35f, 0.7f).rev (RV::plate, 0.12f, 1.4f, 30).dly (D::pingpong, 2, 0.1f, 0.3f, 0.85f).stereo (1.0f));
    p.push_back (R (V::principal, "Lead Minimal Dry", "Minimal / rominimal", "Voz de minimal: seca y con eco largo que deja aire al groove", "principal,minimal,seca,eco")
        .eq (1.0f, 1.0f, 1.0f, 1.0f).comp (C::opto, 5, 3, 15, 150).noHarm().rev (RV::ambience, 0.04f, 0.4f).dly (D::analog, 8, 0.16f, 0.45f, 0.75f).stereo (1.0f));
    p.push_back (R (V::coro, "Coro Club Underground", "Tech house underground", "Coro de tech house underground: detr\xc3\xa1s, c\xc3\xa1lido y abierto, sin pisar la voz", "coro,club,tech house,underground,noscope,yyy891")
        .eq (1.6f, 0.5f, 2.5f, -2.0f).comp (C::opto, 5, 4, 10, 200).harm (H::tape, 0.3f, 0.8f).rev (RV::room, 0.2f, 1.2f, 30).dly (D::digital, 5, 0.12f, 0.35f, 0.6f).stereo (1.6f, 0.2f));
    p.push_back (R (V::doble, "Doble Club Underground", "Tech house underground", "Doble pegado con color de cinta, para engordar la principal", "doble,club,tech house,underground,noscope,yyy891")
        .eq (1.4f, -0.5f, 1.5f, -1.5f).comp (C::vca, 7, 5, 3, 90).harm (H::tape, 0.3f, 0.7f).voc (1.4f, 0.4f, 0.8f).rev (RV::room, 0.08f, 0.8f).stereo (1.5f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Club Underground", "Tech house underground", "Armon\xc3\xad" "a abierta con plate y eco con puntillo", "armonia,club,tech house,underground,noscope,yyy891")
        .eq (1.6f, -0.5f, 3.0f, -2.0f).comp (C::opto, 5, 3, 10, 250).harm (H::tape, 0.25f).rev (RV::plate, 0.22f, 1.6f, 30).dly (D::digital, 5, 0.12f, 0.35f, 0.5f).stereo (1.7f, 0.2f));
    p.push_back (R (V::adlib, "Ad-lib Club Underground", "Tech house underground", "Ad-lib con eco con puntillo y remate al final de frase", "adlib,club,tech house,underground,noscope,yyy891")
        .eq (1.4f, 2.0f, 2.0f, -1.0f).comp (C::fet, 7, 6, 2, 80).harm (H::tape, 0.35f).rev (RV::room, 0.12f, 0.9f).dly (D::digital, 5, 0.2f, 0.4f, 0.6f).stereo (1.4f).autom (0.5f, 0.0f));
    p.push_back (R (V::efecto, "FX Club Filter Throw", "Tech house underground", "Voz filtrada tipo radio con eco ping-pong para transiciones", "filtro,transicion,eco,club,tech house,underground,noscope,yyy891")
        .eq (2.5f, 3.0f, -3.0f, -2.5f).comp (C::fet, 8, 6, 2, 80).harm (H::transformer, 0.45f).rev (RV::plate, 0.2f, 1.6f).dly (D::pingpong, 5, 0.3f, 0.5f, 0.4f).stereo (1.5f).autom (0.8f, 0.2f));
    p.push_back (R (V::chop, "Chop Club Underground", "Tech house underground", "Chop vocal seco con puerta a corcheas", "chop,gate,club,tech house,underground,noscope,yyy891")
        .eq (1.3f, 2.0f, 2.0f).comp (C::fet, 7, 6, 1, 60).harm (H::tape, 0.3f).rev (RV::room, 0.1f, 0.6f).crea (CR::gate, 4, 0.6f));
    p.push_back (R (V::coro, "Coro Tech House", "Tech house", "Coro de tech house: ancho, brillante y con plate", "coro,tech house,club")
        .eq (1.6f, 1.0f, 3.0f, -2.0f).comp (C::vca, 6, 4, 5, 120).harm (H::tape, 0.25f).rev (RV::plate, 0.22f, 1.6f, 35).dly (D::digital, 7, 0.12f, 0.35f, 0.6f).stereo (1.7f, 0.25f));
    p.push_back (R (V::doble, "Doble Tech House", "Tech house", "Doble ancho y limpio para el estribillo", "doble,tech house,club")
        .eq (1.4f, 0.0f, 2.0f, -1.5f).comp (C::vca, 7, 5, 3, 90).harm (H::tape, 0.2f).voc (1.4f, 0.4f, 0.8f).rev (RV::plate, 0.08f, 1.0f).stereo (1.6f));
    p.push_back (R (V::adlib, "Ad-lib Tech House", "Tech house", "Ad-lib con eco con puntillo que rebota en el groove", "adlib,tech house,club")
        .eq (1.4f, 2.0f, 3.0f, -1.0f).comp (C::fet, 7, 6, 2, 80).harm (H::tape, 0.3f).rev (RV::plate, 0.15f, 1.2f).dly (D::digital, 5, 0.2f, 0.4f, 0.6f).stereo (1.5f).autom (0.5f, 0.0f));
    p.push_back (R (V::efecto, "FX Tech House Stutter", "Tech house", "Repetici\xc3\xb3n a semicorcheas para subidas", "stutter,subida,tech house,club")
        .eq (1.4f, 2.0f, 2.0f).comp (C::fet, 7, 6, 2, 80).harm (H::tape, 0.3f).rev (RV::plate, 0.14f, 1.2f).crea (CR::stutter, 2, 0.8f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Tech House", "Tech house", "Armon\xc3\xad" "a abierta y brillante con plate", "armonia,tech house,club")
        .eq (1.6f, 0.0f, 3.0f, -1.5f).comp (C::opto, 5, 3, 10, 200).harm (H::tape, 0.2f).rev (RV::plate, 0.22f, 1.8f, 35).stereo (1.8f, 0.25f));
    p.push_back (R (V::coro, "Coro House Gospel", "House", "Coro de house con sabor gospel, hall amplio", "coro,gospel,house,club")
        .eq (1.5f, 1.0f, 3.0f, -1.0f).comp (C::opto, 5, 3, 10, 250).harm (H::tube, 0.2f).rev (RV::hall, 0.3f, 2.6f, 40).stereo (1.8f, 0.25f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a House Diva", "House", "Armon\xc3\xad" "as de diva de house, aire y eco con puntillo", "armonia,diva,house,club")
        .eq (1.5f, 0.5f, 3.5f, -1.5f).comp (C::opto, 5, 3, 10, 200).harm (H::tube, 0.15f).rev (RV::hall, 0.26f, 2.4f, 40).dly (D::digital, 8, 0.1f, 0.35f, 0.6f).stereo (1.8f, 0.25f));
    p.push_back (R (V::adlib, "Ad-lib House Shout", "House", "Grito de house que se queda en la sala", "adlib,grito,house,club")
        .eq (1.3f, 2.0f, 2.5f, 0.0f).comp (C::fet, 7, 6, 2, 80).harm (H::tube, 0.3f).rev (RV::hall, 0.2f, 2.0f).dly (D::digital, 8, 0.15f, 0.4f, 0.6f).stereo (1.4f).autom (0.5f, 0.2f));
    p.push_back (R (V::doble, "Doble House", "House", "Doble suave con hall", "doble,house,club")
        .eq (1.4f, 1.0f, 1.5f, -1.0f).comp (C::vca, 7, 5, 3, 100).harm (H::tube, 0.25f).voc (1.4f, 0.4f, 0.8f).rev (RV::room, 0.1f, 1.0f).stereo (1.5f));
    p.push_back (R (V::coro, "Coro Deep Haze", "Deep house", "Coro brumoso y c\xc3\xa1lido, lejos", "coro,bruma,deep house,club")
        .eq (1.7f, -1.0f, 2.0f, -2.0f).comp (C::opto, 4, 3, 15, 300).harm (H::tape, 0.25f, 1, -0.3f).rev (RV::hall, 0.35f, 3.2f, 45, 0.45f).dly (D::tape, 5, 0.15f, 0.4f, 0.5f).stereo (1.8f, 0.25f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Deep Warm", "Deep house", "Armon\xc3\xad" "a c\xc3\xa1lida con cinta y hall oscuro", "armonia,calida,deep house,club")
        .eq (1.6f, -0.5f, 1.5f, -1.5f).comp (C::opto, 5, 3, 15, 300).harm (H::tape, 0.25f, 1, -0.3f).rev (RV::hall, 0.3f, 2.8f, 40, 0.45f).stereo (1.7f, 0.2f));
    p.push_back (R (V::efecto, "FX Deep Dub Echo", "Deep house", "Eco anal\xc3\xb3gico tipo dub para remates", "dub,eco,deep house,club")
        .eq (1.6f, 1.0f, -1.0f, -1.0f).comp (C::vca, 6, 4, 5, 120).harm (H::tape, 0.35f).rev (RV::chamber, 0.18f, 1.8f).dly (D::analog, 5, 0.35f, 0.6f, 0.35f).stereo (1.5f).autom (0.6f, 0.2f));
    p.push_back (R (V::coro, "Coro Afro Chant", "Afro house", "Coro de canto afro: presente, ancho y con eco", "coro,canto,afro house,club")
        .eq (1.5f, 1.5f, 2.5f, -1.0f).comp (C::vca, 6, 4, 5, 120).harm (H::tube, 0.25f).rev (RV::plate, 0.22f, 1.8f, 30).dly (D::digital, 5, 0.12f, 0.35f, 0.6f).stereo (1.7f, 0.2f));
    p.push_back (R (V::respuesta, "Respuesta Afro Call", "Afro house", "Respuesta que salta de lado a lado", "respuesta,pingpong,afro house,club")
        .eq (1.3f, 2.0f, 2.5f).comp (C::vca, 6, 4, 4, 100).harm (H::tube, 0.2f).rev (RV::plate, 0.16f, 1.4f).dly (D::pingpong, 5, 0.2f, 0.4f, 0.4f).stereo (1.4f).autom (0.6f, 0.0f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Afro", "Afro house", "Armon\xc3\xad" "a abierta y suave", "armonia,afro house,club")
        .eq (1.5f, 0.5f, 3.0f, -1.5f).comp (C::opto, 5, 3, 10, 200).harm (H::tube, 0.15f).rev (RV::plate, 0.22f, 1.8f, 35).stereo (1.7f, 0.2f));
    p.push_back (R (V::chop, "Chop Minimal", "Minimal / rominimal", "Chop seco con eco largo y puerta ligera", "chop,eco,minimal,rominimal,club")
        .eq (1.4f, 1.5f, 1.5f).comp (C::fet, 7, 6, 1, 60).harm (H::tape, 0.25f).rev (RV::room, 0.1f, 0.6f).dly (D::digital, 5, 0.18f, 0.4f, 0.5f).crea (CR::gate, 2, 0.5f));
    p.push_back (R (V::adlib, "Ad-lib Minimal Echo", "Minimal / rominimal", "Ad-lib casi seco con eco que se repite", "adlib,eco,minimal,rominimal,club")
        .eq (1.4f, 1.0f, 1.5f, -1.0f).comp (C::vca, 6, 4, 4, 100).harm (H::tape, 0.25f).rev (RV::room, 0.1f, 0.8f).dly (D::digital, 5, 0.25f, 0.5f, 0.5f).stereo (1.3f).autom (0.6f, 0.0f));
    p.push_back (R (V::chop, "Chop Garage Pitch", "UK garage", "Chop de garage con capa una octava arriba", "chop,pitch,octava,garage,ukg,club")
        .eq (1.3f, 2.0f, 3.0f).comp (C::fet, 7, 6, 1, 60).harm (H::exciter, 0.25f).rev (RV::plate, 0.12f, 1.0f).crea (CR::pitch, 4, 0.5f).pitch (12.0f).stereo (1.3f));
    p.push_back (R (V::coro, "Coro Garage", "UK garage", "Coro brillante y r\xc3\xa1pido", "coro,brillante,garage,ukg,club")
        .eq (1.5f, 1.5f, 3.5f, -1.5f).comp (C::vca, 6, 4, 4, 100).harm (H::exciter, 0.25f).rev (RV::plate, 0.2f, 1.4f).stereo (1.7f, 0.2f));
    p.push_back (R (V::adlib, "Ad-lib Garage", "UK garage", "Ad-lib con eco en tresillo", "adlib,tresillo,garage,ukg,club")
        .eq (1.3f, 2.5f, 3.0f).comp (C::fet, 7, 6, 2, 80).harm (H::exciter, 0.25f).rev (RV::plate, 0.12f, 1.0f).dly (D::digital, 3, 0.15f, 0.3f, 0.6f).stereo (1.4f));
    p.push_back (R (V::chop, "Chop Jersey", "Jersey club", "Chop que se repite en tresillos", "chop,stutter,jersey,club")
        .eq (1.3f, 2.5f, 2.5f).comp (C::fet, 8, 6, 1, 60).harm (H::transformer, 0.35f).rev (RV::room, 0.08f, 0.5f).crea (CR::stutter, 1, 0.7f));
    p.push_back (R (V::adlib, "Ad-lib Jersey Bounce", "Jersey club", "Ad-lib duro con eco en tresillo", "adlib,jersey,club")
        .eq (1.3f, 3.0f, 2.5f).comp (C::fet, 8, 6, 1, 60).harm (H::transformer, 0.35f).rev (RV::room, 0.1f, 0.6f).dly (D::digital, 3, 0.15f, 0.3f, 0.7f).stereo (1.4f));
    p.push_back (R (V::coro, "Coro Jersey", "Jersey club", "Coro de grupo con pegada", "coro,grupo,jersey,club")
        .eq (1.5f, 2.0f, 2.5f, -1.5f).comp (C::fet, 7, 6, 2, 80).harm (H::transformer, 0.35f).rev (RV::room, 0.15f, 0.9f).stereo (1.7f, 0.2f));
    p.push_back (R (V::efecto, "FX Techno Tunnel", "Techno", "Voz en un t\xc3\xbanel oscuro con eco ping-pong", "tunel,oscuro,techno,club")
        .eq (2.0f, 1.0f, 0.0f, -2.0f).comp (C::fet, 8, 6, 2, 80).harm (H::transformer, 0.5f).rev (RV::hall, 0.35f, 3.5f, 40, 0.4f).dly (D::pingpong, 5, 0.25f, 0.55f, 0.4f).stereo (1.8f));
    p.push_back (R (V::textura, "Texture Techno Drone", "Techno", "Voz convertida en un drone granular", "drone,granular,techno,club")
        .eq (2.2f, -2.0f, 1.0f, -3.0f).comp (C::opto, 4, 3, 15, 400).harm (H::transformer, 0.3f).rev (RV::hall, 0.6f, 6.0f, 50, 0.35f).crea (CR::granular, 5, 0.5f).stereo (2.0f, 0.3f));
    p.push_back (R (V::coro, "Coro Club Pop", "Club cantado", "Coro de club pop: ancho y brillante", "coro,club,cantado,pop,yyy891")
        .eq (1.5f, 1.0f, 3.5f, -1.5f).comp (C::vca, 6, 4, 4, 110).harm (H::exciter, 0.25f).rev (RV::plate, 0.22f, 1.6f).dly (D::digital, 7, 0.1f, 0.3f, 0.6f).stereo (1.8f, 0.25f));
    p.push_back (R (V::doble, "Doble Club Pop", "Club cantado", "Doble pulido para el estribillo", "doble,club,cantado,pop,yyy891")
        .eq (1.4f, 0.0f, 2.5f, -1.5f).comp (C::vca, 7, 5, 3, 90).harm (H::exciter, 0.2f).voc (1.4f, 0.5f, 0.8f).rev (RV::plate, 0.1f, 1.2f).stereo (1.6f, 0.2f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Club Pop", "Club cantado", "Armon\xc3\xad" "as apiladas y brillantes", "armonia,club,cantado,pop,yyy891")
        .eq (1.6f, 0.0f, 3.0f, -2.0f).comp (C::opto, 5, 3, 10, 200).noHarm().rev (RV::plate, 0.18f, 1.4f, 30).dly (D::pingpong, 5, 0.16f, 0.35f, 0.5f).stereo (1.9f, 0.4f));
    p.push_back (R (V::adlib, "Ad-lib Club Pop", "Club cantado", "Ad-lib pulido con eco con puntillo", "adlib,club,cantado,pop,yyy891")
        .eq (1.3f, 2.0f, 3.5f).comp (C::vca, 6, 4, 3, 100).harm (H::exciter, 0.3f).rev (RV::plate, 0.16f, 1.4f).dly (D::digital, 5, 0.18f, 0.35f, 0.6f).stereo (1.5f).autom (0.5f, 0.0f));
    p.push_back (R (V::adlib, "Ad-lib Guaracha", "Guaracha / aleteo", "Ad-lib brillante con eco r\xc3\xa1pido", "adlib,guaracha,aleteo,club")
        .eq (1.4f, 3.5f, 2.0f, -1.0f).comp (C::fet, 9, 8, 1, 60).harm (H::transformer, 0.35f).rev (RV::room, 0.12f, 0.7f).dly (D::pingpong, 3, 0.18f, 0.35f, 0.6f).stereo (1.5f));
    p.push_back (R (V::coro, "Coro Guaracha", "Guaracha / aleteo", "Coro de fiesta, brillante y ancho", "coro,fiesta,guaracha,aleteo,club")
        .eq (1.5f, 2.0f, 3.0f, -1.5f).comp (C::fet, 7, 6, 2, 80).harm (H::exciter, 0.3f).rev (RV::plate, 0.18f, 1.2f).stereo (1.7f, 0.2f));
    p.push_back (R (V::adlib, "Ad-lib Neoperreo", "Neoperreo", "Ad-lib sucio con eco a corcheas", "adlib,sucio,neoperreo,reggaeton,underground,ladiferencia2006,yung beef")
        .eq (1.4f, 2.5f, 2.5f, -0.5f).comp (C::fet, 8, 6, 2, 70).harm (H::transformer, 0.45f).rev (RV::plate, 0.14f, 1.2f).dly (D::digital, 4, 0.18f, 0.3f, 0.6f).stereo (1.4f).autom (0.5f, 0.0f));
    p.push_back (R (V::coro, "Coro Neoperreo", "Neoperreo", "Coro saturado y ancho", "coro,neoperreo,reggaeton,underground,ladiferencia2006,yung beef")
        .eq (1.5f, 1.5f, 2.5f, -1.5f).comp (C::fet, 7, 6, 2, 80).harm (H::transformer, 0.4f).rev (RV::plate, 0.2f, 1.4f).stereo (1.7f, 0.25f));
    p.push_back (R (V::doble, "Doble Neoperreo", "Neoperreo", "Doble sucio pegado a la principal", "doble,neoperreo,reggaeton,underground,ladiferencia2006,yung beef")
        .eq (1.4f, 0.5f, 1.5f, -1.0f).comp (C::fet, 8, 6, 2, 80).harm (H::transformer, 0.4f).voc (1.4f, 0.5f, 0.8f).rev (RV::room, 0.08f, 0.8f).stereo (1.4f));
    p.push_back (R (V::efecto, "FX Neoperreo Radio", "Neoperreo", "Voz de radio saturada para intros y puentes", "radio,lofi,neoperreo,reggaeton,underground,ladiferencia2006,yung beef")
        .eq (3.0f, 4.0f, -4.0f, -3.0f).comp (C::fet, 9, 8, 1, 60).harm (H::transformer, 0.6f).rev (RV::plate, 0.15f, 1.2f).dly (D::digital, 4, 0.15f, 0.3f, 0.6f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Neoperreo", "Neoperreo", "Armon\xc3\xad" "a con transformador y plate", "armonia,neoperreo,reggaeton,underground,ladiferencia2006,yung beef")
        .eq (1.5f, 1.0f, 2.0f, -1.0f).comp (C::fet, 7, 6, 2, 80).harm (H::transformer, 0.45f).rev (RV::room, 0.18f, 1.0f).dly (D::pingpong, 4, 0.14f, 0.3f, 0.5f).stereo (1.6f, 0.2f));
    p.push_back (R (V::adlib, "Ad-lib Dembow", "Dembow", "Ad-lib con pegada y eco corto", "adlib,dembow,reggaeton")
        .eq (1.3f, 2.5f, 2.0f).comp (C::fet, 8, 6, 2, 70).harm (H::tube, 0.35f).rev (RV::room, 0.12f, 0.8f).dly (D::digital, 4, 0.15f, 0.25f, 0.6f).stereo (1.3f));
    p.push_back (R (V::coro, "Coro Dembow", "Dembow", "Coro de calle, compacto", "coro,dembow,reggaeton")
        .eq (1.4f, 2.0f, 2.0f, -1.0f).comp (C::fet, 7, 6, 2, 80).harm (H::tube, 0.3f).rev (RV::room, 0.16f, 1.0f).stereo (1.6f, 0.2f));
    p.push_back (R (V::respuesta, "Respuesta Dembow", "Dembow", "Respuesta seca y contundente", "respuesta,dembow,reggaeton")
        .eq (1.3f, 2.5f, 1.5f).comp (C::fet, 8, 6, 2, 80).harm (H::tube, 0.35f).rev (RV::room, 0.1f, 0.7f).stereo (1.2f));
    p.push_back (R (V::adlib, "Ad-lib Underground ES", "Trap underground espa\xc3\xb1ol", "Ad-lib con cinta y eco con puntillo", "adlib,trap,underground,espanol,gloosito,kaydy cain")
        .eq (1.3f, 2.0f, 2.0f, -0.5f).comp (C::vca, 7, 5, 3, 90).harm (H::tape, 0.4f).rev (RV::plate, 0.16f, 1.4f).dly (D::digital, 5, 0.18f, 0.35f, 0.6f).stereo (1.4f).autom (0.4f, 0.0f));
    p.push_back (R (V::doble, "Doble Underground ES", "Trap underground espa\xc3\xb1ol", "Doble con cinta, ancho", "doble,trap,underground,espanol,gloosito,kaydy cain")
        .eq (1.4f, 0.0f, 1.5f, -1.5f).comp (C::vca, 7, 5, 3, 90).harm (H::tape, 0.35f).voc (1.4f, 0.4f, 0.8f).rev (RV::plate, 0.08f, 1.0f).stereo (1.5f));
    p.push_back (R (V::coro, "Coro Underground ES", "Trap underground espa\xc3\xb1ol", "Coro detr\xc3\xa1s con plate y eco a negras", "coro,trap,underground,espanol,gloosito,kaydy cain")
        .eq (1.6f, 0.5f, 2.5f, -2.0f).comp (C::opto, 5, 4, 10, 200).harm (H::tape, 0.3f).rev (RV::plate, 0.22f, 1.8f).dly (D::digital, 7, 0.1f, 0.35f, 0.6f).stereo (1.7f, 0.25f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Underground ES", "Trap underground espa\xc3\xb1ol", "Armon\xc3\xad" "a con hall y cinta", "armonia,trap,underground,espanol,gloosito,kaydy cain")
        .eq (1.5f, 0.0f, 2.5f, -1.5f).comp (C::opto, 5, 3, 10, 250).harm (H::tape, 0.25f).rev (RV::hall, 0.25f, 2.4f, 35).stereo (1.7f, 0.2f));
    p.push_back (R (V::efecto, "FX Underground ES Tape", "Trap underground espa\xc3\xb1ol", "Eco de cinta saturado para remates", "cinta,eco,trap,underground,espanol,gloosito,kaydy cain")
        .eq (1.6f, 0.5f, -1.5f, -1.0f).comp (C::opto, 5, 4, 10, 200).harm (H::tape, 0.55f, 1, -0.4f).rev (RV::chamber, 0.18f, 1.6f).dly (D::tape, 5, 0.3f, 0.55f, 0.4f));
    p.push_back (R (V::adlib, "Ad-lib Jerk", "Jerk", "Ad-lib chill\xc3\xb3n y apretado", "adlib,jerk,trap")
        .eq (1.3f, 3.0f, 3.0f, -0.5f).comp (C::fet, 9, 8, 1, 60).harm (H::transformer, 0.5f).rev (RV::plate, 0.1f, 0.9f).dly (D::digital, 2, 0.15f, 0.25f, 0.7f).stereo (1.4f));
    p.push_back (R (V::doble, "Doble Jerk", "Jerk", "Doble saturado y pegado", "doble,jerk,trap")
        .eq (1.4f, 1.0f, 2.0f, -1.0f).comp (C::fet, 9, 8, 1, 60).harm (H::transformer, 0.45f).voc (1.4f, 0.5f, 0.8f).rev (RV::room, 0.06f, 0.6f).stereo (1.5f));
    p.push_back (R (V::coro, "Coro Jerk", "Jerk", "Coro saturado y muy abierto", "coro,jerk,trap")
        .eq (1.5f, 2.0f, 3.0f, -1.5f).comp (C::fet, 8, 6, 2, 70).harm (H::transformer, 0.45f).rev (RV::plate, 0.14f, 1.0f).stereo (1.8f, 0.3f));
    p.push_back (R (V::efecto, "FX Jerk Stutter", "Jerk", "Repetici\xc3\xb3n a semicorcheas saturada", "stutter,jerk,trap")
        .eq (1.3f, 3.0f, 2.5f).comp (C::fet, 8, 6, 1, 60).harm (H::transformer, 0.45f).rev (RV::plate, 0.1f, 0.9f).crea (CR::stutter, 2, 0.85f));
    p.push_back (R (V::adlib, "Ad-lib Hyperpop", "Hyperpop / digicore", "Ad-lib muy brillante con eco r\xc3\xa1pido", "adlib,hyperpop,digicore")
        .eq (1.4f, 3.0f, 4.0f, -1.0f).comp (C::fet, 9, 8, 1, 60).harm (H::exciter, 0.45f).rev (RV::plate, 0.14f, 1.2f).dly (D::digital, 2, 0.18f, 0.3f, 0.7f).stereo (1.6f));
    p.push_back (R (V::armonia, "Armon\xc3\xad" "a Hyperpop", "Hyperpop / digicore", "Armon\xc3\xad" "as brillantes y abiertas", "armonia,hyperpop,digicore")
        .eq (1.6f, 1.0f, 4.0f, -2.0f).comp (C::fet, 8, 6, 2, 80).harm (H::exciter, 0.4f).rev (RV::plate, 0.22f, 1.6f).stereo (1.9f, 0.3f));
    p.push_back (R (V::efecto, "FX Hyperpop Octave", "Hyperpop / digicore", "Capa una octava arriba (efecto)", "octava,pitch,hyperpop,digicore")
        .eq (1.4f, 2.0f, 3.0f).comp (C::fet, 8, 6, 2, 80).harm (H::exciter, 0.4f).rev (RV::plate, 0.15f, 1.2f).crea (CR::pitch, 4, 0.5f).pitch (12.0f).stereo (1.5f));
    p.push_back (R (V::adlib, "Ad-lib Emo", "Emo rap", "Ad-lib triste con hall y eco a negras", "adlib,emo,rap")
        .eq (1.4f, 1.0f, 2.5f, -1.0f).comp (C::vca, 6, 4, 4, 120).harm (H::tape, 0.25f).rev (RV::hall, 0.25f, 2.6f, 35).dly (D::digital, 7, 0.2f, 0.4f, 0.6f).stereo (1.5f).autom (0.4f, 0.4f));
    p.push_back (R (V::coro, "Coro Emo", "Emo rap", "Coro lejano y triste", "coro,emo,rap")
        .eq (1.7f, -0.5f, 2.5f, -2.0f).comp (C::opto, 5, 3, 10, 300).harm (H::tape, 0.25f).rev (RV::hall, 0.35f, 3.2f, 40).stereo (1.8f, 0.3f));
    p.push_back (R (V::doble, "Doble Emo", "Emo rap", "Doble suave escondido", "doble,emo,rap")
        .eq (1.5f, -0.5f, 0.5f, -1.5f).comp (C::opto, 6, 4, 10, 200).harm (H::tape, 0.25f, 1, -0.3f).voc (1.5f, 0.5f, 0.9f).rev (RV::plate, 0.1f, 1.4f).stereo (1.4f));
    p.push_back (R (V::adlib, "Ad-lib Drill", "Drill", "Ad-lib seco y duro", "adlib,drill,rap")
        .eq (1.2f, 2.5f, 1.0f, 0.5f).comp (C::fet, 9, 8, 1, 70).harm (H::transformer, 0.4f).rev (RV::room, 0.08f, 0.6f).dly (D::digital, 4, 0.12f, 0.25f, 0.7f).stereo (1.2f));
    p.push_back (R (V::doble, "Doble Drill", "Drill", "Doble muy pegado y seco", "doble,drill,rap")
        .eq (1.3f, 1.0f, 0.5f, -0.5f).comp (C::fet, 9, 8, 1, 70).harm (H::transformer, 0.4f).voc (1.4f, 0.5f, 0.8f).rev (RV::ambience, 0.06f, 0.4f).stereo (1.2f));
    p.push_back (R (V::coro, "Coro Drill Gang", "Drill", "Coro de grupo, duro", "coro,grupo,drill,rap")
        .eq (1.4f, 2.0f, 1.0f, -1.0f).comp (C::fet, 9, 8, 1, 70).harm (H::transformer, 0.45f).rev (RV::room, 0.12f, 0.8f).stereo (1.7f, 0.25f));
    p.push_back (R (V::adlib, "Ad-lib Phonk", "Phonk", "Ad-lib oscuro con cinta", "adlib,phonk,memphis")
        .eq (1.6f, 2.0f, -2.0f, -1.0f).comp (C::fet, 9, 8, 1, 70).harm (H::tape, 0.6f, 1, -0.5f).rev (RV::room, 0.14f, 0.9f).dly (D::tape, 4, 0.2f, 0.35f, 0.6f));
    p.push_back (R (V::efecto, "FX Phonk Memphis", "Phonk", "Voz de cassette de Memphis", "cassette,lofi,phonk,memphis")
        .eq (3.0f, 3.0f, -5.0f, -2.0f).comp (C::fet, 9, 8, 1, 70).harm (H::tape, 0.7f, 1, -0.6f).rev (RV::room, 0.12f, 0.8f).dly (D::tape, 4, 0.2f, 0.4f, 0.6f));
    p.push_back (R (V::coro, "Coro Afrobeats", "Afrobeats", "Coro suave y brillante", "coro,afrobeats,afro")
        .eq (1.5f, 1.5f, 3.0f, -1.0f).comp (C::vca, 6, 4, 5, 120).harm (H::tube, 0.2f).rev (RV::plate, 0.22f, 1.6f).stereo (1.7f, 0.2f));
    p.push_back (R (V::adlib, "Ad-lib Afrobeats", "Afrobeats", "Ad-lib con eco con puntillo", "adlib,afrobeats,afro")
        .eq (1.3f, 2.0f, 3.0f).comp (C::vca, 6, 4, 3, 100).harm (H::tube, 0.25f).rev (RV::plate, 0.15f, 1.2f).dly (D::digital, 5, 0.18f, 0.35f, 0.6f).stereo (1.4f).autom (0.5f, 0.0f));

    std::vector<Recipe> out;
    for (auto& r : p) out.push_back (r.r);
    return out;
}
} // namespace

PresetLibrary::PresetLibrary() : factory (makeFactory()) { loadFavourites(); }

juce::StringArray PresetLibrary::categories()
{
    return { "Voz principal", "Ad-libs", "Coros", "Dobles y segundas", T ("Armon\xc3\xad" "as"), "Efectos", "Usuario" };
}

juce::String PresetLibrary::categoryFor (VoiceType t)
{
    switch (t)
    {
        case VoiceType::adlib: case VoiceType::respuesta: case VoiceType::chop: case VoiceType::grito: case VoiceType::hablada: return "Ad-libs";
        case VoiceType::coro: return "Coros";
        case VoiceType::principal: return "Voz principal";
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
        PresetInfo p; p.name = r.name; p.category = categoryFor (r.type); p.intention = r.intention; p.tags = r.tags;
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

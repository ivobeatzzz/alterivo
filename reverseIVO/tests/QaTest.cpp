// reverseIVO — CONTROL DE CALIDAD: lo que un productor notaría en una mezcla.
//  · Nivel: el reverse llega al golpe con el mismo nivel que la voz (no se pierde ni se dispara), igual en todos los presets.
//  · Tono: al llegar al golpe el reverse está en el mismo tono que la voz (o a octavas).
//  · Tempo: duración exacta y efectos rítmicos a tempo, a varios BPM.
//  · Alineación: la subida culmina justo en el golpe.
//  · Sin clips, sin clics, sin NaN, a 44,1/48/96 kHz.
//  · En el DAW: bounce offline nada más abrir el proyecto, proyecto sin el archivo de la captura, tamaños de bloque
//    raros, pista mono, through intacto, golpe en el inicio real de la palabra.
#include "PluginProcessor.h"
#include "analysis/Onset.h"
#include "Voice.h"
#include <cstdio>
#include <chrono>

using namespace reverseivo;

struct Check { int fails = 0; void expect (bool ok, const juce::String& what) { if (! ok) { std::printf ("  FALLA %s\n", what.toRawUTF8()); ++fails; } else std::printf ("  OK    %s\n", what.toRawUTF8()); } };

static std::vector<float> mid (const std::vector<float>& l, const std::vector<float>& r)
{
    std::vector<float> m (l.size()); for (size_t i = 0; i < l.size(); ++i) m[i] = 0.5f * (l[i] + r[i]); return m;
}
// RMS máximo en ventanas de 'win' segundos dentro de [a, b)
static double maxRms (const std::vector<float>& x, int a, int b, double sr, double win, int* at = nullptr)
{
    const int w = std::max (1, (int) (win * sr)), hop = std::max (1, w / 8);
    double best = 0;
    for (int s = a; s + w <= b; s += hop)
    {
        double e = 0; for (int i = s; i < s + w; ++i) e += (double) x[(size_t) i] * x[(size_t) i];
        e = std::sqrt (e / w);
        if (e > best) { best = e; if (at != nullptr) *at = s + w / 2; }
    }
    return best;
}
static double db (double v) { return 20.0 * std::log10 (std::max (v, 1e-9)); }

// F0 por autocorrelación normalizada (60–700 Hz)
static double f0 (const std::vector<float>& x, int a, int b, double sr)
{
    const int minLag = (int) (sr / 700.0), maxLag = (int) (sr / 60.0);
    double best = 0; int bestLag = 0;
    std::vector<double> r ((size_t) maxLag + 2, 0.0);
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double s = 0, e1 = 0, e2 = 0;
        for (int i = a; i + lag < b; ++i) { s += (double) x[(size_t) i] * x[(size_t) i + lag]; e1 += (double) x[(size_t) i] * x[(size_t) i]; e2 += (double) x[(size_t) i + lag] * x[(size_t) i + lag]; }
        r[(size_t) lag] = s / std::sqrt (e1 * e2 + 1e-18);
    }
    // el primer pico que llega al 90 % del máximo (evita errores de octava hacia abajo)
    for (int lag = minLag; lag <= maxLag; ++lag) best = std::max (best, r[(size_t) lag]);
    for (int lag = minLag + 1; lag < maxLag; ++lag)
        if (r[(size_t) lag] >= 0.9 * best && r[(size_t) lag] >= r[(size_t) lag - 1] && r[(size_t) lag] >= r[(size_t) lag + 1]) { bestLag = lag; break; }
    if (bestLag == 0) return 0;
    const double y0 = r[(size_t) bestLag - 1], y1 = r[(size_t) bestLag], y2 = r[(size_t) bestLag + 1];
    const double d = 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2 + 1e-12);
    return sr / (bestLag + d);
}
static double centsToKey (double a, double b)   // distancia en cents, ignorando octavas
{
    double c = 1200.0 * std::log2 (a / b);
    c = std::fmod (c, 1200.0); if (c < -600) c += 1200; if (c > 600) c -= 1200;
    return c;
}

static int worstAt = 0;
static double worstClick (const Result& r)
{
    const int n = r.size(), w = (int) (r.sampleRate * 0.01);
    std::vector<double> dd ((size_t) n, 0.0);
    for (int i = 2; i < n; ++i)
        for (const auto* ch : { &r.l, &r.r })
            dd[(size_t) i] = std::max (dd[(size_t) i], std::abs ((double) (*ch)[(size_t) i] - 2.0 * (*ch)[(size_t) i - 1] + (*ch)[(size_t) i - 2]));
    std::vector<double> cum ((size_t) n + 1, 0.0);
    for (int i = 0; i < n; ++i) cum[(size_t) i + 1] = cum[(size_t) i] + dd[(size_t) i] * dd[(size_t) i];
    double worst = 0;
    for (int i = w + 2; i < n - w; ++i)
    {
        const double loc = std::sqrt ((cum[(size_t) (i + w)] - cum[(size_t) (i - w)]) / (2.0 * w)) + 1e-6;
        if (dd[(size_t) i] > 2e-3 && dd[(size_t) i] / loc > worst)
        {
            // un clic es AISLADO: si a 2–15 ms hay otro salto parecido, es la forma de onda de la voz (cada periodo)
            const int a = (int) (0.002 * r.sampleRate), b = (int) (0.015 * r.sampleRate);
            bool periodic = false;
            for (int k = a; k <= b && ! periodic; ++k)
                periodic = (i - k > 0 && dd[(size_t) (i - k)] > 0.5 * dd[(size_t) i]) || (i + k < n && dd[(size_t) (i + k)] > 0.5 * dd[(size_t) i]);
            if (! periodic) { worst = dd[(size_t) i] / loc; worstAt = i; }
        }
    }
    return worst;
}

struct PH : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 120.0; bool playing = true; int num = 4, den = 4;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setIsPlaying (playing); p.setPpqPosition (ppq); p.setBpm (bpm);
        juce::AudioPlayHead::TimeSignature ts; ts.numerator = num; ts.denominator = den; p.setTimeSignature (ts);
        return p;
    }
};
static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }
static void setP (ReverseIvoProcessor& p, const char* id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
static ResultPtr waitResult (ReverseIvoProcessor& p, int prevGen, int ms = 20000)
{
    for (int t = 0; t < ms; t += 20)
    {
        pump (20);
        auto r = p.getResult();
        if (r != nullptr && r->generation > prevGen && ! p.isGenerating() && ! p.hasPendingChanges()) return r;
    }
    return p.getResult();
}
// toca 'in' (stereo) por bloques; tamaño de bloque variable si bs <= 0
static juce::AudioBuffer<float> play (ReverseIvoProcessor& p, PH& ph, const std::vector<float>& inL, const std::vector<float>& inR, double ppq0, int bs, int chans = 2)
{
    const int n = (int) inL.size();
    juce::AudioBuffer<float> out (chans, n);
    const double sr = p.getSampleRate();
    juce::Random rnd (7);
    for (int s = 0; s < n;)
    {
        const int want = bs > 0 ? bs : 1 + rnd.nextInt (1500);
        const int m = std::min (want, n - s);
        juce::AudioBuffer<float> blk (chans, m);
        for (int i = 0; i < m; ++i) { blk.setSample (0, i, inL[(size_t) (s + i)]); if (chans > 1) blk.setSample (1, i, inR[(size_t) (s + i)]); }
        ph.ppq = ppq0 + (double) s / sr * ph.bpm / 60.0;
        juce::MidiBuffer midi;
        p.processBlock (blk, midi);
        for (int c = 0; c < chans; ++c) out.copyFrom (c, s, blk, c, 0, m);
        s += m;
    }
    return out;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String outDir = argc > 1 ? argv[1] : "/tmp";
    Check c;

    // ======================================================================== 1. motor: todos los presets
    std::printf ("\n== Presets × tempos × frecuencias de muestreo\n");
    std::printf ("  %-22s %5s %6s | %7s %7s %7s | %7s %6s | %6s %5s\n", "preset", "bpm", "kHz", "voz dB", "llega", "dif", "tono c", "cumbre", "pico", "ms");
    ReverseIvoProcessor host;
    std::vector<double> diffs;
    double worstMs = 0;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        const Source src = makeVoice (sr, 0.0);   // tono estable (sin caída) para poder medir la afinación
        const auto reg = analysis::detectFirstWord (src.l, src.r, sr);
        const auto sm = mid (src.l, src.r);
        const double voiceLvl = maxRms (sm, reg.start, reg.end, sr, 0.3);
        const double voiceF0 = f0 (sm, reg.start + (int) (0.06 * sr), reg.start + (int) (0.16 * sr), sr);
        for (int pi = 0; pi < (int) host.presets.all().size(); ++pi)
        {
            host.presets.apply (host.apvts, pi);
            const auto st = readSettings (host.apvts);
            for (double bpm : { 84.0, 128.0, 174.0 })
            {
                if (sr != 48000.0 && bpm != 128.0) continue;   // a 44,1 y 96 kHz basta un tempo
                const Tempo t { bpm, 4.0 };
                const auto t0 = std::chrono::steady_clock::now();
                const auto res = render (src, reg.start, reg.end, st, t);
                const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
                worstMs = std::max (worstMs, ms / (sr / 48000.0));
                const auto& name = host.presets.all()[(size_t) pi].name;
                if (res == nullptr) { c.expect (false, name + " genera"); continue; }
                const auto& r = *res;
                const int N = r.hitIndex;
                const auto m = mid (r.l, r.r);
                // nivel con el que el reverse llega al golpe (máximo de 300 ms en el último tercio)
                int at = 0;
                const double arrive = maxRms (m, std::max (0, N - (int) (N * 0.34)), N, sr, std::min (0.3, N / sr * 0.3), &at);
                const double diff = db (arrive) - db (voiceLvl);
                diffs.push_back (diff);
                // tono en el tramo final del reverse (la voz invertida, antes del golpe)
                const bool tuned = std::abs (st.pitch - std::round (st.pitch)) < 1e-3 && std::abs (st.pitchEnd - std::round (st.pitchEnd)) < 1e-3;
                // el tono se mide con la voz invertida presente (VOICE 100 %): con sólo reverb no hay altura clara que medir
                RenderSettings sv = st; sv.voice = 1.0f; sv.gapIndex = 0;
                const auto rv = render (src, reg.start, reg.end, sv, t);
                const auto mv = mid (rv->l, rv->r);
                const double rf = f0 (mv, N - (int) (0.16 * sr), N - (int) (0.04 * sr), sr);
                const double cents = rf > 0 ? centsToKey (rf * std::pow (2.0, -(st.pitch + st.pitchEnd) / 12.0), voiceF0) : 999;
                // dónde culmina la subida (máximo de 20 ms) respecto al golpe
                int top = 0; maxRms (m, 0, N, sr, 0.02, &top);
                const double topMs = (top - N) / sr * 1000.0;
                float pk = 0; bool fin = true;
                for (int i = 0; i < r.size(); ++i) { fin &= std::isfinite (r.l[(size_t) i]) && std::isfinite (r.r[(size_t) i]); pk = std::max (pk, std::max (std::abs (r.l[(size_t) i]), std::abs (r.r[(size_t) i]))); }
                const double wc = worstClick (r);
                std::printf ("  %-22s %5.0f %6.1f | %7.1f %7.1f %+7.1f | %+7.0f %6.0f | %6.2f %5.0f\n", name.toRawUTF8(), bpm, sr / 1000.0,
                             db (voiceLvl), db (arrive), diff, cents, topMs, dsp::linToDb (pk), ms);
                const juce::String tag = name + " " + juce::String (bpm, 0) + " BPM " + juce::String (sr / 1000.0, 1) + " kHz: ";
                c.expect (N == (int) std::llround (st.durationSeconds (bpm, 4.0) * sr), tag + "duración exacta a tempo");
                c.expect (fin, tag + "sin NaN");
                c.expect (pk <= dsp::dbToLin (-0.25f), tag + "pico <= -0,3 dBFS");
                c.expect (std::abs (diff) <= 2.0, tag + "llega al golpe al nivel de la voz (±2 dB): " + juce::String (diff, 1) + " dB");
                c.expect (! tuned || std::abs (cents) <= 35.0, tag + "en el tono de la voz al llegar al golpe: " + juce::String (cents, 0) + " cents");
                const double allowMs = std::max (400.0, 0.05 * N / sr * 1000.0);   // 5 % de la duración en subidas largas
                c.expect (topMs > -allowMs && topMs < 30.0, tag + "la subida culmina en el golpe: " + juce::String (topMs, 0) + " ms");
                // GAP: justo antes del golpe la subida ya se ha apartado (la voz real entra limpia)
                const int G = (int) std::llround (gapQuarters (st.gapIndex) * 60.0 / bpm * sr);
                if (G > 0)
                {
                    const double inGap = maxRms (m, N - G / 2, N, sr, 0.004);
                    std::printf ("  %-22s %5.0f %6.1f | hueco antes del golpe %.1f dB por debajo de la llegada\n", "", bpm, sr / 1000.0, db (arrive) - db (inGap));
                    c.expect (db (arrive) - db (inGap) >= 12.0, tag + "hueco limpio antes del golpe (la palabra no se oye dos veces)");
                }
                c.expect (st.crush > 0.001f || wc < 7.0, tag + "sin clics (" + juce::String (wc, 1) + " en " + juce::String (worstAt / sr, 3) + " s)");
            }
        }
    }
    {
        const double lo = *std::min_element (diffs.begin(), diffs.end()), hi = *std::max_element (diffs.begin(), diffs.end());
        std::printf ("  diferencia de nivel entre presets: %.1f dB (de %+.1f a %+.1f) · peor tiempo %.0f ms\n", hi - lo, lo, hi, worstMs);
        c.expect (hi - lo <= 3.0, "todos los presets llegan con el mismo nivel (margen <= 3 dB)");
        c.expect (worstMs < 4000.0, "ningún preset tarda más de 4 s en generarse");
    }

    // ======================================================================== 2. efectos rítmicos a tempo
    std::printf ("\n== Ritmo a tempo\n");
    {
        const double sr = 48000.0;
        const Source src = makeVoice (sr, 0.0, 0.0);   // voz sin modulación de amplitud: la envolvente es sólo el efecto
        const auto reg = analysis::detectFirstWord (src.l, src.r, sr);
        for (double bpm : { 90.0, 140.0 })
        {
            // tremolo a 1/8 sobre una subida: el periodo de la envolvente debe ser una corchea exacta
            RenderSettings s; s.durationIndex = 3; s.tremOn = true; s.tremDiv = 2; s.tremDepth = 1.0f; s.reverbOn = false; s.wash = 0.6f;
            const auto r = render (src, reg.start, reg.end, s, { bpm, 4.0 });
            const auto m = mid (r->l, r->r);
            // con profundidad 100 % el tremolo anula la señal en t = (k + 1/2) corcheas desde el inicio: se mide la energía
            // en esos instantes frente a la de los máximos (k corcheas). Si se fuera de tempo o de fase, no saldría.
            const int N = r->hitIndex;
            const double per = 30.0 / bpm * sr;
            const int w = (int) (0.004 * sr);
            double eMin = 0, eMax = 0;
            for (double t = per * 2; t + per < N - 0.2 * sr; t += per)
            {
                for (int k = -w; k <= w; ++k)
                {
                    const double a = m[(size_t) ((int) (t + per * 0.5) + k)], b = m[(size_t) ((int) t + k)];
                    eMin += a * a; eMax += b * b;
                }
            }
            const double ratio = db (std::sqrt (eMin / (eMax + 1e-18)));
            std::printf ("  tremolo 1/8 a %.0f BPM: valles %.1f dB por debajo de los picos (bloqueado al tempo y a la fase)\n", bpm, -ratio);
            c.expect (ratio < -25.0, "tremolo a tempo a " + juce::String (bpm, 0) + " BPM");

            // delay a 1/4 sin realimentación sobre un golpe de ruido: el eco aparece exactamente una negra después
            Source burst; burst.sampleRate = sr; burst.l.assign ((size_t) sr, 0.0f); burst.r.assign ((size_t) sr, 0.0f);
            { juce::Random rr (3); for (int i = 0; i < (int) (0.03 * sr); ++i) { burst.l[(size_t) (24000 + i)] = rr.nextFloat() - 0.5f; burst.r[(size_t) (24000 + i)] = burst.l[(size_t) (24000 + i)]; } }
            RenderSettings d; d.durationIndex = 2; d.fit = Fit::natural; d.delayOn = true; d.delayDiv = 5; d.delayFb = 0.0f; d.delayMix = 1.0f;
            d.reverbOn = false; d.wash = 0.0f; d.tailSeconds = 2.0f; d.ending = Ending::hard; d.volStartDb = 0.0f; d.character = 0.0f; d.transient = 0.0f;
            const auto rd = render (burst, 24000, 24000 + (int) (0.04 * sr), d, { bpm, 4.0 });
            const auto md = mid (rd->l, rd->r);
            const double q = 60.0 / bpm * sr;
            int bestL = 0; double bc = -1e18;
            for (int L = (int) (q * 0.7); L < (int) (q * 1.3); ++L)
            {
                double s2 = 0; for (size_t i = 0; i + (size_t) L < md.size(); ++i) s2 += (double) md[i] * md[i + (size_t) L];
                if (s2 > bc) { bc = s2; bestL = L; }
            }
            const double echoMs = bestL / sr * 1000.0;
            std::printf ("  delay 1/4 a %.0f BPM: eco a %.2f ms (negra = %.2f ms)\n", bpm, echoMs, 60000.0 / bpm);
            c.expect (std::abs (echoMs - 60000.0 / bpm) < 1.0, "delay a tempo a " + juce::String (bpm, 0) + " BPM");
        }
    }

    // ======================================================================== 3. en el DAW
    std::printf ("\n== En el DAW\n");
    const double sr = 48000.0;
    const Source voice = makeVoice (sr);
    juce::MemoryBlock saved;
    double wordPpq = 0;
    {
        ReverseIvoProcessor p; PH ph; p.setPlayHead (&ph);
        p.setRateAndBufferSizeDetails (sr, 512); p.prepareToPlay (sr, 512);
        p.loadPreset (p.presets.indexOf ("Crank Riser"));
        c.expect (p.apvts.getRawParameterValue (id::playInSong)->load() < 0.5f, "por defecto es una herramienta: no suena en la pista (PLAY IN SONG apagado)");
        p.toggleCapture(); pump (50);
        const double capPpq = 33.37;   // empieza en un sitio cualquiera (no en la rejilla)
        play (p, ph, voice.l, voice.r, capPpq, -1);
        ph.playing = false;
        { juce::AudioBuffer<float> b (2, 512); b.clear(); juce::MidiBuffer m; p.processBlock (b, m); }
        auto r = waitResult (p, 0);
        c.expect (r != nullptr, "captura con bloques de tamaño variable y genera");
        if (r == nullptr) return 1;
        // ¿dónde empieza la palabra en la línea de tiempo?
        const auto reg = analysis::detectFirstWord (voice.l, voice.r, sr);
        wordPpq = capPpq + reg.start / sr * 2.0;
        // herramienta: al reproducir la canción sólo pasa la voz, sin reverse
        {
            juce::MemoryBlock tmp; p.getStateInformation (tmp);
            const double h0 = juce::AudioProcessor::getXmlFromBinary (tmp.getData(), (int) tmp.getSize())->getDoubleAttribute ("hitPpq");
            ph.playing = true;
            std::vector<float> z ((size_t) (sr * 6.0), 0.0f);
            auto o = play (p, ph, z, z, h0 - 8.0, 512);
            double e = 0; for (int i = 0; i < o.getNumSamples(); ++i) e = std::max (e, (double) std::abs (o.getSample (0, i)));
            c.expect (e < 1e-9, "con PLAY IN SONG apagado, al reproducir el proyecto no suena el reverse en la pista");
            ph.playing = false;
        }
        // el WAV arrastrado empieza en un compás: soltándolo ahí, el golpe cae en la muestra de la palabra
        {
            const auto f = p.exportForDrag();
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
            juce::MemoryBlock tmp; p.getStateInformation (tmp);
            const double h0 = juce::AudioProcessor::getXmlFromBinary (tmp.getData(), (int) tmp.getSize())->getDoubleAttribute ("hitPpq");
            const int bar = p.dropBar();
            const double barPpq = (bar - 1) * 4.0;
            const juce::int64 hitInFile = rd != nullptr ? rd->metadataValues.getValue ("Cue0Offset", "-1").getLargeIntValue() : -1;
            const double bars = rd != nullptr ? (double) rd->lengthInSamples / (4 * 0.5 * sr) : 0.0;
            std::printf ("  el WAV dura %.4f compases · tempo en el chunk acid: %s BPM\n", bars, rd != nullptr ? rd->metadataValues.getValue ("acid tempo", "-").toRawUTF8() : "-");
            c.expect (std::abs (bars - std::round (bars)) < 1e-3, "el WAV dura compases enteros (el DAW no lo estira con el warp)");
            const double hitPpqFromFile = barPpq + (double) hitInFile / sr * 2.0;
            const double errMs = (hitPpqFromFile - h0) * 500.0;
            std::printf ("  arrastrar: soltar en el compás %d · el golpe cae a %.3f ms de la palabra\n", bar, errMs);
            c.expect (bar > 0 && std::abs (errMs) < 0.05, "el WAV arrastrado, soltado al principio del compás que indica, cae exacto en la palabra");
            f.deleteFile();
        }
        setP (p, id::playInSong, 1.0f);   // el resto mide la reproducción sincronizada
        p.getStateInformation (saved);
        auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
        const double hit = xml->getDoubleAttribute ("hitPpq");
        const double errMs = (hit - wordPpq) * 500.0;
        std::printf ("  golpe a %.2f ms del inicio real de la palabra\n", errMs);
        c.expect (std::abs (errMs) < 1.0, "el golpe cae en el inicio real de la palabra (no se mueve a la rejilla)");

        // through: fuera del reverse la voz sale idéntica; dentro, voz + reverse
        ph.playing = true;
        const double from = hit - 12.0;
        std::vector<float> inL ((size_t) (sr * 9.0)), inR (inL.size());
        for (size_t i = 0; i < inL.size(); ++i) { inL[i] = 0.1f * (float) std::sin (i * 0.01); inR[i] = 0.1f * (float) std::cos (i * 0.013); }
        auto out = play (p, ph, inL, inR, from, 333);
        const int hitS = (int) std::llround ((hit - from) * 0.5 * sr);
        double dryErr = 0, wetErr = 0;
        for (int i = 0; i < hitS - r->hitIndex - 10; ++i) dryErr = std::max (dryErr, (double) std::abs (out.getSample (0, i) - inL[(size_t) i]));
        for (int i = 0; i < r->hitIndex; ++i)
        {
            const int k = hitS - r->hitIndex + i;
            wetErr = std::max (wetErr, (double) std::abs (out.getSample (0, k) - inL[(size_t) k] - r->l[(size_t) i]));
        }
        std::printf ("  through: error fuera %.1e · dentro %.1e (bloques de 333)\n", dryErr, wetErr);
        c.expect (dryErr < 1e-7, "la voz de la pista pasa intacta (bit a bit) fuera del reverse");
        c.expect (wetErr < 2e-3, "voz + reverse alineado a la muestra con bloques de 333");
    }
    // ---- bounce offline nada más abrir el proyecto (Ableton/FL/Logic exportan en modo no tiempo real)
    {
        ReverseIvoProcessor q; PH ph; q.setPlayHead (&ph);
        q.setRateAndBufferSizeDetails (sr, 1024); q.prepareToPlay (sr, 1024);
        q.setNonRealtime (true);
        q.setStateInformation (saved.getData(), (int) saved.getSize());
        auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
        const double hit = xml->getDoubleAttribute ("hitPpq");
        const double from = hit - 8.0;
        std::vector<float> z ((size_t) (sr * 4.5), 0.0f);
        auto out = play (q, ph, z, z, from, 1024);   // sin dar tiempo al hilo de mensajes
        double e = 0; for (int i = 0; i < out.getNumSamples(); ++i) e = std::max (e, (double) std::abs (out.getSample (0, i)));
        c.expect (e > 0.05, "export offline justo después de abrir el proyecto: el reverse ya suena");
    }
    // ---- proyecto abierto en otro ordenador (sin el archivo de la captura)
    {
        auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
        xml->setAttribute ("sourceFile", "/no/existe/source.wav");
        juce::MemoryBlock mb; juce::AudioProcessor::copyXmlToBinary (*xml, mb);
        ReverseIvoProcessor q; PH ph; q.setPlayHead (&ph);
        q.setRateAndBufferSizeDetails (44100.0, 256); q.prepareToPlay (44100.0, 256);   // y además a otra frecuencia
        q.setStateInformation (mb.getData(), (int) mb.getSize());
        auto r = waitResult (q, 0);
        c.expect (r != nullptr && q.getSource() != nullptr, "el proyecto lleva la voz dentro: funciona sin el archivo de la captura (y a 44,1 kHz)");
        std::printf ("  tamaño del estado guardado: %.0f KB\n", saved.getSize() / 1024.0);
    }
    // ---- pista mono (Logic, FL)
    {
        ReverseIvoProcessor q; PH ph; q.setPlayHead (&ph);
        juce::AudioProcessor::BusesLayout mono; mono.inputBuses.add (juce::AudioChannelSet::mono()); mono.outputBuses.add (juce::AudioChannelSet::mono());
        c.expect (q.setBusesLayout (mono), "acepta pista mono");
        q.setRateAndBufferSizeDetails (sr, 64); q.prepareToPlay (sr, 64);
        q.setStateInformation (saved.getData(), (int) saved.getSize());
        auto r = waitResult (q, 0);
        auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
        const double hit = xml->getDoubleAttribute ("hitPpq");
        std::vector<float> z ((size_t) (sr * 3.0), 0.0f);
        auto out = play (q, ph, z, z, hit - 4.0, 64, 1);
        double e = 0; for (int i = 0; i < out.getNumSamples(); ++i) e = std::max (e, (double) std::abs (out.getSample (0, i)));
        std::printf ("  mono: %s, pico %.3f\n", r != nullptr ? "generado" : "sin resultado", e);
        c.expect (r != nullptr && e > 0.05 && e < 1.0, "suena en mono, sin pasar de 0 dBFS");
    }

    std::printf ("\n%s (%d fallos)\n", c.fails == 0 ? "TODO OK" : "HAY FALLOS", c.fails);
    return c.fails == 0 ? 0 : 1;
}

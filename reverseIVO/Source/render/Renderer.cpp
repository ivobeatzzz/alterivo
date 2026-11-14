// reverseIVO — MOTOR DE GENERACIÓN (ver Renderer.h). Orden de la cadena:
//  1. Región → limpieza de DC y fundidos de 4 ms (sin clics en los bordes).
//  2. Lavado (wash): reverb sobre la región ANTES de invertir → al invertir, la cola se convierte en la subida.
//  3. Inversión.
//  4. Ajuste a la duración: vocoder de fase (tono intacto) + lectura de velocidad variable con sinc antialias
//     para la evolución de tono. La duración sale exacta a la muestra y el final cae justo en el golpe.
//  5. Capa de textura (opcional): espectro difuminado y transportado.
//  6. Ritmo: tremolo, cortes progresivos y stutter sincronizados al tempo.
//  7. Carácter: armónicos, saturación 4x, bits, ruido, sub y aire.
//  8. Evolución del filtro (SVF TPT, sin clics) y pre-impacto.
//  9. Envolvente de volumen, final y transitorio.
// 10. Delay y reverb con su movimiento, más la cola después del golpe.
// 11. Estéreo (anchura, auto-pan, movimiento), fundido de salida, ganancia y Clip Safe.
#include "Renderer.h"
#include "../dsp/Dsp.h"
#include "../dsp/Fx.h"
#include "../dsp/Stretch.h"

namespace reverseivo
{
using namespace dsp;

namespace
{
bool cancelled (const std::atomic<bool>* c) { return c != nullptr && c->load (std::memory_order_relaxed); }

// Curva de volumen en dB (lo que también se dibuja en CUSTOMIZE)
float volumeDbAt (const RenderSettings& s, float u)
{
    const float shape = std::min (0.95f, 0.5f + 0.3f * s.volCurve + 0.06f * s.intensity);
    return lerp (s.volStartDb, s.volEndDb, shapeCurve (u, shape));
}
float pitchAt (const RenderSettings& s, float u)
{
    return s.pitch + lerp (s.pitchStart, s.pitchEnd, shapeCurve (u, s.pitchCurve));
}
void filterAt (const RenderSettings& s, float u, float& hp, float& lp)
{
    const float c = shapeCurve (u, s.filterCurve);
    hp = freqLerp (std::max (20.0f, s.hpStart), std::max (20.0f, s.hpEnd), c);
    lp = freqLerp (std::max (200.0f, s.lpStart), std::max (200.0f, s.lpEnd), c);
    if (s.moveFilter > 0.0f)   // el filtro se va abriendo a lo largo del reverse
        lp = freqLerp (std::max (250.0f, lp * (1.0f - 0.93f * s.moveFilter)), lp, shapeCurve (u, 0.65f));
}

// Lectura de velocidad variable: out[t] = in[pos(t)], pos avanza rate(t) por muestra
void varispeed (const Stereo& in, const std::vector<double>& rate, double startPos, Stereo& out)
{
    static const SincReader sinc;
    const int n = (int) rate.size(), inN = in.size();
    out.resize (n);
    double pos = startPos;
    for (int t = 0; t < n; ++t)
    {
        const double cut = std::min (1.0, 0.97 / std::max (1e-3, rate[(size_t) t]));
        if (pos > -SincReader::half && pos < inN + SincReader::half)
        {
            out.l[(size_t) t] = sinc.read (in.l.data(), inN, pos, cut);
            out.r[(size_t) t] = sinc.read (in.r.data(), inN, pos, cut);
        }
        pos += rate[(size_t) t];
    }
}

void applyWidth (float& l, float& r, float w)
{
    const float m = 0.5f * (l + r), s = 0.5f * (l - r) * w;
    l = m + s; r = m - s;
}
} // namespace

void laneCurves (const RenderSettings& s, int k, std::vector<float>& vol, std::vector<float>& pitch, std::vector<float>& filter)
{
    vol.resize ((size_t) k); pitch.resize ((size_t) k); filter.resize ((size_t) k);
    for (int i = 0; i < k; ++i)
    {
        const float u = (float) i / (float) std::max (1, k - 1);
        float fade = s.fadeIn > 0.0f ? fadeCos (u / s.fadeIn) : 1.0f;
        const float db = volumeDbAt (s, u) + linToDb (std::max (1e-6f, fade));
        vol[(size_t) i] = clamp01 ((db + 60.0f) / 66.0f);
        pitch[(size_t) i] = clamp01 ((pitchAt (s, u) + 24.0f) / 48.0f);
        float hp, lp; filterAt (s, u, hp, lp);
        filter[(size_t) i] = clamp01 (std::log (lp / 20.0f) / std::log (1000.0f));
    }
}

std::shared_ptr<Result> render (const Source& src, int a, int b, const RenderSettings& s, const Tempo& tempo, const std::atomic<bool>* cancel)
{
    const double sr = src.sampleRate;
    a = std::clamp (a, 0, src.size()); b = std::clamp (b, 0, src.size());
    if (b - a < (int) (sr * 0.01)) return nullptr;

    const double seconds = s.durationSeconds (tempo.bpm, tempo.beatsPerBar);
    const int N = std::max (1, (int) std::llround (seconds * sr));
    const double qSec = 60.0 / tempo.bpmOr120();   // una negra en segundos
    const bool anyFx = s.reverbOn || s.delayOn;
    const int T = anyFx ? std::max (0, (int) (s.tailSeconds * sr)) : (int) (0.02 * sr);

    // ---------------------------------------------------------------- 1. región
    Stereo seg; seg.resize (b - a);
    double mL = 0, mR = 0;
    for (int i = a; i < b; ++i) { mL += src.l[(size_t) i]; mR += src.r[(size_t) i]; }
    mL /= (b - a); mR /= (b - a);
    const int edge = std::min ((int) (0.004 * sr), (b - a) / 4);
    for (int i = 0; i < b - a; ++i)
    {
        float g = 1.0f;
        if (i < edge) g = fadeCos ((float) i / edge);
        if (b - a - 1 - i < edge) g = std::min (g, fadeCos ((float) (b - a - 1 - i) / edge));
        seg.l[(size_t) i] = (float) (src.l[(size_t) (a + i)] - mL) * g;
        seg.r[(size_t) i] = (float) (src.r[(size_t) (a + i)] - mR) * g;
    }

    // ---------------------------------------------------------------- 2. lavado (reverb antes de invertir)
    Stereo mat;
    const float washAmt = clamp01 (s.wash * (0.35f + 0.9f * s.intensity));
    if (washAmt > 0.001f)
    {
        const float decay = 1.4f + 3.6f * s.intensity;
        const int tail = (int) (decay * 0.85f * sr);
        Reverb rv; rv.prepare (sr, 0.85f, decay, 0.0f, 0.35f);
        mat.resize (seg.size() + tail);
        Stereo wet; wet.resize (seg.size() + tail);
        double eDry = 0, eWet = 0;
        for (int i = 0; i < mat.size(); ++i)
        {
            const float xl = i < seg.size() ? seg.l[(size_t) i] : 0.0f, xr = i < seg.size() ? seg.r[(size_t) i] : 0.0f;
            float wl, wr; rv.tick (xl, xr, wl, wr);
            wet.l[(size_t) i] = wl; wet.r[(size_t) i] = wr;
            eDry += (double) xl * xl + (double) xr * xr; eWet += (double) wl * wl + (double) wr * wr;
        }
        // el lavado suena tan fuerte como la voz (no depende de lo que mida la reverb por dentro)
        const float match = eWet > 1e-12 ? (float) std::sqrt (eDry / eWet) : 0.0f;
        for (int i = 0; i < mat.size(); ++i)
        {
            const float xl = i < seg.size() ? seg.l[(size_t) i] : 0.0f, xr = i < seg.size() ? seg.r[(size_t) i] : 0.0f;
            // fundido largo al final de la cola: al invertirla, la subida empieza de la nada
            const float tf = fadeCos ((float) (mat.size() - 1 - i) / (float) (tail * 0.3f));
            mat.l[(size_t) i] = xl * (1.0f - 0.35f * washAmt) + wet.l[(size_t) i] * match * 1.4f * washAmt * tf;
            mat.r[(size_t) i] = xr * (1.0f - 0.35f * washAmt) + wet.r[(size_t) i] * match * 1.4f * washAmt * tf;
        }
    }
    else mat = seg;
    if (cancelled (cancel)) return nullptr;

    // ---------------------------------------------------------------- 3. inversión
    std::reverse (mat.l.begin(), mat.l.end());
    std::reverse (mat.r.begin(), mat.r.end());
    const int M = mat.size();

    // Forma de la subida: una cola de reverb invertida sube 60 dB, y el oído sólo oye el final. Un compresor
    // ascendente sobre la envolvente la deja en un crecimiento audible desde el principio. Con más INTENSITY,
    // la subida es más dramática (menos compresión).
    if (washAmt > 0.001f)
    {
        std::vector<float> env ((size_t) M);
        const double c = OnePole::coefFor (6.0, sr);
        double z = 0;
        for (int i = 0; i < M; ++i) { z += c * (0.5 * (std::abs (mat.l[(size_t) i]) + std::abs (mat.r[(size_t) i])) - z); env[(size_t) i] = (float) z; }
        z = 0;
        for (int i = M - 1; i >= 0; --i) { z += c * (env[(size_t) i] - z); env[(size_t) i] = (float) z; }
        const float peakEnv = *std::max_element (env.begin(), env.end());
        const float k = 0.62f - 0.32f * s.intensity;
        if (peakEnv > 1e-7f)
            for (int i = 0; i < M; ++i)
            {
                const float rel = std::max (env[(size_t) i] / peakEnv, 1e-4f);
                const float g = std::min (dbToLin (32.0f), std::pow (rel, -k));
                mat.l[(size_t) i] *= g; mat.r[(size_t) i] *= g;
            }
    }

    // ---------------------------------------------------------------- 4. duración exacta + evolución de tono
    std::vector<double> rate ((size_t) N);
    double Ls = 0;
    for (int t = 0; t < N; ++t)
    {
        const float u = (float) t / (float) N;
        float p = pitchAt (s, u);
        if (s.microPitch > 0.0f)   // micro movimiento orgánico (dos osciladores lentos, ±25 cents)
            p += s.microPitch * 0.25f * (float) (0.6 * std::sin (2 * pi * 0.7 * t / sr) + 0.4 * std::sin (2 * pi * 1.9 * t / sr + 1.3));
        rate[(size_t) t] = std::pow (2.0, p / 12.0);
        Ls += rate[(size_t) t];
    }
    Stereo main;
    if (s.mainOn)
    {
        if (s.fit == Fit::stretch)
        {
            const int target = std::max (8, (int) std::llround (Ls));
            const Stereo st = Stretcher::stretch (mat, target, sr);
            varispeed (st, rate, 0.0, main);
        }
        else
            varispeed (mat, rate, (double) M - Ls, main);   // a su velocidad: se usa el final, silencio delante si falta
        const float g = dbToLin (s.mainDb);
        for (int t = 0; t < N; ++t) { main.l[(size_t) t] *= g; main.r[(size_t) t] *= g; applyWidth (main.l[(size_t) t], main.r[(size_t) t], s.mainWidth); }
    }
    else main.resize (N);
    if (cancelled (cancel)) return nullptr;

    // ---------------------------------------------------------------- 5. textura
    if (s.texOn)
    {
        std::vector<double> rt ((size_t) N); double Lt = 0;
        const double tp = std::pow (2.0, s.texPitch / 12.0);
        for (int t = 0; t < N; ++t) { rt[(size_t) t] = rate[(size_t) t] * tp; Lt += rt[(size_t) t]; }
        const Stereo sm = Stretcher::smear (mat, std::max (8, (int) std::llround (Lt)), sr, s.texSmear);
        Stereo tex; varispeed (sm, rt, 0.0, tex);
        const float g = dbToLin (s.texDb);
        Svf fl, fr;
        for (int t = 0; t < N; ++t)
        {
            float l = (float) fl.lowpass (tex.l[(size_t) t], 9000.0, 0.6, sr) * g;
            float r = (float) fr.lowpass (tex.r[(size_t) t], 9000.0, 0.6, sr) * g;
            applyWidth (l, r, s.texWidth);
            main.l[(size_t) t] += l; main.r[(size_t) t] += r;
        }
    }
    if (cancelled (cancel)) return nullptr;
    Stereo& x = main;   // a partir de aquí se trabaja sobre la mezcla de capas

    // ---------------------------------------------------------------- 6. ritmo
    if (s.stutterOn && s.stutterLen > 0.0f)
    {
        const int slice = std::max ((int) (0.01 * sr), (int) (divisionQuarters (s.stutterDiv) * qSec * sr));
        const int zone = std::min (N, (int) (s.stutterLen * N));
        const int z0 = N - zone;
        if (zone > slice)
        {
            const Stereo copy = x;
            const int f = std::min (slice / 8, (int) (0.003 * sr));
            for (int t = z0; t < N; ++t)
            {
                const int k = (t - z0) % slice;
                const int srcI = z0 + k;
                float g = 1.0f;
                if (k < f && t - z0 >= slice) g = fadeCos ((float) k / f);   // la primera repetición continúa sin corte
                if (slice - 1 - k < f) g = std::min (g, fadeCos ((float) (slice - 1 - k) / f));
                x.l[(size_t) t] = copy.l[(size_t) srcI] * g; x.r[(size_t) t] = copy.r[(size_t) srcI] * g;
            }
        }
    }
    if (s.cutsOn || s.tremOn)
    {
        const double fromHz = 1.0 / (divisionQuarters (s.cutsFrom) * qSec), toHz = 1.0 / (divisionQuarters (s.cutsTo) * qSec);
        const double tremHz = 1.0 / (divisionQuarters (s.tremDiv) * qSec);
        double ph = 0;
        OnePole sm; const double smc = OnePole::coefFor (220.0, sr);   // bordes de los cortes suaves (sin clics)
        for (int t = 0; t < N; ++t)
        {
            const float u = (float) t / (float) N;
            float g = 1.0f;
            if (s.cutsOn)
            {
                // la velocidad de corte acelera hacia el golpe (interpolación logarítmica de la frecuencia)
                const double hz = fromHz * std::pow (toHz / fromHz, (double) shapeCurve (u, 0.62f));
                ph += hz / sr; ph -= std::floor (ph);
                g *= ph < 0.5 ? 1.0f : 1.0f - s.cutsDepth;
            }
            float gs = (float) sm.lp (g, smc);
            if (s.tremOn)
            {
                const float d = s.tremDepth * (1.0f - s.moveTrem + s.moveTrem * u);
                gs *= 1.0f - d * (0.5f + 0.5f * (float) std::cos (2 * pi * tremHz * t / sr));
            }
            x.l[(size_t) t] *= gs; x.r[(size_t) t] *= gs;
        }
    }
    if (cancelled (cancel)) return nullptr;

    // ---------------------------------------------------------------- 7. carácter
    {
        const float harm = clamp01 (s.harmonics + 0.35f * s.character);
        const float drive = clamp01 (s.drive + 0.45f * s.character);
        const float crush = clamp01 (s.crush);
        const float noiseAmt = clamp01 (s.noise + 0.12f * s.character);
        if (harm > 0.001f)
        {
            Svf hl, hr;
            for (int t = 0; t < N; ++t)
            {
                const double el = hl.highpass (x.l[(size_t) t], 2800.0, 0.707, sr), er = hr.highpass (x.r[(size_t) t], 2800.0, 0.707, sr);
                x.l[(size_t) t] += (float) (std::tanh (el * 4.0) * 0.18 * harm);
                x.r[(size_t) t] += (float) (std::tanh (er * 4.0) * 0.18 * harm);
            }
        }
        if (drive > 0.001f)
        {
            static const Oversampler4 os;
            std::vector<float> curve ((size_t) N);
            for (int t = 0; t < N; ++t)
            {
                const float u = (float) t / (float) N;
                curve[(size_t) t] = drive * (1.0f - s.moveDrive + s.moveDrive * shapeCurve (u, 0.7f));
            }
            auto fn = [&] (double v, int i) { return saturate (v, curve[(size_t) std::min (i, N - 1)]); };
            os.process (x.l, fn); os.process (x.r, fn);
        }
        if (crush > 0.001f)
        {
            const double levels = std::pow (2.0, 15.0 - 11.0 * crush);
            for (int t = 0; t < N; ++t)
            {
                x.l[(size_t) t] = lerp (x.l[(size_t) t], (float) (std::round (x.l[(size_t) t] * levels) / levels), std::min (1.0f, crush * 1.6f));
                x.r[(size_t) t] = lerp (x.r[(size_t) t], (float) (std::round (x.r[(size_t) t] * levels) / levels), std::min (1.0f, crush * 1.6f));
            }
        }
        if (noiseAmt > 0.001f)
        {
            Pink pl (0x51ed270bu), pr (0x2a7e1f3du);
            OnePole env; const double ec = OnePole::coefFor (12.0, sr);
            Svf nhp, nhp2;
            for (int t = 0; t < N; ++t)
            {
                const double e = env.lp (0.5 * (std::abs (x.l[(size_t) t]) + std::abs (x.r[(size_t) t])), ec);
                const float k = (float) (e * 0.9 * noiseAmt);
                x.l[(size_t) t] += (float) nhp.highpass (pl.pink(), 900.0, 0.707, sr) * k;
                x.r[(size_t) t] += (float) nhp2.highpass (pr.pink(), 900.0, 0.707, sr) * k;
            }
        }
        if (std::abs (s.subDb) > 0.05f || std::abs (s.airDb) > 0.05f)
        {
            Svf a1, a2, b1, b2;
            const double gs = dbToLin (s.subDb), ga = dbToLin (s.airDb);
            for (int t = 0; t < N; ++t)
            {
                x.l[(size_t) t] = (float) b1.highShelf (a1.lowShelf (x.l[(size_t) t], 95.0, gs, sr), 9500.0, ga, sr);
                x.r[(size_t) t] = (float) b2.highShelf (a2.lowShelf (x.r[(size_t) t], 95.0, gs, sr), 9500.0, ga, sr);
            }
        }
    }
    if (cancelled (cancel)) return nullptr;

    // ---------------------------------------------------------------- 8. filtro + pre-impacto
    {
        const bool needHp = s.hpStart > 21.0f || s.hpEnd > 21.0f;
        const bool needLp = s.lpStart < 19500.0f || s.lpEnd < 19500.0f || s.moveFilter > 0.0f;
        const double q = 0.707 + 5.0 * s.resonance;
        const int pre = (int) (N * 0.22f);
        Svf hpl, hpr, lpl, lpr, pil, pir;
        for (int t = 0; t < N; ++t)
        {
            const float u = (float) t / (float) N;
            float hp, lp; filterAt (s, u, hp, lp);
            double l = x.l[(size_t) t], r = x.r[(size_t) t];
            if (needHp) { l = hpl.highpass (l, hp, q, sr); r = hpr.highpass (r, hp, q, sr); }
            if (needLp) { l = lpl.lowpass (l, lp, q, sr); r = lpr.lowpass (r, lp, q, sr); }
            if (s.preImpact > 0.0f && t >= N - pre)
            {
                // tensión final: brillo y nivel que crecen justo antes del golpe
                const float v = (float) (t - (N - pre)) / (float) std::max (1, pre);
                const double k = s.preImpact * shapeCurve (v, 0.75f);
                l = pil.highShelf (l, 4500.0, dbToLin ((float) (5.0 * k)), sr) * dbToLin ((float) (2.5 * k));
                r = pir.highShelf (r, 4500.0, dbToLin ((float) (5.0 * k)), sr) * dbToLin ((float) (2.5 * k));
            }
            x.l[(size_t) t] = (float) l; x.r[(size_t) t] = (float) r;
        }
    }

    // ---------------------------------------------------------------- 9. volumen, final y transitorio
    {
        const int endFade = std::max (8, (int) (sr * (s.ending == Ending::soft ? 0.12 : (s.ending == Ending::natural ? 0.012 : 0.0015))));
        const int trL = (int) (0.08 * sr);
        const float trDb = s.transient * (s.ending == Ending::hard ? 7.0f : (s.ending == Ending::natural ? 4.0f : 1.5f));
        for (int t = 0; t < N; ++t)
        {
            const float u = (float) t / (float) N;
            float g = dbToLin (volumeDbAt (s, u));
            if (s.fadeIn > 0.0f) g *= fadeCos (u / s.fadeIn);
            if (t >= N - trL) g *= dbToLin (trDb * std::pow ((float) (t - (N - trL)) / (float) trL, 3.0f));
            const int left = N - 1 - t;
            if (s.smartEnding) { if (left < endFade) g *= fadeCos ((float) left / (float) endFade); }
            else if (left < 8) g *= (float) left / 8.0f;
            x.l[(size_t) t] *= g; x.r[(size_t) t] *= g;
        }
    }
    if (cancelled (cancel)) return nullptr;

    // ---------------------------------------------------------------- 10. delay, reverb y cola
    Stereo out; out.resize (N + T);
    for (int t = 0; t < N; ++t) { out.l[(size_t) t] = x.l[(size_t) t]; out.r[(size_t) t] = x.r[(size_t) t]; }
    if (s.delayOn)
    {
        TempoDelay dl; dl.prepare (sr, divisionQuarters (s.delayDiv) * qSec);
        for (int t = 0; t < N + T; ++t)
        {
            const float u = std::min (1.0f, (float) t / (float) N);
            const float fb = std::min (0.92f, s.delayFb * (1.0f - s.moveDelay + s.moveDelay * shapeCurve (u, 0.6f)));
            const float il = t < N ? out.l[(size_t) t] : 0.0f, ir = t < N ? out.r[(size_t) t] : 0.0f;
            float wl, wr; dl.tick (il, ir, fb, s.pingPong, wl, wr);
            out.l[(size_t) t] = il + wl * s.delayMix; out.r[(size_t) t] = ir + wr * s.delayMix;
        }
    }
    if (s.reverbOn)
    {
        Reverb rv; rv.prepare (sr, s.revSize, s.revDecay, s.revPredelay, s.revDamp);
        for (int t = 0; t < N + T; ++t)
        {
            const float u = std::min (1.0f, (float) t / (float) N);
            const float mix = s.revMix * (1.0f - s.moveReverb + s.moveReverb * shapeCurve (u, 0.7f));
            const float il = out.l[(size_t) t], ir = out.r[(size_t) t];
            float wl, wr; rv.tick (il, ir, wl, wr);
            out.l[(size_t) t] = il * (1.0f - 0.3f * mix) + wl * 0.7f * mix;
            out.r[(size_t) t] = ir * (1.0f - 0.3f * mix) + wr * 0.7f * mix;
        }
    }
    if (cancelled (cancel)) return nullptr;

    // ---------------------------------------------------------------- 11. estéreo, fundido de la cola, ganancia
    {
        const double panHz = 1.0 / (divisionQuarters (s.panDiv) * qSec);
        const int fadeLen = std::min (T, (int) (s.outFadeMs * 0.001 * sr));
        for (int t = 0; t < N + T; ++t)
        {
            const float u = std::min (1.0f, (float) t / (float) N);
            float l = out.l[(size_t) t], r = out.r[(size_t) t];
            const float w = std::clamp (s.width * (1.0f - s.moveWidth + s.moveWidth * shapeCurve (u, 0.6f)), 0.0f, 2.0f);
            applyWidth (l, r, w);
            float pan = 0.0f;
            if (s.autoPan > 0.0f) pan += s.autoPan * 0.8f * (float) std::sin (2 * pi * panHz * t / sr);
            if (s.movePan > 0.0f) pan += s.movePan * 0.8f * (2.0f * u - 1.0f);
            pan = std::clamp (pan, -1.0f, 1.0f);
            if (std::abs (pan) > 1e-4f)
            {
                const float ang = (float) ((pan + 1.0f) * 0.25 * pi);   // potencia constante
                const float gl = std::cos (ang) * 1.41421356f, gr = std::sin (ang) * 1.41421356f;
                l *= gl; r *= gr;
            }
            if (t >= N + T - fadeLen && fadeLen > 0) { const float g = fadeCos ((float) (N + T - 1 - t) / (float) fadeLen); l *= g; r *= g; }
            out.l[(size_t) t] = l; out.r[(size_t) t] = r;
        }
        // nivel: pico a −2 dBFS (el volumen final se ajusta en tiempo real con OUTPUT)
        float pk = 0.0f;
        for (int t = 0; t < N + T; ++t) pk = std::max (pk, std::max (std::abs (out.l[(size_t) t]), std::abs (out.r[(size_t) t])));
        float g = pk > 1e-6f ? dbToLin (-2.0f) / pk : 1.0f;
        g = std::min (g, dbToLin (24.0f));
        for (int t = 0; t < N + T; ++t) { out.l[(size_t) t] *= g; out.r[(size_t) t] *= g; }
        auto res = std::make_shared<Result>();
        res->gainDb = linToDb (g);
        if (s.clipSafe)
        {
            // limitador suave sobremuestreado: nada pasa de −0,3 dBFS aunque OUTPUT suba (true peak aproximado)
            static const Oversampler4 os;
            const double ceil = dbToLin (-0.3f);
            auto soft = [ceil] (double v, int) { const double a = std::abs (v); if (a < 0.7 * ceil) return v; const double k = 0.7 * ceil; const double over = (a - k) / (ceil - k); return (v < 0 ? -1.0 : 1.0) * (k + (ceil - k) * std::tanh (over)); };
            os.process (out.l, soft); os.process (out.r, soft);
        }
        res->l = std::move (out.l); res->r = std::move (out.r);
        res->hitIndex = N; res->sampleRate = sr; res->bpm = tempo.bpm; res->seconds = seconds;
        const int bins = 1200, tot = res->size();
        res->peaks.assign ((size_t) bins, 0.0f);
        for (int t = 0; t < tot; ++t)
        {
            const int k = (int) ((int64_t) t * bins / tot);
            res->peaks[(size_t) k] = std::max (res->peaks[(size_t) k], 0.5f * (std::abs (res->l[(size_t) t]) + std::abs (res->r[(size_t) t])));
        }
        laneCurves (s, 160, res->volLane, res->pitchLane, res->filterLane);
        return res;
    }
}
} // namespace reverseivo

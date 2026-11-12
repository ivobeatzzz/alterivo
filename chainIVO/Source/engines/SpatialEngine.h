// SPATIAL ENGINE — reverb de red de retardos realimentada (FDN), delay sincronizado, ducking, doblador y anchura.
// Reverb: difusión de entrada con all-pass, FDN con matriz de Householder (sin pérdidas), amortiguación de agudos
// por línea ajustada al tiempo de caída, modulación lenta de líneas para evitar metalizado.
//   ECO: 4 líneas, sin modulación · NORMAL: 8 líneas moduladas · HIGH: 8 líneas, interpolación Hermite y más difusión
//   ULTRA: 16 líneas, Hermite.
// Tipos: PLATE (densa y brillante) · HALL (grande, más oscura) · ROOM (corta, primeras reflexiones fuertes)
//        CHAMBER (media) · AMBIENCE (casi sólo reflexiones, da cuerpo sin cola).
// La señal original (M) nunca cambia con la anchura: sólo se abre la parte lateral (S), así que es compatible en mono.
#pragma once
#include "../dsp/DspCore.h"

namespace chainivo
{
enum class RevType { plate = 0, hall, room, chamber, ambience };
enum class DlyType { digital = 0, analog, tape, pingpong };

struct SpaceSettings
{
    bool on = true;
    RevType revType = RevType::plate;
    float revMix = 0.15f, decay = 1.6f, predelay = 20.0f, tone = 0.6f;
    DlyType dlyType = DlyType::digital;
    int dlyNote = 5;
    float dlyFeedback = 0.3f, dlyMix = 0.0f, duck = 0.5f, width = 1.0f, doubler = 0.0f;
};

class SpatialEngine
{
public:
    void prepare (double sampleRate, int maxBlock, Quality q)
    {
        sr = sampleRate; quality = q;
        lines = q == Quality::eco ? 4 : (q == Quality::ultra ? 16 : 8);
        hermite = q == Quality::high || q == Quality::ultra;
        modulate = q != Quality::eco;
        const int maxLine = (int) (0.35 * sr);
        for (auto& l : fdn) l.prepare (maxLine);
        for (auto& c : diff) for (auto& a : c) a.d.prepare ((int) (0.03 * sr));
        for (auto& p : pre) p.prepare ((int) (0.25 * sr));
        for (auto& d : dly) d.prepare ((int) (4.2 * sr));
        for (auto& d : dbl) d.prepare ((int) (0.04 * sr));
        wetL.assign ((size_t) maxBlock, 0.0f); wetR.assign ((size_t) maxBlock, 0.0f);
        for (auto* s : { &revMixS, &dlyMixS, &widthS, &dblS, &fbS }) s->prepare (sr, 40.0f);
        dlyTimeS.prepare (sr, 250.0f);
        duckEnv.prepare (sr, 10.0f, 300.0f);
        first = true;
        reset();
    }
    void reset()
    {
        for (auto& l : fdn) l.reset();
        for (auto& c : diff) for (auto& a : c) a.d.reset();
        for (auto& p : pre) p.reset();
        for (auto& d : dly) d.reset();
        for (auto& d : dbl) d.reset();
        std::fill (std::begin (damp), std::end (damp), 0.0f);
        inHp.reset(); fbHp.reset(); fbLp.reset();
        lfo = 0.0; wow = 0.0; flutter = 0.0;
    }

    void process (float* const* ch, int nch, int n, const SpaceSettings& s, const TempoInfo& tempo)
    {
        if (first) { revMixS.reset (s.on ? s.revMix : 0); dlyMixS.reset (s.on ? s.dlyMix : 0); widthS.reset (s.on ? s.width : 1); dblS.reset (s.on ? s.doubler : 0); fbS.reset (s.dlyFeedback); first = false; }
        revMixS.set (s.on ? s.revMix : 0.0f); dlyMixS.set (s.on ? s.dlyMix : 0.0f);
        widthS.set (s.on ? s.width : 1.0f); dblS.set (s.on ? s.doubler : 0.0f); fbS.set (s.dlyFeedback);
        const double secs = noteBeats (s.dlyNote) * 60.0 / juce::jlimit (30.0, 300.0, tempo.bpm);
        dlyTimeS.set ((float) std::min (4.0 * sr, secs * sr));
        if (! dlyInit) { dlyTimeS.reset (dlyTimeS.target); dlyInit = true; }
        setupReverb (s);
        inHp.setup (sr, 180.0f, 0.7071f);
        const bool analogish = s.dlyType == DlyType::analog || s.dlyType == DlyType::tape;
        fbHp.setup (sr, analogish ? 220.0f : 120.0f, 0.7071f);
        fbLp.setup (sr, s.dlyType == DlyType::analog ? 3500.0f : (s.dlyType == DlyType::tape ? 5200.0f : 12000.0f), 0.7071f);

        const bool stereo = nch > 1;
        float* L = ch[0]; float* R = stereo ? ch[1] : ch[0];
        const float duckAmt = s.duck;

        for (int i = 0; i < n; ++i)
        {
            const float xl = L[i], xr = R[i], mono = 0.5f * (xl + xr);
            const float rm = revMixS.next(), dm = dlyMixS.next(), fb = fbS.next(), dt = dlyTimeS.next();

            // ---- Reverb
            float rl = 0.0f, rr = 0.0f;
            if (rm > 1.0e-4f || ! revSilent)
            {
                pre[0].push (inHp.tick (0, xl).hp); pre[1].push (inHp.tick (1, xr).hp);
                float il = pre[0].tap (predelaySamples), ir = pre[1].tap (predelaySamples);
                // Primeras reflexiones (ROOM, CHAMBER, AMBIENCE)
                float el = 0.0f, er = 0.0f;
                for (int t = 0; t < erTaps; ++t) { el += erGain[t] * pre[0].tap (predelaySamples + erDelay[t]); er += erGain[t] * pre[1].tap (predelaySamples + erDelay[t] + 7); }
                for (int a = 0; a < diffStages; ++a) { il = diff[0][(size_t) a].tick (il); ir = diff[1][(size_t) a].tick (ir); }
                processFdn (il, ir, rl, rr);
                rl = rl * tailGain + el * erLevel; rr = rr * tailGain + er * erLevel;
                const float e = std::abs (rl) + std::abs (rr);
                revSilent = rm <= 1.0e-4f && e < 1.0e-6f;
            }

            // ---- Delay
            float dl = 0.0f, dr = 0.0f;
            if (dm > 1.0e-4f || ! dlySilent)
            {
                float mod = 0.0f;
                if (s.dlyType == DlyType::analog) mod = 0.00015f * (float) sr * (float) std::sin (lfo * 0.6);
                if (s.dlyType == DlyType::tape) mod = 0.0006f * (float) sr * (float) std::sin (wow) + 0.00005f * (float) sr * (float) std::sin (flutter);
                const float d = juce::jlimit (4.0f, (float) (4.1 * sr), dt + mod);
                const float yl = hermite ? dly[0].readHermite (d) : dly[0].readLinear (d);
                const float yr = hermite ? dly[1].readHermite (d) : dly[1].readLinear (d);
                auto colour = [&] (int c, float v)
                {
                    v = fbLp.tick (c, fbHp.tick (c, v).hp).lp;
                    return analogish ? std::tanh (v * 1.2f) / 1.2f : v;
                };
                if (s.dlyType == DlyType::pingpong)
                {
                    dly[0].push (mono * dm + colour (0, yr) * fb);
                    dly[1].push (colour (1, yl) * fb);
                }
                else
                {
                    dly[0].push (xl * dm + colour (0, yl) * fb);
                    dly[1].push (xr * dm + colour (1, yr) * fb);
                }
                dl = yl; dr = yr;
                dlySilent = dm <= 1.0e-4f && std::abs (yl) + std::abs (yr) < 1.0e-6f;
            }
            else { dly[0].push (0.0f); dly[1].push (0.0f); }

            // ---- Ducking: lo húmedo baja mientras la voz suena
            const float env = duckEnv.tick (powToDb (mono * mono));
            const float duckDb = duckAmt * 15.0f * juce::jlimit (0.0f, 1.0f, (env + 42.0f) / 24.0f);
            const float dg = dbToGain (-duckDb);
            float ol = xl + dg * (rl * rm * 1.4f + dl);
            float orr = xr + dg * (rr * rm * 1.4f + dr);

            // ---- Doblador (micro-pitch: dos retardos cortos modulados, uno a cada lado)
            const float db = dblS.next();
            dbl[0].push (mono); dbl[1].push (mono);
            if (db > 1.0e-4f)
            {
                const float dL = (float) ((0.0095 + 0.0012 * std::sin (lfo * 0.43)) * sr);
                const float dR = (float) ((0.0143 + 0.0012 * std::sin (lfo * 0.31 + 1.7)) * sr);
                ol += db * 0.5f * dbl[0].readLinear (dL);
                orr += db * 0.5f * dbl[1].readLinear (dR);
            }

            // ---- Anchura M/S
            const float w = widthS.next();
            const float mm = 0.5f * (ol + orr), ss = 0.5f * (ol - orr) * w;
            ol = mm + ss; orr = mm - ss;

            L[i] = stereo ? ol : mm;
            if (stereo) R[i] = orr;

            lfo += 2.0 * juce::MathConstants<double>::pi / sr;
            wow += 2.0 * juce::MathConstants<double>::pi * 0.55 / sr;
            flutter += 2.0 * juce::MathConstants<double>::pi * 6.2 / sr;
            if (lfo > 1.0e4) lfo -= 2.0 * juce::MathConstants<double>::pi * 1000.0;
            if (wow > 1.0e4) wow -= 2.0 * juce::MathConstants<double>::pi * 1000.0;
            if (flutter > 1.0e4) flutter -= 2.0 * juce::MathConstants<double>::pi * 1000.0;
        }
        duckShown.store (dbToGain (0.0f));
    }

    int lineCount() const { return lines; }

private:
    struct Allpass
    {
        DelayLine d; int len = 1; float g = 0.6f;
        float tick (float x) { const float z = d.tap (len - 1); const float v = x + g * z; d.push (v); return z - g * v; }
    };

    void setupReverb (const SpaceSettings& s)
    {
        static const float base[16] = { 29.7f, 37.1f, 41.1f, 43.7f, 47.9f, 53.3f, 59.9f, 61.7f, 67.1f, 71.9f, 73.3f, 79.1f, 83.9f, 89.3f, 97.1f, 101.3f };
        // 1.13: calibrado midiendo el T60 real (Schroeder) frente al ajustado; ver tests
        float scale = 1.0f, decay = s.decay * 1.13f, er = 0.0f; diffStages = quality == Quality::eco ? 2 : 4;
        float diffG = 0.62f;
        erTaps = 0;
        switch (s.revType)
        {
            case RevType::plate:    scale = 0.45f; diffG = 0.72f; if (quality != Quality::eco) diffStages = 4; break;
            case RevType::hall:     scale = 1.35f; er = 0.25f; erTaps = 6; break;
            case RevType::room:     scale = 0.28f; er = 0.6f; erTaps = 8; decay = std::min (decay, 1.4f * 1.13f); break;
            case RevType::chamber:  scale = 0.65f; er = 0.4f; erTaps = 7; break;
            case RevType::ambience: scale = 0.18f; er = 0.85f; erTaps = 8; decay = std::min (decay, 0.6f * 1.13f); break;
        }
        if (quality == Quality::high || quality == Quality::ultra) diffStages = std::min (4, diffStages + 1);
        erLevel = er; tailGain = s.revType == RevType::ambience ? 0.45f : 1.0f;
        predelaySamples = (int) (juce::jlimit (0.0f, 200.0f, s.predelay) * 0.001f * (float) sr);

        static const int erMs10[8] = { 43, 71, 97, 131, 167, 211, 263, 307 };   // décimas de ms
        static const float erG[8] = { 0.8f, -0.62f, 0.55f, -0.45f, 0.38f, -0.3f, 0.24f, -0.18f };
        const float erScale = s.revType == RevType::hall ? 2.2f : (s.revType == RevType::chamber ? 1.4f : 1.0f);
        for (int t = 0; t < erTaps; ++t) { erDelay[t] = (int) (erMs10[t] * 0.1f * erScale * 0.001f * (float) sr); erGain[t] = erG[t]; }

        static const float apMs[4] = { 4.71f, 3.63f, 12.7f, 9.31f };
        for (int c = 0; c < 2; ++c)
            for (int a = 0; a < 4; ++a)
            {
                diff[(size_t) c][(size_t) a].len = std::max (1, (int) (apMs[a] * (c ? 1.07f : 1.0f) * 0.001f * (float) sr * std::max (0.5f, scale)));
                diff[(size_t) c][(size_t) a].g = diffG;
            }

        // Longitudes, ganancias de caída (T60) y amortiguación: los agudos caen antes cuanto más oscuro el TONE
        const float hfRatio = 0.25f + 0.7f * s.tone;
        for (int l = 0; l < lines; ++l)
        {
            const int idx = lines == 16 ? l : l * (16 / lines) + (l & 1);
            len[l] = std::max (8, (int) (base[std::min (15, idx)] * scale * 0.001f * (float) sr));
            const float t60 = std::max (0.1f, decay);
            fbGain[l] = std::pow (10.0f, -3.0f * (float) len[l] / (t60 * (float) sr));
            const float t60hf = std::max (0.05f, t60 * hfRatio);
            const float gHf = std::pow (10.0f, -3.0f * (float) len[l] / (t60hf * (float) sr));
            // Paso bajo de un polo cuya ganancia en Nyquist es gHf/g (el agudo cae con T60 menor)
            const float r = juce::jlimit (0.0f, 0.95f, (1.0f - gHf / fbGain[l]) / (1.0f + gHf / fbGain[l]));
            dampC[l] = r;
        }
    }

    void processFdn (float il, float ir, float& ol, float& orr)
    {
        float v[16];
        float sum = 0.0f;
        for (int l = 0; l < lines; ++l)
        {
            float d = (float) len[l];
            float y;
            if (modulate && (l & 1))
            {
                d += (float) (0.0003 * sr * std::sin (lfo * (0.3 + 0.07 * l) + l));
                y = hermite ? fdn[l].readHermite (d) : fdn[l].readLinear (d);
            }
            else y = fdn[l].tap (len[l] - 1);
            damp[l] = (1.0f - dampC[l]) * y + dampC[l] * damp[l];
            v[l] = damp[l] * fbGain[l];
            sum += v[l];
        }
        const float h = 2.0f / (float) lines * sum;
        ol = 0.0f; orr = 0.0f;
        for (int l = 0; l < lines; ++l)
        {
            const float in = (l & 1) ? ir : il;
            const float sign = (l & 2) ? -1.0f : 1.0f;
            fdn[l].push (v[l] - h + in * sign);
            if (l & 1) orr += v[l] * sign; else ol += v[l] * sign;
        }
        const float norm = 1.0f / std::sqrt ((float) lines * 0.5f);
        ol *= norm; orr *= norm;
    }

    double sr = 48000.0;
    Quality quality = Quality::normal;
    int lines = 8, diffStages = 4, predelaySamples = 0, erTaps = 0;
    bool hermite = false, modulate = true, first = true, dlyInit = false, revSilent = true, dlySilent = true;
    std::array<DelayLine, 16> fdn;
    int len[16] {}; float fbGain[16] {}, dampC[16] {}, damp[16] {};
    std::array<std::array<Allpass, 4>, 2> diff;
    std::array<DelayLine, 2> pre, dly, dbl;
    int erDelay[8] {}; float erGain[8] {}; float erLevel = 0.0f, tailGain = 1.0f;
    Svf inHp, fbHp, fbLp;
    std::vector<float> wetL, wetR;
    Smooth revMixS, dlyMixS, widthS, dblS, fbS, dlyTimeS;
    EnvDb duckEnv;
    double lfo = 0, wow = 0, flutter = 0;
    std::atomic<float> duckShown { 1.0f };
};
} // namespace chainivo

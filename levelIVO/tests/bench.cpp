// levelIVO — banco de pruebas del núcleo (sin interfaz). Uso: bench <voz.raw float32 48k mono> [otros .raw]
// Mide nivelación (errores conocidos inyectados), limpieza (clipping, clics, golpes, ruido, sangrado),
// nulo con todo apagado y tiempos de proceso.
#include "core/Engine.h"
#include <cstdio>
#include <random>
#include <chrono>

using namespace levelivo;
using namespace levelivo::dsp;

static std::vector<float> loadRaw (const char* path)
{
    std::vector<float> v; FILE* f = std::fopen (path, "rb"); if (! f) return v; float x;
    while (std::fread (&x, 4, 1, f) == 1) v.push_back (x);
    std::fclose (f); return v;
}
static TakePtr makeTake (const std::vector<float>& x, double sr = 48000, const std::vector<float>* ref = nullptr)
{
    auto t = std::make_shared<Take>();
    t->sampleRate = sr;
    t->audio.setSize (1, (int) x.size());
    std::copy (x.begin(), x.end(), t->audio.getWritePointer (0));
    t->regions.push_back ({ 0, 0, (int) x.size() });
    if (ref) t->reference = *ref;
    t->name = "test";
    return t;
}
static double energy (const float* x, int n) { double s = 0; for (int i = 0; i < n; ++i) s += (double) x[i] * x[i]; return s; }
static double sdr (const std::vector<float>& ref, const float* est, int a, int b)
{
    double s = 0, e = 0; for (int i = a; i < b; ++i) { s += (double) ref[(size_t) i] * ref[(size_t) i]; const double d = est[i] - ref[(size_t) i]; e += d * d; }
    return 10 * std::log10 ((s + 1e-20) / (e + 1e-20));
}
static CleanParams cleanOff()
{
    CleanParams c; c.noiseOn = c.bleedOn = c.otherOn = c.hitsOn = c.mouthOn = c.breathOn = false; c.clipRepair = false; return c;
}
static LevelParams levelOff() { LevelParams l; l.intensity = 0; l.peakProtect = false; return l; }

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("uso: bench voz.raw\n"); return 1; }
    auto x = loadRaw (argv[1]);
    float pk = 0; for (float v : x) pk = std::max (pk, std::abs (v));
    for (auto& v : x) v *= 0.5f / pk;
    const int n = (int) x.size();
    std::printf ("Voz: %.1f s\n", n / 48000.0);

    // ------------------------------------------------ Detección
    auto base = Engine::computeNow (makeTake (x), cleanOff(), levelOff(), {});
    const auto& A = *base->analysis;
    std::printf ("Detección: %zu frases, %zu palabras, %zu sílabas, %zu respiraciones, %zu otras voces, %zu ataques, %zu clics cand., %zu golpes, clipping %zu\n",
                 A.phrases.size(), A.words.size(), A.syllables.size(), A.breaths.size(), A.others.size(), A.onsets.size(), A.clicks.size(), A.hits.size(), A.clips.size());
    { double c = 0; for (auto& w : A.words) c += w.conf; std::printf ("  confianza media de palabras %.2f · voz %.1f LUFS · suelo %.1f\n", c / std::max<size_t> (1, A.words.size()), A.voiceDb, A.floorDb); }

    // ------------------------------------------------ Nulo
    {
        auto r = Engine::computeNow (makeTake (x), cleanOff(), levelOff(), {});
        double e = 0; for (int i = 0; i < n; ++i) { const double d = r->sample (Listen::final_, 0, i) - x[(size_t) i]; e += d * d; }
        std::printf ("Nulo (todo apagado): %.1f dB\n", 10 * std::log10 (e / energy (x.data(), n) + 1e-30));
    }

    // ------------------------------------------------ Nivelación con errores conocidos
    const int hop = A.hop;
    auto wl = [&] (const float* s) {
        std::vector<float> k (s, s + n); runChain ({ Biquad::kShelf (48000), Biquad::kHighpass (48000) }, k);
        auto L = framePowerDb (k, hop, 2 * hop, A.nFrames);
        std::vector<float> w; for (auto& W : A.words) w.push_back (energyMeanDb (L.data() + W.a, W.len()));
        return std::make_pair (w, L);
    };
    auto [wo, Lo] = wl (x.data());
    const char* modes[] = { "Natural", "Balanced", "Controlled" };
    LevelParams mp[3];
    mp[0].phrase = 0.7f; mp[0].word = 0.5f; mp[0].syllable = 0.0f; mp[0].sensitivity = 0.15f; mp[0].maxGain = mp[0].maxAtt = 6;
    mp[1].phrase = 0.9f; mp[1].word = 0.8f; mp[1].syllable = 0.3f; mp[1].sensitivity = 0.4f; mp[1].maxGain = mp[1].maxAtt = 9;
    mp[2].phrase = 1.0f; mp[2].word = 1.0f; mp[2].syllable = 0.5f; mp[2].sensitivity = 0.8f; mp[2].maxGain = mp[2].maxAtt = 12;
    std::printf ("\nNivelación (5 tomas con deriva ±4 dB y 30 %% de palabras a ±4-6 dB):\n");
    std::printf ("  %-12s  error quitado   intra-palabra   pausas    toca toma buena   dispersión\n", "");
    for (int m = 0; m < 3; ++m)
    {
        double rem = 0, intra = 0, pause = 0, good = 0, spr = 0;
        for (int seed = 0; seed < 5; ++seed)
        {
            std::mt19937 rng ((unsigned) seed + 1);
            std::uniform_real_distribution<float> u (0, 1);
            std::vector<float> err ((size_t) A.nFrames, 0.0f);
            for (int f = 0; f < A.nFrames; ++f) err[(size_t) f] = 4.0f * std::sin (2 * (float) kPi * f * 0.005f / 9.0f + seed);
            for (auto& W : A.words) if (u (rng) < 0.3f) { const float j = (u (rng) < 0.5f ? -1.0f : 1.0f) * (u (rng) < 0.5f ? 4.0f : 6.0f); for (int f = W.a; f < W.b; ++f) err[(size_t) f] += j; }
            auto lin = render::framesToLinear (err, hop, n);
            std::vector<float> xe (x); for (int i = 0; i < n; ++i) xe[(size_t) i] *= lin[(size_t) i];
            auto r = Engine::computeNow (makeTake (xe), cleanOff(), mp[m], {});
            std::vector<float> y ((size_t) n); for (int i = 0; i < n; ++i) y[(size_t) i] = r->sample (Listen::final_, 0, i);
            auto [we, Le] = wl (xe.data());
            auto [wy, Ly] = wl (y.data());
            std::vector<float> ew, tot;
            for (size_t k = 0; k < A.words.size(); ++k) { ew.push_back (we[k] - wo[k]); tot.push_back (wy[k] - wo[k]); }
            auto sd = [] (std::vector<float> v) { double mm = 0; for (float a : v) mm += a; mm /= v.size(); double s = 0; for (float a : v) s += (a - mm) * (a - mm); return std::sqrt (s / v.size()); };
            rem += 1.0 - sd (tot) / sd (ew);
            double di = 0; int ni = 0;
            for (auto& W : A.words) if (W.len() > 10)
            {
                std::vector<float> a (Ly.begin() + W.a, Ly.begin() + W.b), b (Lo.begin() + W.a, Lo.begin() + W.b), c (Le.begin() + W.a, Le.begin() + W.b);
                di += (percentile (a, 90) - percentile (a, 10)) - (percentile (c, 90) - percentile (c, 10)); ++ni;
            }
            intra += di / std::max (1, ni);
            double pe = 0, pi = 0; for (int f = 0; f < A.nFrames; ++f) if (! A.active[(size_t) f]) { pe += Ly[(size_t) f] - Le[(size_t) f]; ++pi; }
            pause += pe / std::max (1.0, pi);
            spr += r->level->spreadAfter;
            // toma buena
            if (seed == 0)
            {
                auto r0 = Engine::computeNow (makeTake (x), cleanOff(), mp[m], {});
                double g = 0; int c = 0; for (int f = 0; f < A.nFrames; ++f) if (A.active[(size_t) f]) { g += std::abs (r0->level->gainDb[(size_t) f]); ++c; }
                good = g / std::max (1, c);
            }
        }
        std::printf ("  %-12s  %6.0f %%        %+5.2f dB        %+5.1f dB  %5.2f dB          %.2f dB\n", modes[m], rem / 5 * 100, intra / 5, pause / 5, good, spr / 5);
    }

    // ------------------------------------------------ Clipping
    {
        std::vector<float> big (x); for (auto& v : big) v *= 2.4f;            // picos a +7,6 dBFS
        std::vector<float> clipped (big); int nclip = 0;
        for (auto& v : clipped) if (std::abs (v) > 1.0f) { v = v > 0 ? 1.0f : -1.0f; ++nclip; }
        CleanParams c = cleanOff(); c.clipRepair = true;
        auto r = Engine::computeNow (makeTake (clipped), c, levelOff(), {});
        const float* y = r->clean->cleaned.getReadPointer (0);
        // SDR sólo alrededor de los tramos recortados
        double s0 = 0, e0 = 0, e1 = 0;
        for (int i = 0; i < n; ++i) if (std::abs (big[(size_t) i]) > 1.0f) { s0 += big[(size_t) i] * big[(size_t) i]; e0 += std::pow (clipped[(size_t) i] - big[(size_t) i], 2); e1 += std::pow (y[i] - big[(size_t) i], 2); }
        std::printf ("\nClipping: %d muestras recortadas, %zu tramos detectados, %d reparados, %d sin reparar · SDR en lo recortado %.1f → %.1f dB\n",
                     nclip, r->analysis->clips.size(), r->clean->nClipsFixed, r->clean->nClipsUnfixable, 10 * std::log10 (s0 / e0), 10 * std::log10 (s0 / e1));
    }
    // ------------------------------------------------ Clics
    {
        std::vector<float> xc (x); std::mt19937 rng (7); std::uniform_int_distribution<int> pos (48000, n - 48000);
        std::vector<int> where;
        for (int k = 0; k < 30; ++k) { const int p = pos (rng); where.push_back (p); for (int i = 0; i < 24; ++i) xc[(size_t) (p + i)] += 0.25f * std::exp (-i / 4.0f) * (i % 2 ? -1.0f : 1.0f); }
        CleanParams c = cleanOff(); c.mouthOn = true; c.mouthAmt = 0.6f;
        auto r = Engine::computeNow (makeTake (xc), c, levelOff(), {});
        int hit = 0; for (int p : where) for (auto& e : r->clean->fixedClicks) if (p + 12 >= e.a && p <= e.b) { ++hit; break; }
        auto r0 = Engine::computeNow (makeTake (x), c, levelOff(), {});
        double s0 = 0, s1 = 0; int cnt = 0;
        for (int p : where) { s0 += sdr (x, xc.data(), p - 480, p + 480); s1 += sdr (x, r->clean->cleaned.getReadPointer (0), p - 480, p + 480); ++cnt; }
        std::printf ("Clics: 30 inyectados, %d detectados y reparados, %d falsas alarmas en la voz limpia · SDR local %.1f → %.1f dB\n",
                     hit, r0->clean->nClicks, s0 / cnt, s1 / cnt);
    }
    // ------------------------------------------------ Golpes
    {
        std::vector<float> xh (x); std::vector<int> where;
        for (int k = 0; k < 6; ++k) { const int p = (int) ((k + 0.5) * n / 6.0); where.push_back (p); for (int i = 0; i < 2400; ++i) xh[(size_t) (p + i)] += 0.6f * std::exp (-i / 600.0f) * std::sin (2 * (float) kPi * 55.0f * i / 48000.0f); }
        CleanParams c = cleanOff(); c.hitsOn = true;
        auto r = Engine::computeNow (makeTake (xh), c, levelOff(), {});
        auto r0 = Engine::computeNow (makeTake (x), c, levelOff(), {});
        double e0 = 0, e1 = 0; for (int p : where) for (int i = 0; i < 2400; ++i) { e0 += std::pow (xh[(size_t) (p + i)] - x[(size_t) (p + i)], 2); e1 += std::pow (r->clean->cleaned.getSample (0, p + i) - x[(size_t) (p + i)], 2); }
        std::printf ("Golpes: 6 inyectados, %d detectados, %d falsas alarmas · energía del golpe %.1f dB menos\n", r->clean->nHits, r0->clean->nHits, 10 * std::log10 (e0 / e1));
    }
    // ------------------------------------------------ Ruido de fondo
    {
        std::vector<float> xn (x), noise ((size_t) n); std::mt19937 rng (3); std::normal_distribution<float> g (0, 1);
        float b0 = 0, b1 = 0; for (auto& v : noise) { const float w = g (rng); b0 = 0.99f * b0 + w * 0.1f; b1 = w; v = (b0 + 0.3f * b1) * 0.0025f; }
        for (int i = 0; i < n; ++i) xn[(size_t) i] += noise[(size_t) i];
        CleanParams c = cleanOff(); c.noiseOn = true; c.noiseDb = 12;
        auto r = Engine::computeNow (makeTake (xn), c, levelOff(), {});
        const float* y = r->clean->cleaned.getReadPointer (0);
        double pin = 0, pout = 0, vin = 0, vout = 0, vs = 0;
        for (int f = 0; f < A.nFrames; ++f)
        {
            for (int i = f * hop; i < (f + 1) * hop; ++i)
            {
                if (! A.active[(size_t) f]) { pin += xn[(size_t) i] * xn[(size_t) i]; pout += (double) y[i] * y[i]; }
                else { vs += x[(size_t) i] * x[(size_t) i]; vin += std::pow (xn[(size_t) i] - x[(size_t) i], 2); vout += std::pow (y[i] - x[(size_t) i], 2); }
            }
        }
        std::printf ("Ruido (-50 dBFS): pausas %.1f dB más bajas · SDR en la voz %.1f → %.1f dB · suelo medido %.1f dBFS\n",
                     10 * std::log10 (pin / pout), 10 * std::log10 (vs / vin), 10 * std::log10 (vs / vout), r->clean->noiseFloorDb);
    }
    // ------------------------------------------------ Sangrado de cascos con y sin referencia
    {
        std::vector<float> beat ((size_t) n, 0.0f); std::mt19937 rng (11); std::normal_distribution<float> g (0, 1);
        const int spb = 48000 * 60 / 124 / 2;   // corcheas a 124 bpm
        for (int i = 0; i < n; ++i)
        {
            const float t = i / 48000.0f; const int ph = i % spb; const float env = std::exp (-ph / 900.0f);
            beat[(size_t) i] = 0.3f * std::sin (2 * (float) kPi * 110 * t) * (0.6f + 0.4f * std::sin (t * 3)) + 0.25f * env * g (rng) + 0.15f * std::sin (2 * (float) kPi * 440 * t + std::sin (t * 7));
        }
        std::vector<float> bl (beat);
        runChain ({ Biquad::highpass (48000, 250), Biquad::lowpass (48000, 5000), Biquad::highpass (48000, 400, 2.0) }, bl);
        const int delay = 144;   // 3 ms
        std::vector<float> xb (x);
        for (int i = delay; i < n; ++i) xb[(size_t) i] += 0.02f * bl[(size_t) (i - delay)];
        for (int withRef = 0; withRef < 2; ++withRef)
        {
            CleanParams c = cleanOff(); c.bleedOn = true; c.bleedAmt = 0.7f;
            auto r = Engine::computeNow (makeTake (xb, 48000, withRef ? &beat : nullptr), c, levelOff(), {});
            double pin = 0, pout = 0, vs = 0, vin = 0, vout = 0;
            for (int f = 0; f < A.nFrames; ++f)
                for (int i = f * hop; i < (f + 1) * hop; ++i)
                {
                    const float y = r->sample (Listen::clean, 0, i);
                    if (! A.active[(size_t) f]) { pin += xb[(size_t) i] * xb[(size_t) i]; pout += (double) y * y; }
                    else { vs += x[(size_t) i] * x[(size_t) i]; vin += std::pow (xb[(size_t) i] - x[(size_t) i], 2); vout += std::pow (y - x[(size_t) i], 2); }
                }
            std::printf ("Sangrado %s: pausas %.1f dB más bajas · SDR en la voz %.1f → %.1f dB", withRef ? "CON referencia" : "SIN referencia",
                         10 * std::log10 (pin / pout), 10 * std::log10 (vs / vin), 10 * std::log10 (vs / vout));
            if (withRef) std::printf (" · retardo %d (real %d) · coherencia %.2f", r->clean->refDelay, delay, r->clean->coherence);
            std::printf ("\n");
        }
    }
    // ------------------------------------------------ Tiempo para 3 minutos estéreo
    {
        std::vector<float> longx; while ((int) longx.size() < 48000 * 180) longx.insert (longx.end(), x.begin(), x.end());
        longx.resize (48000 * 180);
        auto t = std::make_shared<Take>(); t->sampleRate = 48000; t->audio.setSize (2, (int) longx.size());
        for (int ch = 0; ch < 2; ++ch) std::copy (longx.begin(), longx.end(), t->audio.getWritePointer (ch));
        t->regions.push_back ({ 0, 0, (int) longx.size() }); t->reference = longx;
        std::atomic<bool> no { false };
        auto t0 = std::chrono::steady_clock::now();
        Analysis A2; Analyzer::analyze (*t, A2, no);
        auto t1 = std::chrono::steady_clock::now();
        CleanParams c; CleanResult C; Cleaner::process (*t, A2, c, C, no);
        auto t2 = std::chrono::steady_clock::now();
        LevelResult L; Leveler::decide (A2, LevelParams(), {}, render::framePeaks (C.cleaned, A2.hop, A2.nFrames), C.gainDb, L);
        auto t3 = std::chrono::steady_clock::now();
        auto ms = [] (auto a, auto b) { return std::chrono::duration<double, std::milli> (b - a).count(); };
        std::printf ("\n3 min estéreo con referencia: análisis %.0f ms · limpieza completa %.0f ms · decisión de nivel %.0f ms\n", ms (t0, t1), ms (t1, t2), ms (t2, t3));
    }
    return 0;
}

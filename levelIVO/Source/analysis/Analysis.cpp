#include "Analysis.h"
#include "../dsp/Dsp.h"

namespace levelivo
{
using namespace dsp;

float Analysis::segLevel (const Seg& s) const
{
    const int a = std::clamp (s.a, 0, nFrames), b = std::clamp (s.b, a, nFrames);
    return energyMeanDb (L.data() + a, b - a);
}

namespace
{
using Runs = std::vector<std::pair<int, int>>;

Runs runsOf (const std::vector<uint8_t>& m, int a, int b)
{
    Runs r; int s = -1;
    for (int i = a; i < b; ++i)
    {
        if (m[(size_t) i] && s < 0) s = i;
        if (! m[(size_t) i] && s >= 0) { r.push_back ({ s, i }); s = -1; }
    }
    if (s >= 0) r.push_back ({ s, b });
    return r;
}
// Rellena huecos menores que k dentro de [a,b) (no toca los bordes)
void closeGaps (std::vector<uint8_t>& m, int a, int b, int k)
{
    std::vector<uint8_t> inv ((size_t) m.size());
    for (int i = a; i < b; ++i) inv[(size_t) i] = ! m[(size_t) i];
    for (auto [s, e] : runsOf (inv, a, b))
        if (e - s < k && s > a && e < b) for (int i = s; i < e; ++i) m[(size_t) i] = 1;
}
void removeIslands (std::vector<uint8_t>& m, int a, int b, int k)
{
    for (auto [s, e] : runsOf (m, a, b))
        if (e - s < k) for (int i = s; i < e; ++i) m[(size_t) i] = 0;
}
std::vector<float> smooth (const float* x, int n, int half)
{
    std::vector<float> y ((size_t) n);
    std::vector<double> cs ((size_t) n + 1, 0.0);
    for (int i = 0; i < n; ++i) cs[(size_t) i + 1] = cs[(size_t) i] + x[i];
    for (int i = 0; i < n; ++i)
    {
        const int a = std::max (0, i - half), b = std::min (n, i + half + 1);
        y[(size_t) i] = (float) ((cs[(size_t) b] - cs[(size_t) a]) / (b - a));
    }
    return y;
}

// Periodicidad tipo YIN sobre la señal diezmada a ~16 kHz; una medida cada 10 ms
void periodicity (const std::vector<float>& mono, double sr, int hop, int nFrames,
                  const std::vector<float>& L, float gate, std::vector<float>& per, std::vector<float>& f0,
                  const std::atomic<bool>& cancel)
{
    per.assign ((size_t) nFrames, 0.0f); f0.assign ((size_t) nFrames, 0.0f);
    const int D = std::max (1, (int) std::lround (sr / 16000.0));
    const double fsd = sr / D;
    std::vector<float> x = mono;
    runChain ({ Biquad::lowpass (sr, 0.42 * fsd, 0.54), Biquad::lowpass (sr, 0.42 * fsd, 1.31), Biquad::highpass (sr, 60.0) }, x);
    std::vector<float> d ((size_t) (x.size() / (size_t) D + 1));
    for (size_t i = 0, j = 0; i < x.size(); i += (size_t) D, ++j) d[j] = x[i];
    const int W = (int) (0.025 * fsd);                    // ventana 25 ms
    const int tauMin = std::max (2, (int) (fsd / 1100.0)), tauMax = (int) (fsd / 65.0);
    std::vector<double> df ((size_t) tauMax + 1), cm ((size_t) tauMax + 1);
    const int nd = (int) d.size();
    for (int f = 0; f < nFrames; f += 2)
    {
        if ((f & 255) == 0 && cancel.load()) return;
        if (L[(size_t) f] < gate) continue;
        const int c = (f * hop + hop) / D;                // centro en la señal diezmada
        const int s = c - (W + tauMax) / 2;
        if (s < 0 || s + W + tauMax >= nd) continue;
        const float* p = d.data() + s;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double acc = 0;
            for (int j = 0; j < W; ++j) { const double e = (double) p[j] - p[j + tau]; acc += e * e; }
            df[(size_t) tau] = acc;
        }
        double run = 0; cm[0] = 1.0;
        for (int tau = 1; tau <= tauMax; ++tau) { run += df[(size_t) tau]; cm[(size_t) tau] = run > 0 ? df[(size_t) tau] * tau / run : 1.0; }
        int best = -1; double bv = 1.0;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (cm[(size_t) tau] < 0.2 && cm[(size_t) tau] <= cm[(size_t) tau - 1] && cm[(size_t) tau] <= cm[(size_t) tau + 1]) { best = tau; bv = cm[(size_t) tau]; break; }
            if (cm[(size_t) tau] < bv) { bv = cm[(size_t) tau]; best = tau; }
        }
        const float pv = (float) std::clamp (1.0 - bv, 0.0, 1.0);
        float hz = 0;
        if (best > 0 && bv < 0.3)
        {
            // interpolación parabólica del mínimo
            double t = best;
            if (best > 1 && best < tauMax)
            {
                const double a = cm[(size_t) best - 1], b = cm[(size_t) best], cc = cm[(size_t) best + 1];
                const double den = a - 2 * b + cc;
                if (std::abs (den) > 1e-12) t = best + 0.5 * (a - cc) / den;
            }
            hz = (float) (fsd / t);
        }
        per[(size_t) f] = pv; f0[(size_t) f] = hz;
        if (f + 1 < nFrames) { per[(size_t) f + 1] = pv; f0[(size_t) f + 1] = hz; }
    }
}
} // namespace

bool Analyzer::analyze (const Take& take, Analysis& A, const std::atomic<bool>& cancel, const std::function<void (float)>& progress)
{
    auto prog = [&] (float v) { if (progress) progress (v); };
    const double sr = take.sampleRate;
    A = Analysis();
    A.sampleRate = sr;
    A.hop = std::max (16, (int) std::lround (sr * 0.005));
    const int hop = A.hop;
    const int n = take.numSamples();
    A.nFrames = n / hop;
    const int F = A.nFrames;
    if (F < 4) return true;
    const auto mono = take.mono();
    prog (0.02f);

    // ---------------------------------------------------------------- Medidas por trama
    {
        std::vector<float> k = mono;
        runChain ({ Biquad::kShelf (sr), Biquad::kHighpass (sr) }, k);
        A.L = framePowerDb (k, hop, 2 * hop, F);
        // BS.1770 suma -0.691 dB al nivel ponderado: así las cifras se leen como LUFS
        for (auto& v : A.L) v -= 0.691f;
    }
    if (cancel.load()) return false;
    {
        std::vector<float> v = mono;
        runChain ({ Biquad::highpass (sr, 300), Biquad::highpass (sr, 300), Biquad::lowpass (sr, 2500), Biquad::lowpass (sr, 2500) }, v);
        A.V = framePowerDb (v, hop, 4 * hop, F);
        std::vector<float> h = mono;
        runChain ({ Biquad::highpass (sr, 3000), Biquad::highpass (sr, 3000) }, h);
        A.H = framePowerDb (h, hop, 2 * hop, F);
        std::vector<float> lo = mono;
        runChain ({ Biquad::lowpass (sr, 100), Biquad::lowpass (sr, 100) }, lo);
        A.Lo = framePowerDb (lo, hop, 4 * hop, F);
        std::vector<float> t1 = mono, t2 = mono;
        runChain ({ Biquad::highpass (sr, 200), Biquad::lowpass (sr, 900) }, t1);
        runChain ({ Biquad::highpass (sr, 2000), Biquad::lowpass (sr, std::min (6000.0, sr * 0.45)) }, t2);
        auto a1 = framePowerDb (t1, hop, 4 * hop, F), a2 = framePowerDb (t2, hop, 4 * hop, F);
        A.tilt.resize ((size_t) F);
        for (int f = 0; f < F; ++f) A.tilt[(size_t) f] = a2[(size_t) f] - a1[(size_t) f];
    }
    if (take.hasReference())
    {
        std::vector<float> r = take.reference;
        r.resize ((size_t) n, 0.0f);
        runChain ({ Biquad::kShelf (sr), Biquad::kHighpass (sr) }, r);
        A.refL = framePowerDb (r, hop, 80 * hop, F);   // 400 ms: nivel del beat, no de cada golpe
    }
    A.peakDb.assign ((size_t) F, -120.0f);
    for (int f = 0; f < F; ++f)
    {
        float m = 0;
        for (int c = 0; c < take.numChannels(); ++c)
        {
            const float* p = take.audio.getReadPointer (c) + f * hop;
            for (int i = 0; i < hop; ++i) m = std::max (m, std::abs (p[i]));
        }
        A.peakDb[(size_t) f] = linToDb (m);
    }
    prog (0.15f);
    if (cancel.load()) return false;

    // ---------------------------------------------------------------- Suelo y umbral de voz
    A.regionStart.assign ((size_t) F, 0);
    std::vector<std::pair<int, int>> regs;   // regiones en tramas
    for (const auto& r : take.regions)
    {
        const int a = std::clamp (r.takeStart / hop, 0, F), b = std::clamp ((r.takeStart + r.length) / hop, a, F);
        if (b > a) { regs.push_back ({ a, b }); A.regionStart[(size_t) a] = 1; }
    }
    if (regs.empty()) { regs.push_back ({ 0, F }); A.regionStart[0] = 1; }

    {
        std::vector<float> l = A.L;
        A.floorDb = percentile (l, 10.0);
        A.peakLevelDb = percentile (l, 99.0);
    }
    const float thr = A.floorDb + std::max (10.0f, 0.35f * (A.peakLevelDb - A.floorDb));

    periodicity (mono, sr, hop, F, A.L, A.floorDb + 6.0f, A.per, A.f0, cancel);
    if (cancel.load()) return false;
    prog (0.45f);

    A.active.assign ((size_t) F, 0);
    for (int f = 0; f < F; ++f) A.active[(size_t) f] = A.L[(size_t) f] > thr;
    // nivel típico de la voz (mediana de las tramas activas)
    {
        std::vector<float> act;
        for (int f = 0; f < F; ++f) if (A.active[(size_t) f]) act.push_back (A.L[(size_t) f]);
        A.voiceDb = act.empty() ? A.peakLevelDb - 10.0f : median (act);
        std::vector<float> f0s;
        for (int f = 0; f < F; ++f) if (A.active[(size_t) f] && A.f0[(size_t) f] > 0) f0s.push_back (A.f0[(size_t) f]);
        A.f0Median = f0s.empty() ? 0.0f : median (f0s);
    }

    // ---------------------------------------------------------------- Respiraciones
    // Ruido de aire: aperiódico, más flojo que la voz, sin el pico de agudos de una "s", y de 150 ms a 1,2 s.
    std::vector<uint8_t> breathMask ((size_t) F, 0);
    for (int f = 0; f < F; ++f)
    {
        const bool loudEnough = A.L[(size_t) f] > A.floorDb + 5.0f;
        const bool soft = A.L[(size_t) f] < A.voiceDb - 8.0f;
        const bool aper = A.per[(size_t) f] < 0.55f;
        const bool notSibilant = A.H[(size_t) f] - A.V[(size_t) f] < 6.0f;
        breathMask[(size_t) f] = loudEnough && soft && aper && notSibilant;
    }
    for (auto [ra, rb] : regs)
    {
        closeGaps (breathMask, ra, rb, 6);
        for (auto [s, e] : runsOf (breathMask, ra, rb))
        {
            const int dur = e - s;
            if (dur < 30 || dur > 240) continue;   // 150 ms .. 1,2 s
            // confianza: cuanto más aperiódico y más lejos del nivel de voz, más seguro
            double ap = 0; for (int f = s; f < e; ++f) ap += A.per[(size_t) f];
            ap /= dur;
            const float lvl = energyMeanDb (A.L.data() + s, dur);
            Seg g; g.a = s; g.b = e;
            g.conf = (float) std::clamp ((0.6 - ap) * 2.5, 0.2, 1.0) * std::clamp ((A.voiceDb - lvl) / 18.0f, 0.3f, 1.0f);
            A.breaths.push_back (g);
            for (int f = s; f < e; ++f) A.active[(size_t) f] = 0;
        }
    }

    // ---------------------------------------------------------------- Frases, palabras, sílabas
    for (auto [ra, rb] : regs)
    {
        closeGaps (A.active, ra, rb, 12);    // huecos < 60 ms dentro de una palabra
        removeIslands (A.active, ra, rb, 8); // islas < 40 ms
    }
    std::vector<uint8_t> ph = A.active;
    for (auto [ra, rb] : regs) closeGaps (ph, ra, rb, 50);   // pausas >= 250 ms separan frases
    for (auto [ra, rb] : regs)
        for (auto [s, e] : runsOf (ph, ra, rb)) { Seg p; p.a = s; p.b = e; A.phrases.push_back (p); }
    if (cancel.load()) return false;
    prog (0.6f);

    for (int pi = 0; pi < (int) A.phrases.size(); ++pi)
    {
        const auto P = A.phrases[(size_t) pi];
        const int len = P.len();
        auto s = smooth (A.L.data() + P.a, len, 4);   // 45 ms
        std::vector<int> cuts { P.a }; std::vector<float> conf;
        int i = 20;
        while (i < len - 20)
        {
            const int w0 = std::max (0, i - 15), w1 = std::min (len, i + 16);
            float mn = s[(size_t) w0]; for (int j = w0; j < w1; ++j) mn = std::min (mn, s[(size_t) j]);
            if (s[(size_t) i] == mn)
            {
                float left = -200, right = -200;
                for (int j = std::max (0, i - 40); j < i; ++j) left = std::max (left, s[(size_t) j]);
                for (int j = i; j < std::min (len, i + 40); ++j) right = std::max (right, s[(size_t) j]);
                const float depth = std::min (left, right) - s[(size_t) i];
                if (depth >= 6.0f && P.a + i - cuts.back() >= 20)
                {
                    cuts.push_back (P.a + i); conf.push_back (std::min (1.0f, depth / 15.0f)); i += 15; continue;
                }
            }
            ++i;
        }
        cuts.push_back (P.b);
        std::vector<float> bc; bc.push_back (1.0f); for (float c : conf) bc.push_back (c); bc.push_back (1.0f);
        for (size_t k = 0; k + 1 < cuts.size(); ++k)
        {
            Seg w; w.a = cuts[k]; w.b = cuts[k + 1]; w.parent = pi; w.conf = std::min (bc[k], bc[k + 1]);
            // confianza menor en palabras con poca energía sonora (posible consonante suelta o ruido)
            int voiced = 0; for (int f = w.a; f < w.b; ++f) voiced += A.per[(size_t) f] > 0.6f;
            if (w.len() > 0 && voiced < w.len() / 4) w.conf *= 0.6f;
            A.words.push_back (w);
        }
    }
    for (int wi = 0; wi < (int) A.words.size(); ++wi)
    {
        const auto W = A.words[(size_t) wi];
        const int len = W.len();
        auto v = smooth (A.V.data() + W.a, len, 2);
        float vmax = -200; for (float x : v) vmax = std::max (vmax, x);
        std::vector<int> peaks;
        for (int i = 2; i < len - 2; ++i)
        {
            const int w0 = std::max (0, i - 16), w1 = std::min (len, i + 17);
            float mx = v[(size_t) w0]; for (int j = w0; j < w1; ++j) mx = std::max (mx, v[(size_t) j]);
            if (v[(size_t) i] == mx && v[(size_t) i] > vmax - 18.0f && (peaks.empty() || i - peaks.back() >= 16)) peaks.push_back (i);
        }
        if (peaks.size() <= 1) { Seg s; s.a = W.a; s.b = W.b; s.parent = wi; s.conf = W.conf; A.syllables.push_back (s); continue; }
        std::vector<int> cuts { W.a };
        for (size_t k = 0; k + 1 < peaks.size(); ++k)
        {
            int m = peaks[k];
            for (int j = peaks[k]; j < peaks[k + 1]; ++j) if (v[(size_t) j] < v[(size_t) m]) m = j;
            cuts.push_back (W.a + m);
        }
        cuts.push_back (W.b);
        for (size_t k = 0; k + 1 < cuts.size(); ++k) { Seg s; s.a = cuts[k]; s.b = cuts[k + 1]; s.parent = wi; s.conf = W.conf * 0.9f; A.syllables.push_back (s); }
    }
    // ataques: subida rápida de sonoridad (4 tramas = 20 ms) o de agudos (consonantes)
    for (int f = 4; f < F; ++f)
    {
        // subida que se mantiene (una consonante o un ataque de voz; un clic sube y baja en 1-2 ms)
        if (f + 6 > F) break;
        float sl = 0, sh = 0; for (int j = f; j < f + 6; ++j) { sl += A.L[(size_t) j]; sh += A.H[(size_t) j]; }
        const float rise = sl / 6.0f - A.L[(size_t) f - 4];
        const float riseH = sh / 6.0f - A.H[(size_t) f - 4];
        if ((rise > 7.0f || riseH > 10.0f) && A.active[(size_t) f] && (A.onsets.empty() || f - A.onsets.back() > 6))
            A.onsets.push_back (f);
    }
    prog (0.7f);

    // ---------------------------------------------------------------- Otras voces
    // Frases muy por debajo de la voz principal y cortas, o con un tono muy distinto: probablemente otra persona.
    if (! A.phrases.empty())
    {
        std::vector<float> pl;
        for (const auto& p : A.phrases) pl.push_back (A.segLevel (p));
        const float med = median (pl);
        for (size_t k = 0; k < A.phrases.size(); ++k)
        {
            const auto& p = A.phrases[k];
            const float dur = p.len() * 0.005f;
            std::vector<float> fs; for (int f = p.a; f < p.b; ++f) if (A.f0[(size_t) f] > 0) fs.push_back (A.f0[(size_t) f]);
            const float pf = fs.empty() ? 0.0f : median (fs);
            const float semis = (pf > 0 && A.f0Median > 0) ? 12.0f * std::log2 (pf / A.f0Median) : 0.0f;
            const bool quiet = pl[k] < med - 14.0f;
            const bool farPitch = std::abs (semis) > 7.0f && pl[k] < med - 8.0f;
            if ((quiet || farPitch) && dur < 3.0f)
            {
                Seg o = p; o.conf = quiet && farPitch ? 0.9f : 0.55f; o.parent = (int) k;
                A.others.push_back (o);
            }
        }
    }

    // ---------------------------------------------------------------- Golpes al micro
    {
        std::vector<float> lo;
        for (int f = 0; f < F; ++f) if (A.active[(size_t) f]) lo.push_back (A.Lo[(size_t) f]);
        const float base = lo.empty() ? A.floorDb : median (lo);
        for (int f = 2; f < F; ++f)
        {
            const bool jump = A.Lo[(size_t) f] - A.Lo[(size_t) f - 2] > 10.0f;
            const bool big = A.Lo[(size_t) f] > base + 12.0f && A.Lo[(size_t) f] > A.L[(size_t) f] - 4.0f;
            if (jump && big)
            {
                int e = f;
                while (e < F && e - f < 30 && A.Lo[(size_t) e] > base + 6.0f) ++e;
                const int s = std::max (0, f - 2);
                Event ev; ev.a = s * hop; ev.b = std::min (n, (e + 2) * hop); ev.strength = A.Lo[(size_t) f] - base;
                A.hits.push_back (ev);
                f = e + 4;
            }
        }
    }
    prog (0.78f);

    // ---------------------------------------------------------------- Clipping (por canal)
    A.clipLevel.assign ((size_t) take.numChannels(), 0.0f);
    for (int c = 0; c < take.numChannels(); ++c)
    {
        const float* x = take.audio.getReadPointer (c);
        float m = 0; for (int i = 0; i < n; ++i) m = std::max (m, std::abs (x[i]));
        if (m < 0.05f) continue;
        const float lvl = m * 0.9995f;
        std::vector<Event> evs;
        int i = 0;
        while (i < n)
        {
            if (std::abs (x[i]) >= lvl)
            {
                int j = i; while (j < n && std::abs (x[j]) >= lvl && (x[j] > 0) == (x[i] > 0)) ++j;
                if (j - i >= 3) { Event e; e.a = i; e.b = j; e.channel = c; e.strength = lvl; e.repairable = (j - i) <= (int) (0.0025 * sr); evs.push_back (e); }
                i = j;
            }
            else ++i;
        }
        // un único pico plano no es clipping; varias mesetas al mismo nivel sí
        if (evs.size() >= 2)
        {
            A.clipLevel[(size_t) c] = lvl;
            for (auto& e : evs) A.clips.push_back (e);
        }
    }
    if (cancel.load()) return false;
    prog (0.85f);

    // ---------------------------------------------------------------- Clics de boca (candidatos)
    // Residuo de predicción lineal: los clics son impulsos que la predicción no explica.
    {
        const int blk = (int) (0.02 * sr), order = 16;
        std::vector<float> e ((size_t) n, 0.0f);
        std::vector<double> r;
        for (int s = 0; s + blk <= n; s += blk)
        {
            const int wa = std::max (0, s - blk / 2), wb = std::min (n, s + blk + blk / 2);
            autocorr (mono.data() + wa, wb - wa, order, r);
            if (r[0] < 1e-10) continue;
            r[0] *= 1.0001;
            const auto a = levinson (r, order);
            for (int t = std::max (order, s); t < s + blk; ++t)
            {
                double acc = mono[(size_t) t];
                for (int k = 1; k <= order; ++k) acc += a[(size_t) k] * mono[(size_t) (t - k)];
                e[(size_t) t] = (float) acc;
            }
        }
        // dispersión robusta del residuo por bloque (mediana de |e|)
        std::vector<float> sig ((size_t) (n / blk + 1), 1e-9f);
        for (int b = 0; b * blk < n; ++b)
        {
            const int s = b * blk, t = std::min (n, s + blk);
            std::vector<float> v; v.reserve ((size_t) (t - s));
            for (int i = s; i < t; ++i) v.push_back (std::abs (e[(size_t) i]));
            sig[(size_t) b] = std::max (1e-9f, 1.4826f * median (v));
        }
        {
            double tot = 0; for (int i = 0; i < n; ++i) tot += (double) e[(size_t) i] * e[(size_t) i];
            const float g = (float) std::sqrt (tot / std::max (1, n));
            for (auto& v : sig) v = std::max (v, 0.02f * g + 1e-7f);
        }
        std::vector<uint8_t> nearOnset ((size_t) F, 0);
        for (int o : A.onsets) for (int f = std::max (0, o - 2); f < std::min (F, o + 3); ++f) nearOnset[(size_t) f] = 1;
        const int maxLen = (int) (0.003 * sr), merge = (int) (0.0005 * sr);
        int i = order;
        while (i < n)
        {
            const float s = sig[(size_t) (i / blk)];
            if (std::abs (e[(size_t) i]) > 5.0f * s)
            {
                int j = i, last = i; float pk = 0;
                while (j < n && j - last <= merge)
                {
                    if (std::abs (e[(size_t) j]) > 5.0f * s) { last = j; pk = std::max (pk, std::abs (e[(size_t) j])); }
                    ++j;
                }
                const int a = i, b = last + 1;
                const int fr = std::min (F - 1, a / hop);
                // aislado: los pulsos glotales de la voz se repiten cada periodo; un clic no
                const int nb = (int) (0.008 * sr);
                float around = 0;
                for (int k = std::max (0, a - nb); k < std::min (n, b + nb); ++k)
                    if (k < a - merge || k >= b + merge) around = std::max (around, std::abs (e[(size_t) k]));
                const bool isolated = around < 0.5f * pk;
                const bool voiced = A.per[(size_t) fr] > 0.6f;
                // junto a un ataque sólo cuenta si es un impulso muy corto y muy fuerte (las oclusivas duran más)
                const bool onsetOk = ! nearOnset[(size_t) fr] || (pk > 40.0f * s && b - a <= (int) (0.001 * sr));
                if (b - a <= maxLen && onsetOk && isolated && (! voiced || pk > 10.0f * s))
                {
                    Event ev; ev.a = std::max (0, a - merge); ev.b = std::min (n, b + merge); ev.strength = pk / s;
                    A.clicks.push_back (ev);
                }
                i = j;
            }
            else ++i;
        }
    }
    prog (1.0f);
    return ! cancel.load();
}
} // namespace levelivo

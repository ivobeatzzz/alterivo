#include "Cleaner.h"
#include "../dsp/Dsp.h"
#include <juce_dsp/juce_dsp.h>
#include <complex>
#include <cstdio>
#include <cstdlib>

namespace levelivo
{
using namespace dsp;
using cpx = std::complex<float>;

namespace
{
int nextPow2 (int v) { int p = 1; while (p < v) p <<= 1; return p; }

// Retardo del beat respecto al micro (GCC-PHAT): devuelve muestras que hay que retrasar la referencia
int estimateDelay (const std::vector<float>& x, const std::vector<float>& r, double sr)
{
    const int N = 8192, order = 13, H = N / 2;
    const int n = (int) std::min (x.size(), r.size());
    if (n < N * 2) return 0;
    juce::dsp::FFT fft (order);
    std::vector<float> bx ((size_t) N * 2), br ((size_t) N * 2);
    std::vector<cpx> acc ((size_t) N / 2 + 1, cpx (0, 0));
    std::vector<float> win ((size_t) N);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * (float) kPi * i / N);
    const int maxFrames = std::min ((n - N) / H, 400);   // como mucho ~35 s de material
    for (int f = 0; f < maxFrames; ++f)
    {
        std::fill (bx.begin(), bx.end(), 0.0f); std::fill (br.begin(), br.end(), 0.0f);
        for (int i = 0; i < N; ++i) { bx[(size_t) i] = x[(size_t) (f * H + i)] * win[(size_t) i]; br[(size_t) i] = r[(size_t) (f * H + i)] * win[(size_t) i]; }
        fft.performRealOnlyForwardTransform (bx.data(), true);
        fft.performRealOnlyForwardTransform (br.data(), true);
        for (int k = 1; k < N / 2; ++k)
        {
            const cpx X (bx[(size_t) 2 * k], bx[(size_t) 2 * k + 1]), R (br[(size_t) 2 * k], br[(size_t) 2 * k + 1]);
            const cpx c = X * std::conj (R);
            const float m = std::abs (c);
            if (m > 1e-12f) acc[(size_t) k] += c / m;
        }
    }
    std::vector<float> buf ((size_t) N * 2, 0.0f);
    for (int k = 0; k <= N / 2; ++k) { buf[(size_t) 2 * k] = acc[(size_t) k].real(); buf[(size_t) 2 * k + 1] = acc[(size_t) k].imag(); }
    fft.performRealOnlyInverseTransform (buf.data());
    const int maxLag = std::min (N / 2 - 1, (int) (0.1 * sr));
    int best = 0; float bv = -1e30f;
    for (int lag = -maxLag; lag <= maxLag; ++lag)
    {
        const float v = buf[(size_t) ((lag + N) % N)];
        if (v > bv) { bv = v; best = lag; }
    }
    return best;   // >0: el micro va retrasado respecto a la referencia
}
} // namespace

bool Cleaner::process (const Take& take, const Analysis& A, const CleanParams& P, CleanResult& R,
                       const std::atomic<bool>& cancel, const std::function<void (float)>& progress)
{
    auto prog = [&] (float v) { if (progress) progress (v); };
    const double sr = take.sampleRate;
    const int n = take.numSamples(), nc = take.numChannels(), F = A.nFrames, hop = A.hop;
    R = CleanResult();
    R.cleaned.makeCopyOf (take.audio);
    R.gainDb.assign ((size_t) F, 0.0f);
    R.reductionDb.assign ((size_t) F, 0.0f);
    if (n == 0 || F == 0) return true;

    // ---------------------------------------------------------------- Clipping
    for (const auto& e : A.clips)
    {
        if (! P.clipRepair || ! e.repairable) { ++R.nClipsUnfixable; continue; }
        if (lsarInterpolate (R.cleaned.getWritePointer (e.channel), n, e.a, e.b, 32, e.strength)) ++R.nClipsFixed;
        else ++R.nClipsUnfixable;
    }
    prog (0.08f);

    // ---------------------------------------------------------------- Clics de boca
    if (P.mouthOn && P.mouthAmt > 0.0f)
    {
        const float thr = 30.0f - 22.0f * std::clamp (P.mouthAmt, 0.0f, 1.0f);   // 0 % → sólo clics muy claros, 100 % → también suaves
        for (const auto& e : A.clicks)
        {
            if (e.strength < thr) continue;
            bool ok = true;
            for (int c = 0; c < nc; ++c) ok &= lsarInterpolate (R.cleaned.getWritePointer (c), n, e.a, e.b, 24);
            if (ok) { R.fixedClicks.push_back (e); ++R.nClicks; }
        }
    }
    prog (0.14f);

    // ---------------------------------------------------------------- Golpes al micro: paso alto sólo en ese tramo
    if (P.hitsOn && P.hitsAmt > 0.0f)
    {
        const int fade = (int) (0.01 * sr);
        for (const auto& e : A.hits)
        {
            const int a = std::max (0, e.a - fade), b = std::min (n, e.b + fade), len = b - a;
            if (len <= 2 * fade) continue;
            for (int c = 0; c < nc; ++c)
            {
                float* x = R.cleaned.getWritePointer (c);
                std::vector<float> seg (x + a, x + b);
                // márgenes extra para que el filtro de fase cero no deje transitorios en los bordes
                zeroPhase ({ Biquad::highpass (sr, 140.0, 0.54), Biquad::highpass (sr, 140.0, 1.31) }, seg.data(), len);
                for (int i = 0; i < len; ++i)
                {
                    float w = 1.0f;
                    if (i < fade) w = (float) i / fade; else if (i >= len - fade) w = (float) (len - i) / fade;
                    w *= P.hitsAmt;
                    x[a + i] = x[a + i] * (1.0f - w) + seg[(size_t) i] * w;
                }
            }
            ++R.nHits;
        }
    }
    prog (0.2f);
    if (cancel.load()) return false;

    // ---------------------------------------------------------------- Ruido de fondo y sangrado (STFT)
    std::vector<uint8_t> voiceF ((size_t) F, 0), breathF ((size_t) F, 0);
    for (int f = 0; f < F; ++f) voiceF[(size_t) f] = A.active[(size_t) f];
    for (const auto& b : A.breaths) for (int f = b.a; f < b.b; ++f) breathF[(size_t) f] = 1;
    // tramas de "pausa": sin voz, sin respiración y lejos (100 ms) de la voz
    std::vector<uint8_t> pauseF ((size_t) F, 0);
    for (int f = 0; f < F; ++f)
    {
        bool near = false;
        for (int j = std::max (0, f - 20); j < std::min (F, f + 21) && ! near; ++j) near = voiceF[(size_t) j] || breathF[(size_t) j];
        pauseF[(size_t) f] = ! near;
    }
    // ruido medio en las pausas (dBFS)
    {
        double s = 0; int c = 0;
        for (int f = 0; f < F; ++f) if (pauseF[(size_t) f]) { s += std::pow (10.0, A.L[(size_t) f] / 10.0); ++c; }
        R.noiseFloorDb = c ? powToDb (s / c) : A.floorDb;
    }

    const bool useRef = P.bleedOn && P.bleedAmt > 0.01f && take.hasReference();
    if (P.anySpectral())
    {
        const int N = nextPow2 ((int) std::lround (2048.0 * sr / 48000.0));
        int order = 0; while ((1 << order) < N) ++order;
        const int H = N / 4, K = N / 2 + 1;
        juce::dsp::FFT fft (order);
        std::vector<float> win ((size_t) N);
        for (int i = 0; i < N; ++i) win[(size_t) i] = std::sqrt (0.5f - 0.5f * std::cos (2.0f * (float) kPi * i / N));
        const int nF = (n + N) / H + 1;

        // referencia alineada
        std::vector<float> ref;
        if (useRef)
        {
            auto mono = take.mono();
            R.refDelay = estimateDelay (mono, take.reference, sr);
            ref.assign ((size_t) n, 0.0f);
            for (int i = 0; i < n; ++i) { const int j = i - R.refDelay; if (j >= 0 && j < (int) take.reference.size()) ref[(size_t) i] = take.reference[(size_t) j]; }
        }
        auto frameStart = [&] (int t) { return t * H - N / 2; };
        auto stft = [&] (const float* x, int t, std::vector<float>& buf)
        {
            std::fill (buf.begin(), buf.end(), 0.0f);
            const int s = frameStart (t);
            for (int i = 0; i < N; ++i) { const int j = s + i; if (j >= 0 && j < n) buf[(size_t) i] = x[j] * win[(size_t) i]; }
            fft.performRealOnlyForwardTransform (buf.data(), true);
        };
        auto analysisFrame = [&] (int t) { return std::clamp ((frameStart (t) + N / 2) / hop, 0, F - 1); };
        // mezcla mono ya sin clics ni golpes
        std::vector<float> monoMix ((size_t) n, 0.0f);
        for (int c = 0; c < nc; ++c) { const float* x = R.cleaned.getReadPointer (c); for (int i = 0; i < n; ++i) monoMix[(size_t) i] += x[i] / (float) nc; }
        std::vector<float> bx ((size_t) N * 2), brf ((size_t) N * 2);

        // Pasada 1: perfil de ruido en las pausas y relación cascos -> micro donde no cantas
        std::vector<double> noiseAcc ((size_t) K, 0.0);
        std::vector<cpx> sxr; std::vector<double> srr, sxx;
        if (useRef) { sxr.assign ((size_t) K, cpx (0, 0)); srr.assign ((size_t) K, 0.0); sxx.assign ((size_t) K, 0.0); }
        std::vector<float> frameEnergy ((size_t) nF, 0.0f);
        int pauses = 0, refFrames = 0;
        for (int t = 0; t < nF; ++t)
        {
            stft (monoMix.data(), t, bx);
            double e = 0; for (int k = 0; k < K; ++k) e += (double) bx[(size_t) 2 * k] * bx[(size_t) 2 * k] + (double) bx[(size_t) 2 * k + 1] * bx[(size_t) 2 * k + 1];
            frameEnergy[(size_t) t] = (float) e;
            const int af = analysisFrame (t);
            if (pauseF[(size_t) af])
            {
                ++pauses;
                for (int k = 0; k < K; ++k) noiseAcc[(size_t) k] += (double) bx[(size_t) 2 * k] * bx[(size_t) 2 * k] + (double) bx[(size_t) 2 * k + 1] * bx[(size_t) 2 * k + 1];
            }
            if (useRef && pauseF[(size_t) af])
            {
                ++refFrames;
                stft (ref.data(), t, brf);
                for (int k = 0; k < K; ++k)
                {
                    const cpx X (bx[(size_t) 2 * k], bx[(size_t) 2 * k + 1]), Rr (brf[(size_t) 2 * k], brf[(size_t) 2 * k + 1]);
                    sxr[(size_t) k] += X * std::conj (Rr); srr[(size_t) k] += std::norm (Rr); sxx[(size_t) k] += std::norm (X);
                }
            }
            if ((t & 511) == 0) { if (cancel.load()) return false; prog (0.2f + 0.2f * t / nF); }
        }
        std::vector<float> noise ((size_t) K, 0.0f);
        if (pauses >= 20) for (int k = 0; k < K; ++k) noise[(size_t) k] = (float) (noiseAcc[(size_t) k] / pauses);
        else
        {
            // casi sin pausas: el 10 % de tramas más silenciosas hace de perfil
            const float lim = percentile (frameEnergy, 10.0);
            int c = 0;
            for (int t = 0; t < nF; ++t)
            {
                if (frameEnergy[(size_t) t] > lim) continue;
                stft (monoMix.data(), t, bx); ++c;
                for (int k = 0; k < K; ++k) noise[(size_t) k] += bx[(size_t) 2 * k] * bx[(size_t) 2 * k] + bx[(size_t) 2 * k + 1] * bx[(size_t) 2 * k + 1];
            }
            if (c) for (auto& v : noise) v /= (float) c;
        }
        std::vector<cpx> Hk;
        if (useRef)
        {
            if (refFrames < 20)   // casi sin pausas: se usa toda la toma (estimación sesgada pero útil)
                for (int t = 0; t < nF; ++t)
                {
                    stft (monoMix.data(), t, bx); stft (ref.data(), t, brf);
                    for (int k = 0; k < K; ++k)
                    {
                        const cpx X (bx[(size_t) 2 * k], bx[(size_t) 2 * k + 1]), Rr (brf[(size_t) 2 * k], brf[(size_t) 2 * k + 1]);
                        sxr[(size_t) k] += X * std::conj (Rr); srr[(size_t) k] += std::norm (Rr); sxx[(size_t) k] += std::norm (X);
                    }
                }
            Hk.assign ((size_t) K, cpx (0, 0));
            double cohSum = 0, cohW = 0; int cohN = 0;
            const int k0 = (int) (100.0 * N / sr), k1 = std::min (K - 1, (int) (8000.0 * N / sr));
            for (int k = 0; k < K; ++k)
            {
                Hk[(size_t) k] = srr[(size_t) k] > 1e-12 ? sxr[(size_t) k] / (float) srr[(size_t) k] : cpx (0, 0);
                if (k >= k0 && k <= k1 && srr[(size_t) k] > 1e-12 && sxx[(size_t) k] > 1e-12)
                { cohSum += std::norm (sxr[(size_t) k]) / srr[(size_t) k]; cohW += sxx[(size_t) k]; ++cohN; }
            }
            R.coherence = cohW > 0 ? (float) std::clamp (cohSum / cohW, 0.0, 1.0) : 0.0f;
        }
        prog (0.45f);
        // Cantidades: reducción máxima entre frases; durante la voz se limita mucho más (reducción localizada)
        const float noiseRed = P.noiseOn ? P.noiseDb : 0.0f;
        // si la referencia apenas se parece al sangrado, se reduce la cantidad para no estropear la voz
        const float cohScale = useRef ? std::clamp ((R.coherence - 0.05f) / 0.25f, 0.0f, 1.0f) : 0.0f;
        const float bleedRed = useRef ? 24.0f * P.bleedAmt * cohScale : 0.0f;
        const float maxRed = std::max (noiseRed, bleedRed);
        const float floorGap = dbToLin (-maxRed), floorVoice = dbToLin (-0.35f * maxRed);
        const float beta = useRef ? 1.5f * P.bleedAmt * cohScale : 0.0f;
        // primero se resta el sangrado estimado (fase incluida); lo que queda lo baja el filtro de Wiener
        const float alpha = useRef ? 0.85f * std::min (1.0f, P.bleedAmt * 1.4f) * cohScale : 0.0f;
        const float nScale = P.noiseOn ? 1.0f : 0.0f;

        std::vector<float> G ((size_t) K, 1.0f), Gprev ((size_t) K, 1.0f), postPrev ((size_t) K, 1.0f), Gs ((size_t) K);
        std::vector<std::vector<float>> outCh ((size_t) nc, std::vector<float> ((size_t) n + N, 0.0f));
        std::vector<float> cb ((size_t) N * 2);
        std::vector<cpx> hr ((size_t) K, cpx (0, 0));
        std::vector<double> redAcc ((size_t) F, 0.0); std::vector<int> redCnt ((size_t) F, 0);
        for (int t = 0; t < nF; ++t)
        {
            const int af = analysisFrame (t);
            const bool voice = voiceF[(size_t) af] || breathF[(size_t) af];
            const float fl = voice ? floorVoice : floorGap;
            double gsum = 0;
            stft (monoMix.data(), t, bx);
            if (beta > 0)
            {
                stft (ref.data(), t, brf);
                for (int k = 0; k < K; ++k)
                {
                    const cpx b = Hk[(size_t) k] * cpx (brf[(size_t) 2 * k], brf[(size_t) 2 * k + 1]);
                    hr[(size_t) k] = b;
                    bx[(size_t) 2 * k] -= alpha * b.real(); bx[(size_t) 2 * k + 1] -= alpha * b.imag();
                }
            }
            for (int k = 0; k < K; ++k)
            {
                float Nk = nScale * noise[(size_t) k];
                if (beta > 0) Nk += beta * (1.0f - alpha) * (1.0f - alpha) * std::norm (hr[(size_t) k]) + 0.02f * std::norm (hr[(size_t) k]);
                const float Pk = bx[(size_t) 2 * k] * bx[(size_t) 2 * k] + bx[(size_t) 2 * k + 1] * bx[(size_t) 2 * k + 1];
                const float post = Nk > 1e-20f ? Pk / Nk : 1e6f;
                const float xi = 0.98f * Gprev[(size_t) k] * Gprev[(size_t) k] * postPrev[(size_t) k] + 0.02f * std::max (post - 1.0f, 0.0f);
                G[(size_t) k] = std::max (fl, xi / (1.0f + xi));
                postPrev[(size_t) k] = std::min (post, 1e6f);
            }
            for (int k = 0; k < K; ++k)
            {
                const float a = G[(size_t) std::max (0, k - 1)], b = G[(size_t) k], c = G[(size_t) std::min (K - 1, k + 1)];
                Gs[(size_t) k] = 0.25f * a + 0.5f * b + 0.25f * c;
                gsum += Gs[(size_t) k];
            }
            Gprev = G;
            redAcc[(size_t) af] += linToDb ((float) (gsum / K)); redCnt[(size_t) af]++;
            for (int c = 0; c < nc; ++c)
            {
                stft (R.cleaned.getReadPointer (c), t, cb);
                if (alpha > 0) for (int k = 0; k < K; ++k) { cb[(size_t) 2 * k] -= alpha * hr[(size_t) k].real(); cb[(size_t) 2 * k + 1] -= alpha * hr[(size_t) k].imag(); }
                for (int k = 0; k < K; ++k) { cb[(size_t) 2 * k] *= Gs[(size_t) k]; cb[(size_t) 2 * k + 1] *= Gs[(size_t) k]; }
                fft.performRealOnlyInverseTransform (cb.data());
                const int s = frameStart (t);
                auto& o = outCh[(size_t) c];
                for (int i = 0; i < N; ++i) { const int j = s + i; if (j >= 0 && j < n) o[(size_t) j] += cb[(size_t) i] * win[(size_t) i]; }
            }
            if ((t & 255) == 0) { if (cancel.load()) return false; prog (0.45f + 0.45f * t / nF); }
        }
        // ventanas raíz de Hann con solape del 75 %: la suma de los cuadrados vale 2
        for (int c = 0; c < nc; ++c)
        {
            float* x = R.cleaned.getWritePointer (c);
            for (int i = 0; i < n; ++i) x[i] = outCh[(size_t) c][(size_t) i] * 0.5f;
        }
        for (int f = 0; f < F; ++f) if (redCnt[(size_t) f]) R.reductionDb[(size_t) f] = (float) (redAcc[(size_t) f] / redCnt[(size_t) f]);
        // tramas sin medida: la vecina
        for (int f = 1; f < F; ++f) if (! redCnt[(size_t) f]) R.reductionDb[(size_t) f] = R.reductionDb[(size_t) f - 1];
    }
    prog (0.92f);

    // ---------------------------------------------------------------- Ganancia de limpieza por trama
    std::vector<float> g ((size_t) F, 0.0f);
    if (P.breathOn && P.breathDb > 0)
        for (const auto& b : A.breaths) { ++R.nBreaths; for (int f = b.a; f < b.b; ++f) g[(size_t) f] = std::min (g[(size_t) f], -P.breathDb * b.conf); }
    if (P.otherOn && P.otherDb > 0)
        for (const auto& o : A.others) { ++R.nOthers; for (int f = o.a; f < o.b; ++f) g[(size_t) f] = std::min (g[(size_t) f], -P.otherDb * o.conf); }
    if (P.bleedOn && P.bleedAmt > 0 && ! useRef)
    {
        // sin referencia: el sangrado sólo se puede bajar donde no cantas (con margen para no cortar colas)
        std::vector<uint8_t> keep ((size_t) F, 0);
        for (const auto& p : A.phrases) for (int f = std::max (0, p.a - 8); f < std::min (F, p.b + 30); ++f) keep[(size_t) f] = 1;
        for (const auto& b : A.breaths) for (int f = b.a; f < b.b; ++f) keep[(size_t) f] = 1;
        const float red = -18.0f * P.bleedAmt;
        for (int f = 0; f < F; ++f) if (! keep[(size_t) f]) g[(size_t) f] = std::min (g[(size_t) f], red);
    }
    // transiciones de 20 ms
    std::vector<double> cs ((size_t) F + 1, 0.0);
    for (int f = 0; f < F; ++f) cs[(size_t) f + 1] = cs[(size_t) f] + g[(size_t) f];
    for (int f = 0; f < F; ++f) { const int a = std::max (0, f - 2), b = std::min (F, f + 3); R.gainDb[(size_t) f] = (float) ((cs[(size_t) b] - cs[(size_t) a]) / (b - a)); }
    prog (1.0f);
    return ! cancel.load();
}
} // namespace levelivo

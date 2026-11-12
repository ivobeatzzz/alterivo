// levelIVO — modo TIEMPO REAL (sencillo). Causal con 300 ms de anticipación: mide la voz que viene, decide la
// ganancia con los mismos criterios (zona muerta, nunca subir pausas, límites) y la aplica suavizada sobre
// lo que sale. No segmenta en palabras ni repara clipping: para eso está el modo offline.
#pragma once
#include "../dsp/Dsp.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace levelivo
{
class RtLeveler
{
public:
    struct Settings { float intensity = 1, sensitivity = 0.4f, maxGain = 9, maxAtt = 9, speed = 0.5f; bool peakProtect = true; };

    void prepare (double sr, int maxBlock, int channels)
    {
        fs = sr; nch = std::max (1, channels);
        hop = std::max (16, (int) std::lround (sr * 0.005));
        look = (int) std::lround (sr * 0.3) / hop * hop;
        ringLen = look + maxBlock + 4 * hop;
        ring.setSize (nch, ringLen); ring.clear();
        frames = look / hop + 64;
        des.assign ((size_t) frames, 0.0f); pk.assign ((size_t) frames, 0.0f);
        kw = { dsp::Biquad::kShelf (sr), dsp::Biquad::kHighpass (sr) };
        for (auto& b : kw) b.reset();
        written = 0; acc = 0; accPk = 0; accN = 0; frameIdx = 0;
        floorDb = -70; target = -1000; lastDes = 0; gCur = 1;
        shortPow.assign (12, 1e-9);
    }
    int latency() const { return look; }

    void process (juce::AudioBuffer<float>& buf, int n, const Settings& s)
    {
        for (int i = 0; i < n; ++i)
        {
            // entrada: guardar y medir
            double m = 0; float p = 0;
            const int w = (int) (written % ringLen);
            for (int c = 0; c < nch; ++c)
            {
                const float x = buf.getSample (std::min (c, buf.getNumChannels() - 1), i);
                ring.setSample (c, w, x); m += x; p = std::max (p, std::abs (x));
            }
            m /= nch;
            for (auto& b : kw) m = b.process (m);
            acc += m * m; accPk = std::max (accPk, p); ++accN;
            if (accN == hop) { pushFrame (s); acc = 0; accPk = 0; accN = 0; }
            ++written;

            // salida: la muestra de hace 'look' muestras con la ganancia que le toca
            const juce::int64 outPos = written - 1 - look;
            float g = 1.0f;
            if (outPos >= 0)
            {
                const juce::int64 fpos = outPos / hop;
                const float gt = gainAt (fpos, s);
                gCur += 0.02f * (gt - gCur);       // interpolación suave dentro de la trama
                g = gCur;
            }
            const int r = (int) ((outPos % ringLen + ringLen) % ringLen);
            for (int c = 0; c < buf.getNumChannels(); ++c)
                buf.setSample (c, i, outPos >= 0 ? ring.getSample (std::min (c, nch - 1), r) * g : 0.0f);
            lastGainDb = dsp::linToDb (g);
        }
    }
    float currentGainDb() const { return lastGainDb; }

private:
    void pushFrame (const Settings& s)
    {
        const double pw = acc / std::max (1, accN);
        shortPow[(size_t) (frameIdx % (juce::int64) shortPow.size())] = pw;
        double sp = 0; for (double v : shortPow) sp += v; sp /= (double) shortPow.size();   // 60 ms
        const float L = dsp::powToDb (pw) - 0.691f, Ls = dsp::powToDb (sp) - 0.691f;
        // suelo adaptativo: baja rápido, sube despacio
        floorDb = L < floorDb ? L : floorDb + 0.0025f;
        const bool active = L > floorDb + 12.0f && L > -60.0f;
        if (active)
        {
            if (target < -500) target = Ls;
            target += (Ls - target) * 0.0008f;   // ~6 s
        }
        float d = 0;
        if (active && target > -500)
        {
            const float diff = target - Ls;
            const float dead = std::max (0.5f, 2.5f - 2.0f * s.sensitivity);
            const float ad = std::abs (diff);
            d = (diff >= 0 ? 1.0f : -1.0f) * std::max (0.0f, ad - dead) * s.intensity;
            d = std::clamp (d, -s.maxAtt, s.maxGain);
        }
        else d = std::min (lastDes * 0.98f, 0.0f);   // pausas: nunca subir
        lastDes = d;
        des[(size_t) (frameIdx % frames)] = d;
        pk[(size_t) (frameIdx % frames)] = accPk;
        ++frameIdx;
    }
    float gainAt (juce::int64 f, const Settings& s) const
    {
        // media de la decisión en una ventana centrada (conocemos hasta 'look' por delante)
        const int W = std::max (2, (int) std::lround ((150.0 - 130.0 * s.speed) / 5.0));
        double sum = 0; int c = 0; float peak = 0;
        for (juce::int64 k = f - W; k <= f + W; ++k)
        {
            if (k < 0 || k >= frameIdx || frameIdx - k >= frames) continue;
            sum += des[(size_t) (k % frames)]; ++c;
            if (std::abs ((int) (k - f)) <= 1) peak = std::max (peak, pk[(size_t) (k % frames)]);
        }
        float g = dsp::dbToLin (c ? (float) (sum / c) : 0.0f);
        if (s.peakProtect && peak > 1e-6f) g = std::min (g, dsp::dbToLin (-1.0f) / peak);
        return g;
    }

    double fs = 48000; int nch = 2, hop = 240, look = 14400, ringLen = 1, frames = 1;
    juce::AudioBuffer<float> ring;
    std::vector<float> des, pk;
    std::vector<double> shortPow;
    std::vector<dsp::Biquad> kw;
    juce::int64 written = 0, frameIdx = 0;
    double acc = 0; float accPk = 0; int accN = 0;
    float floorDb = -70, target = -1000, lastDes = 0, gCur = 1, lastGainDb = 0;
};
} // namespace levelivo

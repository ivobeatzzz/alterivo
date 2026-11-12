// CREATIVE ENGINE — efectos rítmicos sincronizados al tempo del proyecto. Nunca se activa solo:
// sólo funciona si el usuario (o un preset de efectos que él elige) lo enciende. No cambia la afinación.
//  STUTTER : en cada grupo de 4 divisiones, las dos últimas repiten la segunda (beat repeat).
//  GATE    : puerta rítmica (patrón 1-0-1-1) con bordes de 3 ms (sin clics).
//  REVERSE : eco invertido: cada división suena al revés la anterior, con ventana para no hacer clic.
//  PITCH   : capa con otra afinación (±24 st) mezclada con la voz: dos lectores con ventanas cruzadas
//            (desplazamiento de tono por retardo modulado). La voz original no se reafina nunca.
//  GRANULAR: nube de granos (Hann) tomados del último segundo de voz; el tamaño del grano sigue a RATE.
#pragma once
#include "../dsp/DspCore.h"

namespace chainivo
{
enum class CreaType { stutter = 0, gate, reverse, pitch, granular };

struct CreativeSettings
{
    bool on = false;
    CreaType type = CreaType::stutter;
    int note = 4;          // 1/8
    float amount = 0.5f;   // 0..1
    float pitch = -12.0f;  // semitonos (sólo PITCH)
};

class CreativeEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& b : buf) b.prepare ((int) (4.5 * sr));
        amountS.prepare (sr, 30.0f);
        gateS.prepare (sr, 3.0f);
        rptS.prepare (sr, 2.0f);
        reset();
    }
    void reset() { for (auto& b : buf) b.reset(); freePos = 0.0; first = true; }

    void process (float* const* ch, int nch, int n, const CreativeSettings& s, const TempoInfo& t)
    {
        if (first) { amountS.reset (s.on ? s.amount : 0.0f); gateS.reset (1.0f); rptS.reset (0.0f); first = false; }
        amountS.set (s.on ? s.amount : 0.0f);
        const double bpm = juce::jlimit (30.0, 300.0, t.bpm);
        const double sliceBeats = noteBeats (s.note);
        const double beatsPerSample = bpm / 60.0 / sr;
        double pos = t.playing ? t.ppq : freePos;
        const double sliceSamples = std::min (2.0 * sr, sliceBeats / beatsPerSample);

        for (int i = 0; i < n; ++i)
        {
            const float a = amountS.next();
            for (int c = 0; c < nch; ++c) buf[(size_t) c].push (ch[c][i]);
            if (a < 1.0e-4f) { pos += beatsPerSample; continue; }

            const double slicePos = pos / sliceBeats;
            const long long slice = (long long) std::floor (slicePos);
            const double phase = slicePos - (double) slice;           // 0..1 dentro de la división
            const int inGroup = (int) (((slice % 4) + 4) % 4);
            const float phaseSamples = (float) (phase * sliceSamples);

            switch (s.type)
            {
                case CreaType::pitch:
                {
                    // Dos lectores separados media ventana; el retardo cambia a ritmo (1 - ratio) y se reinicia en cada ventana
                    const float ratio = std::pow (2.0f, juce::jlimit (-24.0f, 24.0f, s.pitch) / 12.0f);
                    const float W = (float) (0.06 * sr);
                    pitchPhase += (1.0f - ratio) / W;
                    pitchPhase -= std::floor (pitchPhase);
                    float layer[kMaxCh] {};
                    for (int h = 0; h < 2; ++h)
                    {
                        const float ph = std::fmod (pitchPhase + 0.5f * (float) h, 1.0f);
                        const float d = 2.0f + ph * W;
                        const float win = std::sin (kPi * ph);
                        for (int c = 0; c < nch; ++c) layer[c] += win * win * buf[(size_t) c].readLinear (d);
                    }
                    for (int c = 0; c < nch; ++c) ch[c][i] += a * layer[c];
                    break;
                }
                case CreaType::granular:
                {
                    const float grainLen = (float) juce::jlimit (0.03 * sr, 0.25 * sr, 0.5 * sliceSamples);
                    if (--nextGrain <= 0)
                    {
                        for (auto& gr : grains)
                            if (gr.left <= 0)
                            {
                                gr.len = (int) (grainLen * (0.7f + 0.6f * rnd.nextFloat()));
                                gr.left = gr.len;
                                gr.back = (float) (gr.len + 64) + rnd.nextFloat() * (float) (0.9 * sr);
                                gr.pan = rnd.nextFloat();
                                break;
                            }
                        nextGrain = (int) (grainLen / 4.0f * (0.6f + 0.8f * rnd.nextFloat()));   // unas 4 capas a la vez
                    }
                    float l = 0.0f, r = 0.0f;
                    for (auto& gr : grains)
                        if (gr.left > 0)
                        {
                            const float ph = 1.0f - (float) gr.left / (float) gr.len;
                            const float w = std::sin (kPi * ph); const float ww = w * w * 0.5f;
                            // el grano avanza con la reproducción: se lee siempre a la misma distancia hacia atrás
                            const float v0 = buf[0].readLinear (gr.back), v1 = buf[(size_t) (nch > 1 ? 1 : 0)].readLinear (gr.back);
                            l += ww * v0 * (1.0f - gr.pan) * 2.0f; r += ww * v1 * gr.pan * 2.0f;
                            --gr.left;
                        }
                    ch[0][i] += a * l;
                    if (nch > 1) ch[1][i] += a * r; else ch[0][i] += a * r;
                    break;
                }
                case CreaType::gate:
                {
                    static const int pattern[4] = { 1, 0, 1, 1 };
                    gateS.set (pattern[inGroup] ? 1.0f : 0.0f);
                    const float g = 1.0f - a * (1.0f - gateS.next());
                    for (int c = 0; c < nch; ++c) ch[c][i] *= g;
                    break;
                }
                case CreaType::stutter:
                {
                    const bool repeat = inGroup >= 2;
                    rptS.set (repeat ? 1.0f : 0.0f);
                    const float r = rptS.next() * a;
                    // Repetir la división 1 del grupo: está (inGroup-1) divisiones atrás
                    const float back = (float) ((repeat ? inGroup - 1 : 1) * sliceSamples);
                    const float w = edge (phase, sliceSamples);
                    for (int c = 0; c < nch; ++c)
                    {
                        const float rep = buf[(size_t) c].readLinear (std::max (1.0f, back)) * w;
                        ch[c][i] = ch[c][i] * (1.0f - r) + rep * r;
                    }
                    break;
                }
                case CreaType::reverse:
                {
                    // Muestra (inicio de la división - 1 - p): retardo 2p + 1
                    const float d = 2.0f * phaseSamples + 1.0f;
                    const float w = (float) std::pow (std::sin (juce::MathConstants<double>::pi * phase), 2.0);
                    for (int c = 0; c < nch; ++c)
                        ch[c][i] += a * w * buf[(size_t) c].readLinear (std::min (d, (float) (4.4 * sr)));
                    break;
                }
            }
            pos += beatsPerSample;
        }
        if (! t.playing) freePos = pos;
    }

private:
    // Ventana con bordes de 2 ms para que las repeticiones no hagan clic
    float edge (double phase, double sliceSamples) const
    {
        const double ramp = 0.002 * sr / sliceSamples;
        if (phase < ramp) return (float) (phase / ramp);
        if (phase > 1.0 - ramp) return (float) ((1.0 - phase) / ramp);
        return 1.0f;
    }

    struct Grain { int len = 0, left = 0; float back = 0, pan = 0.5f; };
    std::array<Grain, 12> grains {};
    int nextGrain = 0;
    float pitchPhase = 0.0f;
    juce::Random rnd { 1234 };
    double sr = 48000.0, freePos = 0.0;
    bool first = true;
    std::array<DelayLine, kMaxCh> buf;
    Smooth amountS, gateS, rptS;
};
} // namespace chainivo

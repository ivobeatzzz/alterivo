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
    // Efectos por palabra / por sección
    bool wordOn = false;
    int wordTarget = 2;    // 0 todas · 1 una de 2 · 2 una de 4 · 3 fuertes · 4 primera de frase · 5 último compás de 4 · 6 último compás de 8
    int wordFx = 0;        // 0 eco · 1 stutter · 2 filtro · 3 distorsión · 4 pitch
    float wordAmount = 0.6f;
};

class CreativeEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& b : buf) b.prepare ((int) (4.5 * sr));
        amountS.prepare (sr, 30.0f);
        for (auto& b : wbuf) b.prepare ((int) (2.5 * sr));
        for (auto& b : echo) b.prepare ((int) (2.5 * sr));
        wordEnv.prepare (sr, 2.0f, 30.0f);
        wordGate.prepare (sr, 5.0f);
        wordAmtS.prepare (sr, 30.0f);
        gateS.prepare (sr, 3.0f);
        rptS.prepare (sr, 2.0f);
        reset();
    }
    void reset()
    {
        for (auto& b : buf) b.reset();
        for (auto& b : wbuf) b.reset();
        for (auto& b : echo) b.reset();
        wf.reset(); freePos = 0.0; first = true; inWord = false; quiet = 1 << 20; sinceStart = 0; wordCount = 0; wordInPhrase = 0; targeted = false;
        wordEnv.reset(); wordGate.reset (0.0f); wordAmtS.reset (0.0f);
    }
    // Para la interfaz: palabra sonando y si lleva efecto
    bool wordActive() const { return shownWord.load(); }
    bool wordTargeted() const { return shownTarget.load(); }
    int wordsDetected() const { return shownCount.load(); }

    void process (float* const* ch, int nch, int n, const CreativeSettings& s, const TempoInfo& t)
    {
        if (first) { amountS.reset (s.on ? s.amount : 0.0f); gateS.reset (1.0f); rptS.reset (0.0f); first = false; }
        processWords (ch, nch, n, s, t);
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
    // ---------------------------------------------------------------- Efectos por palabra y por sección
    // Detecta cada palabra en la voz (inicio: sube por encima del umbral tras 40 ms de valle; final: 40 ms por debajo)
    // y sólo aplica el efecto en las palabras elegidas. En las secciones, el efecto cubre el último compás de cada 4 u 8.
    void processWords (float* const* ch, int nch, int n, const CreativeSettings& s, const TempoInfo& t)
    {
        wordAmtS.set (s.wordOn ? s.wordAmount : 0.0f);
        const double bpm = juce::jlimit (30.0, 300.0, t.bpm);
        const double beatsPerSample = bpm / 60.0 / sr;
        double pos = t.playing ? t.ppq : freePosW;
        const float echoTime = (float) std::min (2.0 * sr, noteBeats (s.note) / beatsPerSample);
        const int valley = (int) (0.04 * sr), phraseGap = (int) (0.3 * sr), loudCheck = (int) (0.02 * sr);
        wf.setup (sr, 1500.0f, 1.2f);
        bool anyWord = false, anyTarget = false;
        for (int i = 0; i < n; ++i)
        {
            float m = 0.0f; for (int c = 0; c < nch; ++c) m += ch[c][i]; m /= (float) nch;
            for (int c = 0; c < nch; ++c) wbuf[(size_t) c].push (ch[c][i]);
            const float e = wordEnv.tick (powToDb (m * m));
            peakW = std::max (e, peakW - 20.0f / (3.0f * (float) sr));
            const bool loud = e > std::max (-50.0f, peakW - 26.0f);
            if (loud && ! inWord && quiet >= valley)
            {
                inWord = true; sinceStart = 0; ++wordCount;
                wordInPhrase = quiet >= phraseGap ? 0 : wordInPhrase + 1;
                switch (s.wordTarget)
                {
                    case 0: targeted = true; break;
                    case 1: targeted = (wordCount % 2) == 0; break;
                    case 2: targeted = (wordCount % 4) == 0; break;
                    case 3: targeted = false; break;   // se decide a los 20 ms
                    case 4: targeted = wordInPhrase == 0; break;
                    default: targeted = false; break;
                }
            }
            if (loud) quiet = 0; else if (++quiet >= valley) inWord = false;
            if (inWord && ++sinceStart == loudCheck && s.wordTarget == 3)
            {
                targeted = e > avgWordPeak + 3.0f;
                avgWordPeak += 0.2f * (e - avgWordPeak);
            }
            bool on = inWord && targeted;
            if (s.wordTarget >= 5)   // secciones: último compás de cada 4 u 8
            {
                const int group = s.wordTarget == 5 ? 4 : 8;
                const long long bar = (long long) std::floor (pos / (double) std::max (1, t.beatsPerBar));
                on = t.playing && ((bar % group) + group) % group == group - 1;
            }
            pos += beatsPerSample;
            wordGate.set (on ? 1.0f : 0.0f);
            const float g = wordGate.next(), a = wordAmtS.next() * g;
            anyWord |= inWord; anyTarget |= on;
            // el eco sigue sonando aunque la palabra acabe
            float ein[kMaxCh] {};
            for (int c = 0; c < nch; ++c) ein[c] = ch[c][i] * a;
            for (int c = 0; c < nch; ++c)
            {
                const float x = ch[c][i];
                float y = x;
                switch (s.wordFx)
                {
                    case 0:
                    {
                        const float d = echo[(size_t) c].readLinear (std::max (8.0f, echoTime));
                        echo[(size_t) c].push (ein[c] + 0.45f * d);
                        y = x + d;
                        break;
                    }
                    case 1:
                    {
                        const int L = std::max (64, (int) (echoTime * 0.25f));
                        const int tIn = std::max (0, sinceStart);
                        const float back = (float) (tIn - (tIn % L));
                        y = x + a * (wbuf[(size_t) c].readLinear (std::max (1.0f, back)) - x);
                        break;
                    }
                    case 2: { const auto o = wf.tick (c, x); y = x + a * (std::tanh (o.bp * wf.k * 3.0f) - x); break; }
                    case 3: { y = x + a * (std::tanh (x * 10.0f) * 0.35f - x); break; }
                    case 4:
                    {
                        const float ratio = std::pow (2.0f, juce::jlimit (-24.0f, 24.0f, s.pitch) / 12.0f);
                        const float W = (float) (0.06 * sr);
                        if (c == 0) { wPitchPhase += (1.0f - ratio) / W; wPitchPhase -= std::floor (wPitchPhase); }
                        float layer = 0.0f;
                        for (int h = 0; h < 2; ++h)
                        {
                            const float ph = std::fmod (wPitchPhase + 0.5f * (float) h, 1.0f);
                            const float w = std::sin (kPi * ph);
                            layer += w * w * wbuf[(size_t) c].readLinear (2.0f + ph * W);
                        }
                        y = x + a * layer;
                        break;
                    }
                    default: break;
                }
                ch[c][i] = y;
            }
        }
        if (! t.playing) freePosW = pos;
        shownWord.store (anyWord); shownTarget.store (anyTarget); shownCount.store (wordCount);
    }

    std::array<DelayLine, kMaxCh> wbuf, echo;
    Svf wf;
    EnvDb wordEnv;
    Smooth wordGate, wordAmtS;
    bool inWord = false, targeted = false;
    int quiet = 1 << 20, sinceStart = 0, wordCount = 0, wordInPhrase = 0;
    float peakW = -40.0f, avgWordPeak = -30.0f, wPitchPhase = 0.0f;
    double freePosW = 0.0;
    std::atomic<bool> shownWord { false }, shownTarget { false };
    std::atomic<int> shownCount { 0 };

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

// reverseIVO — PRUEBAS DEL MOTOR (sin JUCE).
// Compilar y ejecutar:
//   g++ -O2 -std=c++17 -I../Source RenderTest.cpp ../Source/render/Renderer.cpp -o rendertest && ./rendertest /tmp
// Genera una voz sintética (armónicos con vibrato, formantes, consonantes y respiración entre palabras),
// la procesa con varios ajustes y comprueba lo que se le pide al motor en el documento maestro:
// duración exacta, final justo en el golpe, sin clics, sin saturar, subida de energía, mismo resultado siempre.
#include "render/Renderer.h"
#include "analysis/Onset.h"
#include "dsp/Dsp.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

using namespace reverseivo;

static void writeWav (const std::string& path, const std::vector<float>& l, const std::vector<float>& r, int sr)
{
    std::ofstream f (path, std::ios::binary);
    const int n = (int) l.size(), bytes = n * 2 * 2;
    auto w32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto w16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    f.write ("RIFF", 4); w32 (36 + bytes); f.write ("WAVEfmt ", 8); w32 (16); w16 (1); w16 (2); w32 (sr); w32 (sr * 4); w16 (4); w16 (16);
    f.write ("data", 4); w32 (bytes);
    for (int i = 0; i < n; ++i)
        for (float v : { l[(size_t) i], r[(size_t) i] }) { const int16_t s = (int16_t) std::lround (std::clamp (v, -1.0f, 1.0f) * 32767.0f); f.write ((const char*) &s, 2); }
}

#include "Voice.h"

struct Check { int fails = 0; void expect (bool ok, const char* what) { std::printf ("  %s %s\n", ok ? "OK  " : "FALLA", what); if (! ok) ++fails; } };

static double rms (const std::vector<float>& v, int a, int b)
{
    double e = 0; for (int i = a; i < b; ++i) e += (double) v[(size_t) i] * v[(size_t) i];
    return std::sqrt (e / std::max (1, b - a));
}
// Clic = salto aislado: la segunda derivada de una muestra muy por encima de la de su entorno (10 ms).
// Así el ruido de una «s» (que tiene mucha derivada pero repartida) no cuenta como clic.
static int worstAt = 0;
static double worstClick (const Result& r, int from, int to)
{
    const int n = r.size(), w = (int) (r.sampleRate * 0.01);
    std::vector<double> dd ((size_t) n, 0.0);
    for (int i = 2; i < n; ++i)
        for (const auto* ch : { &r.l, &r.r })
            dd[(size_t) i] = std::max (dd[(size_t) i], std::abs ((double) (*ch)[(size_t) i] - 2.0 * (*ch)[(size_t) i - 1] + (*ch)[(size_t) i - 2]));
    std::vector<double> cum ((size_t) n + 1, 0.0);
    for (int i = 0; i < n; ++i) cum[(size_t) i + 1] = cum[(size_t) i] + dd[(size_t) i] * dd[(size_t) i];
    double worst = 0;
    for (int i = std::max (from, w + 2); i < std::min (to, n - w); ++i)
    {
        const double loc = std::sqrt ((cum[(size_t) (i + w)] - cum[(size_t) (i - w)]) / (2.0 * w)) + 1e-6;
        if (dd[(size_t) i] > 2e-3 && dd[(size_t) i] / loc > worst) { worst = dd[(size_t) i] / loc; worstAt = i; }
    }
    return worst;
}

int main (int argc, char** argv)
{
    const std::string outDir = argc > 1 ? argv[1] : "/tmp";
    Check c;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("\n== %.0f Hz\n", sr);
        const Source src = makeVoice (sr);
        const auto reg = analysis::detectFirstWord (src.l, src.r, sr);
        std::printf ("  región detectada: %.3f s → %.3f s\n", reg.start / sr, reg.end / sr);
        c.expect (reg.found && reg.start / sr > 0.25 && reg.start / sr < 0.31, "detecta el inicio de la primera palabra (0,30 s)");
        c.expect (reg.end / sr > 0.70 && reg.end / sr < 0.90, "detecta el final de la primera palabra (0,78 s)");

        struct Case { const char* name; RenderSettings s; Tempo t; };
        std::vector<Case> cases;
        { RenderSettings s; cases.push_back ({ "default_1bar_120", s, { 120.0, 4.0 } }); }
        { RenderSettings s; s.durationIndex = 3; s.intensity = 1.0f; s.character = 0.8f; s.pitchStart = -7; s.pitchEnd = 0; s.microPitch = 0.6f;
          s.delayOn = true; s.cutsOn = true; s.tremOn = true; s.texOn = true; s.drive = 0.6f; s.crush = 0.3f; s.noise = 0.5f; s.subDb = 4; s.airDb = 3;
          s.autoPan = 0.5f; s.movePan = 0.4f; s.moveFilter = 0.7f; s.lpStart = 1500; s.hpStart = 300; s.hpEnd = 60; s.resonance = 0.5f; s.ending = Ending::hard;
          cases.push_back ({ "extremo_2bars_140", s, { 140.0, 4.0 } }); }
        { RenderSettings s; s.fit = Fit::natural; s.durationIndex = 1; s.wash = 0.0f; s.reverbOn = false; cases.push_back ({ "natural_seco_halfbar_95", s, { 95.0, 4.0 } }); }
        { RenderSettings s; s.durationIndex = 5; s.freeSeconds = 3.333; s.stutterOn = true; s.pitch = 5; s.ending = Ending::soft; cases.push_back ({ "libre_3333ms", s, { 0.0, 4.0 } }); }
        { RenderSettings s; s.durationIndex = 4; s.texOn = true; s.mainOn = false; s.texSmear = 1.0f; cases.push_back ({ "solo_textura_4bars_174_3x4", s, { 174.0, 3.0 } }); }

        for (auto& k : cases)
        {
            const auto t0 = std::chrono::steady_clock::now();
            const auto res = render (src, reg.start, reg.end, k.s, k.t);
            const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
            std::printf (" -- %s (%.0f ms)\n", k.name, ms);
            if (res == nullptr) { c.expect (false, "genera resultado"); continue; }
            const auto& r = *res;
            const double secs = k.s.durationSeconds (k.t.bpm, k.t.beatsPerBar);
            c.expect (r.hitIndex == (int) std::llround (secs * sr), "el golpe cae en la muestra exacta de la duración musical");
            bool finite = true; float pk = 0;
            for (int i = 0; i < r.size(); ++i) { finite &= std::isfinite (r.l[(size_t) i]) && std::isfinite (r.r[(size_t) i]); pk = std::max (pk, std::max (std::abs (r.l[(size_t) i]), std::abs (r.r[(size_t) i]))); }
            c.expect (finite, "sin NaN ni infinitos");
            c.expect (pk <= dsp::dbToLin (-0.25f), "no pasa de −0,3 dBFS (Clip Safe)");
            const int N = r.hitIndex;
            const double early = rms (r.l, 0, N / 4) + rms (r.r, 0, N / 4), late = rms (r.l, N * 3 / 4, N) + rms (r.r, N * 3 / 4, N);
            std::printf ("     subida: %.1f dB  · pico %.2f dBFS · ganancia %.1f dB\n", 20 * std::log10 ((late + 1e-9) / (early + 1e-9)), dsp::linToDb (pk), r.gainDb);
            c.expect (late > early * 2.0, "la energía crece hacia el golpe (+6 dB o más)");
            const double wc = worstClick (r, 0, r.size());
            std::printf ("     peor salto/local: %.2f en %.4f s (golpe %.4f s)\n", wc, worstAt / sr, N / sr);
            c.expect (wc < 7.0, "sin clics (ningún salto aislado respecto a su entorno)");
            const float atHit = std::max (std::abs (r.l[(size_t) N - 1]), std::abs (r.r[(size_t) N - 1]));
            c.expect (! k.s.smartEnding || atHit < 0.05f || k.s.reverbOn || k.s.delayOn, "llega limpio al golpe");
            const auto again = render (src, reg.start, reg.end, k.s, k.t);
            c.expect (again != nullptr && again->l == r.l && again->r == r.r, "mismo ajuste = mismo resultado (determinista)");
            c.expect (ms < 4000.0, "se genera en menos de 4 s");
            writeWav (outDir + "/reverseivo_" + k.name + "_" + std::to_string ((int) sr) + ".wav", r.l, r.r, (int) sr);
        }
        writeWav (outDir + "/reverseivo_fuente_" + std::to_string ((int) sr) + ".wav", src.l, src.r, (int) sr);
    }
    std::printf ("\n%s (%d fallos)\n", c.fails == 0 ? "TODO OK" : "HAY FALLOS", c.fails);
    return c.fails == 0 ? 0 : 1;
}

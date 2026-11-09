// Prueba offline del motor: genera una vocal sintética, la procesa y
// guarda los resultados como .raw (float32 mono) para analizarlos.
#include "../Source/PitchEngine.h"
#include "../Source/TubeDrive.h"
#include <cstdio>
#include <string>
#include <vector>
#include <chrono>

using namespace alterivo;

static const double SR = 48000.0;

struct Biquad
{
    double b0, b1, b2, a1, a2, z1 = 0, z2 = 0;
    static Biquad bandpass (double f, double q)
    {
        const double w0 = 2 * 3.141592653589793 * f / SR, al = std::sin (w0) / (2 * q), a0 = 1 + al;
        return { al / a0, 0, -al / a0, -2 * std::cos (w0) / a0, (1 - al) / a0 };
    }
    double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// Vocal "a": pulsos glotales + 3 formantes, con vibrato opcional
static std::vector<float> makeVowel (double f0, double seconds, double vibratoCents, bool withNoiseGap)
{
    const int N = (int) (SR * seconds);
    std::vector<float> out ((size_t) N);
    Biquad f1 = Biquad::bandpass (800, 6), f2 = Biquad::bandpass (1150, 8), f3 = Biquad::bandpass (2900, 10);
    double phase = 0.0;
    unsigned rng = 12345;
    for (int i = 0; i < N; ++i)
    {
        const double t = i / SR;
        const double f = f0 * std::pow (2.0, vibratoCents / 1200.0 * std::sin (2 * 3.141592653589793 * 5.0 * t));
        phase += f / SR;
        double src = 0.0;
        if (phase >= 1.0) { phase -= 1.0; src = 1.0; }  // tren de impulsos
        bool noise = withNoiseGap && t > seconds * 0.45 && t < seconds * 0.55;
        if (noise)
        {
            rng = rng * 1664525u + 1013904223u;
            src = ((rng >> 8) / 16777216.0 - 0.5) * 0.3;
        }
        const double y = 2.0 * f1.process (src) + 1.2 * f2.process (src) + 0.6 * f3.process (src);
        out[(size_t) i] = (float) (y * 0.6);
    }
    return out;
}

static void writeRaw (const std::string& path, const std::vector<float>& v)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    std::fwrite (v.data(), sizeof (float), v.size(), f);
    std::fclose (f);
}

static std::vector<float> run (const std::vector<float>& in, Mode mode, float pitch, float formant, bool link, int& latency, double& ms)
{
    PitchEngine e;
    e.prepare (SR);
    e.setMode (mode);
    e.setPitchSemitones (pitch);
    e.setFormantSemitones (formant);
    e.setLink (link);
    latency = e.getLatencySamples();
    std::vector<float> out (in.size());
    auto t0 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < in.size(); ++i)
        out[i] = e.process (in[i]);
    auto t1 = std::chrono::high_resolution_clock::now();
    ms = std::chrono::duration<double, std::milli> (t1 - t0).count();
    return out;
}

int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : ".";
    auto vowel   = makeVowel (150.0, 3.0, 0.0, true);
    auto vibrato = makeVowel (152.0, 3.0, 80.0, false);
    writeRaw (dir + "/in_vowel.raw", vowel);
    writeRaw (dir + "/in_vibrato.raw", vibrato);

    struct Case { const char* name; const std::vector<float>* in; Mode m; float p, fo; bool link; };
    const Case cases[] = {
        { "t_p0",      &vowel,   Mode::Transpose, 0,   0,  false },
        { "t_p7",      &vowel,   Mode::Transpose, 7,   0,  false },
        { "t_m12",     &vowel,   Mode::Transpose, -12, 0,  false },
        { "t_p12",     &vowel,   Mode::Transpose, 12,  0,  false },
        { "f_p6",      &vowel,   Mode::Transpose, 0,   6,  false },
        { "f_m6",      &vowel,   Mode::Transpose, 0,  -6,  false },
        { "link_p7",   &vowel,   Mode::Transpose, 7,   0,  true  },
        { "quant",     &vibrato, Mode::Quantize,  0,   0,  false },
        { "robot",     &vibrato, Mode::Robot,     0,   0,  false },
    };

    const double secs = vowel.size() / SR;
    for (const auto& c : cases)
    {
        int lat = 0; double ms = 0;
        auto out = run (*c.in, c.m, c.p, c.fo, c.link, lat, ms);
        writeRaw (dir + "/out_" + c.name + ".raw", out);
        std::printf ("%-8s latency=%d samples (%.1f ms)  cpu=%.2f%% realtime\n", c.name, lat, lat / SR * 1000, ms / (secs * 1000) * 100);
    }

    // Drive: seno de 1 kHz a -18 dBFS
    for (float amt : { 0.3f, 0.6f, 1.0f })
    {
        TubeDrive d; d.setAmount (amt);
        DcBlocker dc; dc.prepare (SR);
        std::vector<float> s ((size_t) SR);
        for (size_t i = 0; i < s.size(); ++i)
            s[i] = dc.process (d.process (0.125f * (float) std::sin (2 * 3.141592653589793 * 1000.0 * i / SR)));
        char name[64]; std::snprintf (name, sizeof name, "/drive_%d.raw", (int) (amt * 100));
        writeRaw (dir + name, s);
    }
    return 0;
}

#include "PluginProcessor.h"
#include "ui/PluginEditor.h"
#include <cstdio>
#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <limits>
#include <numeric>
using namespace chainivo;

static std::vector<float> loadRaw (const char* path)
{
    std::vector<float> v; FILE* f = std::fopen (path, "rb"); if (! f) return v; float x;
    while (std::fread (&x, 4, 1, f) == 1) v.push_back (x);
    std::fclose (f); return v;
}
static void setP (ChainIvoProcessor& p, const char* id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
static double rms (const float* x, int n) { double s = 0; for (int i = 0; i < n; ++i) s += (double) x[i] * x[i]; return std::sqrt (s / std::max (1, n)); }
static double db (double v) { return 20.0 * std::log10 (std::max (v, 1e-12)); }

// Procesa (L,R[,lead]) en bloques de bs
static juce::AudioBuffer<float> run (ChainIvoProcessor& p, const std::vector<float>& L, const std::vector<float>& R, const std::vector<float>* lead = nullptr, int bs = 512, bool pump = false)
{
    const int n = (int) L.size();
    const int nch = p.getTotalNumInputChannels();
    juce::AudioBuffer<float> out (2, n);
    juce::MidiBuffer midi;
    for (int s = 0; s < n; s += bs)
    {
        const int m = std::min (bs, n - s);
        juce::AudioBuffer<float> blk (nch, m); blk.clear();
        for (int i = 0; i < m; ++i) { blk.setSample (0, i, L[(size_t) (s + i)]); blk.setSample (1, i, R[(size_t) (s + i)]); if (lead && nch >= 4) { blk.setSample (2, i, (*lead)[(size_t) (s + i)]); blk.setSample (3, i, (*lead)[(size_t) (s + i)]); } }
        p.processBlock (blk, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, s, blk, c, 0, m);
        if (pump && (s / bs) % 40 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
    }
    return out;
}

static void neutral (ChainIvoProcessor& p)
{
    setP (p, pid::intensity, 0); setP (p, pid::gainMatch, 0); setP (p, pid::clipSafe, 0); setP (p, pid::mix, 100);
    setP (p, pid::creaOn, 0); setP (p, pid::harmOn, 0);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    auto voice = loadRaw (argv[1]);
    if (voice.empty()) { std::printf ("sin voz\n"); return 1; }
    { float mx = 0; for (auto v : voice) mx = std::max (mx, std::abs (v)); for (auto& v : voice) v *= 0.5f / mx; }
    const int N = (int) voice.size();
    std::printf ("Voz de prueba: %.1f s\n", N / sr);
    const bool doAll = argc < 3 || juce::String (argv[2]) == "all";
    const juce::String only = argc >= 3 ? juce::String (argv[2]) : juce::String();

    // 1. Intensity 0 = transparente
    if (doAll || only == "null")
    {
        ChainIvoProcessor p; p.prepareToPlay (sr, 512); neutral (p);
        auto out = run (p, voice, voice);
        const int Lat = p.getLatencySamples();
        // la única diferencia permitida es el paso alto en 20 Hz: se compara por encima de 40 Hz
        juce::dsp::IIR::Filter<float> h1, h2; h1.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 40.0); h2.coefficients = h1.coefficients;
        std::vector<float> a ((size_t) N - (size_t) Lat), b ((size_t) N - (size_t) Lat);
        for (int i = Lat; i < N; ++i) { a[(size_t) (i - Lat)] = h1.processSample (out.getSample (0, i)); b[(size_t) (i - Lat)] = h2.processSample (voice[(size_t) (i - Lat)]); }
        double e = 0, r = 0; for (size_t i = 0; i < a.size(); ++i) { e += (a[i] - b[i]) * (a[i] - b[i]); r += b[i] * b[i]; }
        std::printf ("[null] INTENSITY 0 %%: latencia %d muestras, diferencia con la entrada %.1f dB\n", Lat, 10 * std::log10 (e / r + 1e-30));
    }

    // 2. Aliasing de HARMONICS por calidad (seno de 7.3 kHz con mucho drive)
    if (doAll || only == "alias")
    {
        for (int q = 0; q < 4; ++q)
        {
            ChainIvoProcessor p; p.setQuality (q); p.prepareToPlay (sr, 512); neutral (p);
            setP (p, pid::intensity, 100); setP (p, pid::eqOn, 0); setP (p, pid::compOn, 0); setP (p, pid::vocOn, 0); setP (p, pid::spaceOn, 0);
            setP (p, pid::harmOn, 1); setP (p, pid::harmType, 0); setP (p, pid::harmDrive, 80); setP (p, pid::harmMix, 100); setP (p, pid::harmTone, 0);
            const int n = 1 << 16; const float f0 = 7300.0f;
            std::vector<float> s ((size_t) n + 8192); for (size_t i = 0; i < s.size(); ++i) s[i] = 0.5f * std::sin (2.0f * kPi * f0 * (float) i / (float) sr);
            auto out = run (p, s, s);
            juce::dsp::FFT fft (16); std::vector<float> w ((size_t) n * 2, 0.0f);
            for (int i = 0; i < n; ++i) w[(size_t) i] = out.getSample (0, i + 8192) * (0.5f - 0.5f * std::cos (2 * kPi * i / n));
            fft.performFrequencyOnlyForwardTransform (w.data());
            double harmonic = 0, alias = 0;
            for (int k = 2; k < n / 2; ++k)
            {
                const double f = k * sr / n; const double pw = (double) w[(size_t) k] * w[(size_t) k];
                bool isH = false; for (int m = 1; m * f0 < sr / 2; ++m) if (std::abs (f - m * f0) < 30) isH = true;
                if (isH) harmonic += pw; else alias += pw;
            }
            static const char* qn[] = { "ECO   ", "NORMAL", "HIGH  ", "ULTRA " };
            std::printf ("[alias] %s latencia %3d | aliasing frente a armonicos: %.1f dB\n", qn[q], p.getLatencySamples(), 10 * std::log10 (alias / harmonic));
        }
    }

    // 3. Compresores: cada topología suena distinta (mismo umbral y ratio)
    if (doAll || only == "comp")
    {
        std::vector<juce::AudioBuffer<float>> outs;
        static const char* names[] = { "FET", "Opto", "VCA", "Vari-Mu", "Clean" };
        for (int t = 0; t < 5; ++t)
        {
            ChainIvoProcessor p; p.prepareToPlay (sr, 512); neutral (p);
            setP (p, pid::intensity, 100); setP (p, pid::eqOn, 0); setP (p, pid::vocOn, 0); setP (p, pid::spaceOn, 0);
            setP (p, pid::compOn, 1); setP (p, pid::compType, (float) t); setP (p, pid::compThresh, -40); setP (p, pid::compRatio, 4);
            setP (p, pid::compAttack, 5); setP (p, pid::compRelease, 120); setP (p, pid::compMakeup, 0); setP (p, pid::compMix, 100);
            auto out = run (p, voice, voice);
            // rango dinámico de bloques de 50 ms (p95 - p10)
            std::vector<float> lv; for (int s = 0; s + 2400 < N; s += 2400) { const double r = rms (out.getReadPointer (0) + s, 2400); if (r > 1e-3) lv.push_back ((float) db (r)); }
            std::sort (lv.begin(), lv.end());
            std::vector<float> li; for (int s = 0; s + 2400 < N; s += 2400) { const double r = rms (voice.data() + s, 2400); if (r > 1e-3) li.push_back ((float) db (r)); }
            std::sort (li.begin(), li.end());
            std::printf ("[comp] %-8s rango dinamico %.1f dB (original %.1f) | nivel medio %.1f dB\n", names[t], lv[lv.size() * 95 / 100] - lv[lv.size() / 10], li[li.size() * 95 / 100] - li[li.size() / 10], db (rms (out.getReadPointer (0), N)));
            outs.push_back (out);
        }
        std::printf ("[comp] diferencia entre topologias (dB, normalizada al nivel):\n");
        for (int a = 0; a < 5; ++a) for (int b = a + 1; b < 5; ++b)
        {
            const double ga = rms (outs[(size_t) a].getReadPointer (0), N), gb = rms (outs[(size_t) b].getReadPointer (0), N);
            double e = 0; for (int i = 0; i < N; ++i) { const double d = outs[(size_t) a].getSample (0, i) / ga - outs[(size_t) b].getSample (0, i) / gb; e += d * d; }
            std::printf ("   %s-%s %.1f", names[a], names[b], 10 * std::log10 (e / N));
        }
        std::printf ("\n");
    }

    // 4. Reverb: el tiempo de caída medido coincide con el ajustado
    if (doAll || only == "reverb")
    {
        static const char* names[] = { "Plate", "Hall", "Room", "Chamber", "Ambience" };
        for (int t = 0; t < 5; ++t)
        {
            ChainIvoProcessor p; p.prepareToPlay (sr, 512); neutral (p);
            setP (p, pid::intensity, 100); setP (p, pid::eqOn, 0); setP (p, pid::compOn, 0); setP (p, pid::vocOn, 0);
            setP (p, pid::spaceOn, 1); setP (p, pid::revType, (float) t); setP (p, pid::revMix, 50); setP (p, pid::revDecay, 2.0f); setP (p, pid::revPredelay, 0);
            setP (p, pid::revTone, 100); setP (p, pid::dlyMix, 0); setP (p, pid::duck, 0); setP (p, pid::width, 100); setP (p, pid::doubler, 0);
            const int n = (int) (5 * sr);
            std::vector<float> imp ((size_t) n, 0.0f);
            // ruido de 100 ms (banda media) y silencio: se mide la caída de la cola
            juce::Random rnd (3); for (int i = 0; i < 4800; ++i) imp[(size_t) i] = rnd.nextFloat() * 0.6f - 0.3f;
            auto out = run (p, imp, imp);
            // Schroeder: integral inversa de la energía desde 150 ms
            std::vector<double> e ((size_t) n, 0.0); double acc = 0;
            for (int i = n - 1; i >= 7200; --i) { const double v = out.getSample (0, i); acc += v * v; e[(size_t) i] = acc; }
            int i5 = -1, i25 = -1; const double e0 = e[7200];
            for (int i = 7200; i < n; ++i) { const double d = 10 * std::log10 (e[(size_t) i] / e0 + 1e-30); if (i5 < 0 && d < -5) i5 = i; if (i25 < 0 && d < -25) { i25 = i; break; } }
            const double t60 = (i25 > 0 && i5 > 0) ? (i25 - i5) / sr * 3.0 : -1;
            std::printf ("[reverb] %-8s (%d lineas) decay ajustado 2.0 s -> medido T60 %.2f s\n", names[t], p.getQuality() == 0 ? 4 : 8, t60);
        }
    }

    // 5. Mono: la anchura y el doblador no hacen desaparecer la voz en mono
    if (doAll || only == "mono")
    {
        ChainIvoProcessor p; p.prepareToPlay (sr, 512); neutral (p);
        setP (p, pid::intensity, 100); setP (p, pid::spaceOn, 1); setP (p, pid::revMix, 25); setP (p, pid::width, 200); setP (p, pid::doubler, 60); setP (p, pid::dlyMix, 0);
        auto out = run (p, voice, voice);
        double st = 0, mo = 0; for (int i = 0; i < N; ++i) { const double l = out.getSample (0, i), r = out.getSample (1, i); st += 0.5 * (l * l + r * r); mo += 0.25 * (l + r) * (l + r); }
        double corr = 0, el = 0, er = 0; for (int i = 0; i < N; ++i) { const double l = out.getSample (0, i), r = out.getSample (1, i); corr += l * r; el += l * l; er += r * r; }
        std::printf ("[mono] anchura 200 %% + doblador 60 %%: suma mono frente a estereo %.1f dB, correlacion L/R %.2f\n", 10 * std::log10 (mo / st), corr / std::sqrt (el * er));
    }

    // 6. De-esser: baja las eses y no las vocales
    if (doAll || only == "deess")
    {
        ChainIvoProcessor p; p.prepareToPlay (sr, 512); neutral (p);
        setP (p, pid::intensity, 100); setP (p, pid::eqOn, 0); setP (p, pid::compOn, 0); setP (p, pid::spaceOn, 0);
        setP (p, pid::vocOn, 1); setP (p, pid::deess, 100); setP (p, pid::deessFreq, 6500); setP (p, pid::harsh, 0); setP (p, pid::breath, 0);
        auto out = run (p, voice, voice);
        juce::dsp::IIR::Filter<float> hpI, hpO; hpI.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 4500.0); hpO.coefficients = hpI.coefficients;
        double sibIn = 0, sibOut = 0, vowIn = 0, vowOut = 0; int sib = 0, vow = 0;
        for (int s = 0; s + 960 < N; s += 960)
        {
            double eAll = 0, eHi = 0, oAll = 0, oHi = 0;
            for (int i = s; i < s + 960; ++i) { const float x = voice[(size_t) i], y = out.getSample (0, i); const float hx = hpI.processSample (x), hy = hpO.processSample (y); eAll += x * x; eHi += hx * hx; oAll += y * y; oHi += hy * hy; }
            if (eAll < 1e-4) continue;
            if (eHi / eAll > 0.4) { sibIn += eHi; sibOut += oHi; ++sib; }
            else if (eHi / eAll < 0.05) { vowIn += eAll; vowOut += oAll; ++vow; }
        }
        std::printf ("[deess] tramos de 's' (%d): agudos %.1f dB | tramos de vocal (%d): nivel %.2f dB\n", sib, 10 * std::log10 (sibOut / sibIn), vow, 10 * std::log10 (vowOut / vowIn));
    }

    // 7. CPU por calidad con todos los motores encendidos
    if (doAll || only == "cpu")
    {
        for (int q = 0; q < 4; ++q)
        {
            ChainIvoProcessor p; p.setQuality (q); p.prepareToPlay (sr, 256);
            setP (p, pid::intensity, 100); setP (p, pid::harmOn, 1); setP (p, pid::creaOn, 1); setP (p, pid::dlyMix, 30); setP (p, pid::doubler, 30);
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            run (p, voice, voice, nullptr, 256);
            const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
            static const char* qn[] = { "ECO", "NORMAL", "HIGH", "ULTRA" };
            std::printf ("[cpu] %-6s %.1f %% de un nucleo (latencia %d muestras = %.2f ms)\n", qn[q], 100.0 * ms / (N / sr * 1000.0), p.getLatencySamples(), p.getLatencySamples() * 1000.0 / sr);
        }
    }

    // 8. Estabilidad: automatización aleatoria de todos los parámetros, tamaños de bloque variables
    if (doAll || only == "stress")
    {
        ChainIvoProcessor p; p.prepareToPlay (sr, 1024); setP (p, pid::clipSafe, 0); setP (p, pid::compMakeup, 0);
        juce::Random rnd (7); int bad = 0; float peak = 0;
        std::vector<float> loop; for (int k = 0; k < 6; ++k) loop.insert (loop.end(), voice.begin(), voice.end());
        juce::MidiBuffer midi; int pos = 0;
        while (pos < (int) loop.size())
        {
            const int m = std::min ((int) loop.size() - pos, 1 + rnd.nextInt (1024));
            if (rnd.nextFloat() < 0.05f)
                for (auto* prm : p.getParameters())
                {
                    auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm);
                    const auto id = r->getParameterID();
                    // los niveles (entrada, salida, makeup) se dejan fijos: se mide la estabilidad, no cuánto se puede subir
                    if (id == pid::input || id == pid::output || id == pid::compMakeup || id == pid::clipSafe || prm == p.getBypassParameter()) continue;
                    if (rnd.nextFloat() < 0.3f) prm->setValueNotifyingHost (rnd.nextFloat());
                }
            juce::AudioBuffer<float> blk (p.getTotalNumInputChannels(), m); blk.clear();
            for (int i = 0; i < m; ++i) { blk.setSample (0, i, loop[(size_t) (pos + i)]); blk.setSample (1, i, loop[(size_t) (pos + i)]); }
            p.processBlock (blk, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < m; ++i) { const float v = blk.getSample (c, i); if (! std::isfinite (v)) ++bad; peak = std::max (peak, std::abs (v)); }
            pos += m;
        }
        std::printf ("[stress] %.0f s con automatizacion aleatoria: %d valores no validos, pico %.2f (%.1f dBFS)\n", loop.size() / sr, bad, peak, db (peak));
    }

    // 9. AUTO: clasificación y cadena con y sin voz principal
    if (doAll || only == "auto")
    {
        // a) sin voz principal: la pista es la voz de prueba sola
        {
            ChainIvoProcessor p; p.prepareToPlay (sr, 512);
            std::vector<float> loop; for (int k = 0; k < 2; ++k) loop.insert (loop.end(), voice.begin(), voice.end());
            run (p, loop, loop, nullptr, 512, true);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (900);
            const auto a = p.getAnalysis(); const auto c = p.getClassification();
            std::printf ("[auto] sin principal: %.1f s de voz, tipo %s (%.0f %%): %s\n", a.activeSeconds, voiceTypeName (c.type).toRawUTF8(), c.confidence * 100, c.reason.toRawUTF8());
            std::printf ("       medidas: actividad %.2f, frase mediana %.2f s, tono claro %.2f, sibilancia %.1f dB en %.0f Hz, graves %.1f, nasal %.1f, p90 %.1f dB\n",
                         a.activity, a.phraseMedian, a.voicedRatio, a.sibilance, a.sibilantFreq, a.mud, a.nasal, a.levelP90);
            std::printf ("       preset: %s | por que EQ: %s\n", p.getPresetName().toRawUTF8(), p.getExplanations()[0].toRawUTF8());
        }
        // b) con voz principal por sidechain: la pista es un "doble" (misma voz retrasada 25 ms y con otro color)
        {
            ChainIvoProcessor p;
            p.getBus (true, 1)->enable (true);
            p.prepareToPlay (sr, 512);
            std::vector<float> lead; for (int k = 0; k < 2; ++k) lead.insert (lead.end(), voice.begin(), voice.end());
            std::vector<float> dbl (lead.size(), 0.0f);
            juce::dsp::IIR::Filter<float> lp; lp.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, 5000.0);
            for (size_t i = 1200; i < lead.size(); ++i) dbl[i] = lp.processSample (0.6f * lead[i - 1200]);
            run (p, dbl, dbl, &lead, 512, true);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (900);
            const auto a = p.getAnalysis(); const auto c = p.getClassification();
            std::printf ("[auto] doble con principal: tipo %s (%.0f %%) | corr. envolvente %.2f, solape %.2f, mismo tono %.2f, choque banda %d (%.0f Hz) %.2f\n",
                         voiceTypeName (c.type).toRawUTF8(), c.confidence * 100, a.envelopeCorrelation, a.overlapWhenLead, a.pitchAgree, a.conflictBand, a.conflictBand >= 0 ? Analyzer::bandCentre (a.conflictBand) : 0.0f, a.conflictAmount);
            std::printf ("       por que EQ: %s\n", p.getExplanations()[0].toRawUTF8());
        }
        // c) respuesta: la pista suena sólo cuando la principal calla
        {
            ChainIvoProcessor p; p.getBus (true, 1)->enable (true); p.prepareToPlay (sr, 512);
            std::vector<float> lead, resp;
            for (int k = 0; k < 6; ++k)
            {
                const size_t seg = voice.size() / 3;
                for (size_t i = 0; i < seg; ++i) { lead.push_back (k % 2 == 0 ? voice[i] : 0.0f); resp.push_back (k % 2 == 1 ? voice[seg + i] : 0.0f); }
            }
            run (p, resp, resp, &lead, 512, true);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (900);
            const auto a = p.getAnalysis(); const auto c = p.getClassification();
            std::printf ("[auto] respuesta: tipo %s (%.0f %%) | suena con principal %.2f, sin principal %.2f\n", voiceTypeName (c.type).toRawUTF8(), c.confidence * 100, a.overlapWhenLead, a.overlapWhenSilent);
        }
    }

    // 10. Estado: guardar y recuperar deja todo igual
    if (doAll || only == "state")
    {
        ChainIvoProcessor a; a.prepareToPlay (sr, 512);
        a.loadFactoryPreset (3); a.setPro (true); a.moveEngine (4, -2); a.setLocked (1, true); a.selectAb (1); setP (a, pid::revMix, 33); a.selectAb (0);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        ChainIvoProcessor b; b.prepareToPlay (sr, 512); b.setStateInformation (mb.getData(), (int) mb.getSize());
        int diff = 0; for (int i = 0; i < a.getParameters().size(); ++i) if (std::abs (a.getParameters()[i]->getValue() - b.getParameters()[i]->getValue()) > 1e-5f) ++diff;
        const auto oa = a.getOrder(), ob = b.getOrder();
        b.selectAb (1);
        std::printf ("[state] parametros distintos %d | orden %s | PRO %d | lock COMP %d | preset '%s' | B recuperado reverb %.0f %%\n", diff, oa == ob ? "igual" : "DISTINTO",
                     (int) b.isPro(), (int) b.isLocked (1), b.getPresetName().toRawUTF8(), b.apvts.getRawParameterValue (pid::revMix)->load());
    }


    // 12. Presets: cada uno existe, tiene estilo válido y suena distinto a todos los demás
    if (doAll || only == "presets")
    {
        ChainIvoProcessor ref; const auto& recipes = ref.presets.recipes();
        std::map<juce::String, int> cats; int badGenre = 0;
        for (auto& pi : ref.presets.all()) if (pi.factory) ++cats[pi.category];
        for (auto& r : recipes) if (r.genre < 0) { ++badGenre; std::printf ("   estilo no reconocido: %s\n", r.name.toRawUTF8()); }
        std::printf ("[presets] %d presets IVO:", (int) recipes.size());
        for (auto& [c, n] : cats) std::printf (" %s %d ·", c.toRawUTF8(), n);
        std::printf (" estilos sin reconocer %d\n", badGenre);
        const int len = std::min (N, (int) (8 * sr));
        std::vector<float> seg (voice.begin(), voice.begin() + len);
        std::vector<std::vector<float>> outs;
        for (int i = 0; i < (int) recipes.size(); ++i)
        {
            ChainIvoProcessor p; p.prepareToPlay (sr, 512);
            p.loadFactoryPreset (i); setP (p, pid::intensity, 100); setP (p, pid::gainMatch, 0);
            auto out = run (p, seg, seg);
            std::vector<float> m ((size_t) len);
            double e = 0; for (int k = 0; k < len; ++k) { m[(size_t) k] = 0.5f * (out.getSample (0, k) + out.getSample (1, k)); e += m[(size_t) k] * m[(size_t) k]; }
            const float g = (float) (1.0 / std::sqrt (e / len + 1e-20)); for (auto& v : m) v *= g;   // mismo volumen: se compara el sonido, no el nivel
            outs.push_back (std::move (m));
        }
        double minD = 1e9; int ma = 0, mb = 0; int under = 0;
        for (size_t a = 0; a < outs.size(); ++a)
            for (size_t b = a + 1; b < outs.size(); ++b)
            {
                double e = 0; for (int k = 0; k < len; ++k) { const double d = outs[a][(size_t) k] - outs[b][(size_t) k]; e += d * d; }
                const double dbv = 10 * std::log10 (e / len);
                if (dbv < -20.0) { ++under; std::printf ("   parecidos: %s / %s %.1f dB\n", recipes[a].name.toRawUTF8(), recipes[b].name.toRawUTF8(), dbv); }
                if (dbv < minD) { minD = dbv; ma = (int) a; mb = (int) b; }
            }
        std::printf ("[presets] pareja mas parecida: %s / %s (diferencia %.1f dB a igual volumen) | parejas por debajo de -20 dB: %d\n",
                     recipes[(size_t) ma].name.toRawUTF8(), recipes[(size_t) mb].name.toRawUTF8(), minD, under);
    }


    // 13. Exportar WAV e instrumental de referencia
    if (doAll || only == "export")
    {
        const juce::File dir ("/tmp/claude-0/-home-claude/a71dc4c6-2070-51ff-85e0-7f07bc2eb995/scratchpad/ctest/exp");
        dir.deleteRecursively(); dir.createDirectory();
        auto writeWav = [&] (const juce::File& f, const std::vector<float>& x)
        {
            juce::WavAudioFormat w; auto os = f.createOutputStream();
            std::unique_ptr<juce::AudioFormatWriter> wr (w.createWriterFor (os.get(), sr, 1, 32, {}, 0)); os.release();
            juce::AudioBuffer<float> b (1, (int) x.size()); for (size_t i = 0; i < x.size(); ++i) b.setSample (0, (int) i, x[i]);
            wr->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        };
        const auto in = dir.getChildFile ("voz.wav"); writeWav (in, voice);
        juce::MemoryBlock origBytes; in.loadFileAsData (origBytes);
        ChainIvoProcessor p; p.prepareToPlay (sr, 512); setP (p, pid::intensity, 0); setP (p, pid::gainMatch, 0); setP (p, pid::clipSafe, 0);
        bool done = false; juce::String msg;
        const auto out = dir.getChildFile ("voz (chainIVO).wav");
        p.exportFile (in, out, true, [&] (juce::String m) { done = true; msg = m; });
        for (int i = 0; i < 600 && ! done; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (out));
        double e = 0, ref = 0;
        if (r)
        {
            juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples); r->read (&b, 0, b.getNumSamples(), 0, true, true);
            for (int i = 0; i < N; ++i) { const double d = b.getSample (0, i) - voice[(size_t) i]; e += d * d; ref += (double) voice[(size_t) i] * voice[(size_t) i]; }
            std::printf ("[export] %s | canales %d, %.0f Hz, %d bits | duracion %.2f s (original %.2f s + cola) | INTENSITY 0: diferencia con el original %.1f dB | original intacto: %s\n",
                         msg.toRawUTF8(), (int) r->numChannels, r->sampleRate, (int) r->bitsPerSample, r->lengthInSamples / sr, N / sr, 10 * std::log10 (e / ref + 1e-30),
                         [&] { juce::MemoryBlock now; in.loadFileAsData (now); return now == origBytes; }() ? "si" : "NO");
        }
        else std::printf ("[export] FALLO: %s\n", msg.toRawUTF8());
        // Instrumental: graves muy fuertes (808) y medios llenos salvo un hueco en 4 kHz
        std::vector<float> beat ((size_t) (20 * sr)); juce::Random rnd (1);
        juce::dsp::IIR::Filter<float> lp, notch; lp.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, 90.0); notch.coefficients = juce::dsp::IIR::Coefficients<float>::makeNotch (sr, 4000.0, 0.7);
        for (size_t i = 0; i < beat.size(); ++i) { const float n = rnd.nextFloat() * 2 - 1; beat[i] = 0.2f * notch.processSample (n) + 0.8f * std::sin (2 * kPi * 50.0f * (float) i / (float) sr) + 0.3f * lp.processSample (n); }
        const auto bf = dir.getChildFile ("beat.wav"); writeWav (bf, beat);
        ChainIvoProcessor q; q.prepareToPlay (sr, 512);
        q.loadInstrumental (bf);
        for (int i = 0; i < 100 && (q.isInstrumentalBusy() || q.getInstrumentalName().isEmpty()); ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        std::printf ("[beat] cargado '%s' | por que EQ: %s\n", q.getInstrumentalName().toRawUTF8(), q.getExplanations()[0].toRawUTF8());
    }


    // 14. Automatizaciones al final de frase, pitch creativo y granular
    if (doAll || only == "autom")
    {
        // frases de 0.8 s de voz seguidas de 1.2 s de silencio
        std::vector<float> ph; size_t src = 0;
        for (int k = 0; k < 4; ++k)
        {
            while (src < voice.size() && std::abs (voice[src]) < 0.05f) ++src;
            for (int i = 0; i < (int) (0.8 * sr) && src < voice.size(); ++i) ph.push_back (voice[src++] );
            for (int i = 0; i < (int) (1.2 * sr); ++i) ph.push_back (0.0f);
        }
        auto gapEnergy = [&] (float throwAmt, float swell, int& ends)
        {
            ChainIvoProcessor p; p.prepareToPlay (sr, 512); neutral (p);
            setP (p, pid::intensity, 100); setP (p, pid::eqOn, 0); setP (p, pid::compOn, 0); setP (p, pid::vocOn, 0);
            setP (p, pid::spaceOn, 1); setP (p, pid::revMix, swell > 0 ? 15 : 0); setP (p, pid::dlyMix, 0); setP (p, pid::dlyNote, 7); setP (p, pid::dlyFeedback, 0);
            setP (p, pid::duck, 0); setP (p, pid::width, 100); setP (p, pid::doubler, 0); setP (p, pid::dlyThrow, throwAmt); setP (p, pid::revSwell, swell);
            auto out = run (p, ph, ph);
            double e = 0; const int per = (int) (2.0 * sr);
            for (int k = 0; k < 4; ++k) for (int i = k * per + (int) (0.85 * sr); i < k * per + (int) (1.9 * sr) && i < (int) ph.size(); ++i) e += out.getSample (0, i) * out.getSample (0, i);
            ends = p.phraseEnds();
            return e;
        };
        int e0, e1, e2, e3;
        const double off = gapEnergy (0, 0, e0), thr = gapEnergy (100, 0, e1), swOff = gapEnergy (0, 0, e2), sw = gapEnergy (0, 100, e3);
        double revOnly; { int x; revOnly = [&] { ChainIvoProcessor p; p.prepareToPlay (sr, 512); neutral (p); setP (p, pid::intensity, 100); setP (p, pid::eqOn, 0); setP (p, pid::compOn, 0); setP (p, pid::vocOn, 0); setP (p, pid::revMix, 15); setP (p, pid::dlyMix, 0); setP (p, pid::duck, 0); setP (p, pid::doubler, 0); auto out = run (p, ph, ph); double e = 0; const int per = (int) (2.0 * sr); for (int k = 0; k < 4; ++k) for (int i = k * per + (int) (0.85 * sr); i < k * per + (int) (1.9 * sr); ++i) e += out.getSample (0, i) * out.getSample (0, i); x = 0; return e; }(); }
        std::printf ("[autom] THROW: finales de frase detectados %d de 4 | energia en los silencios: sin throw %.1f dB, con throw %.1f dB\n", e1, 10 * std::log10 (off + 1e-12), 10 * std::log10 (thr + 1e-12));
        std::printf ("[autom] SWELL: cola de reverb en los silencios %.1f dB sin swell, %.1f dB con swell (+%.1f dB)\n", 10 * std::log10 (revOnly), 10 * std::log10 (sw), 10 * std::log10 (sw / revOnly));
        (void) swOff; (void) e0; (void) e2; (void) e3;

        // Pitch: seno de 440 Hz con capa a -12 st -> aparece 220 Hz
        {
            ChainIvoProcessor p; p.prepareToPlay (sr, 512); neutral (p);
            setP (p, pid::intensity, 100); setP (p, pid::eqOn, 0); setP (p, pid::compOn, 0); setP (p, pid::vocOn, 0); setP (p, pid::spaceOn, 0);
            setP (p, pid::creaOn, 1); setP (p, pid::creaType, 3); setP (p, pid::creaPitch, -12); setP (p, pid::creaAmount, 100);
            const int n = 1 << 16; std::vector<float> s ((size_t) n + 8192); for (size_t i = 0; i < s.size(); ++i) s[i] = 0.3f * std::sin (2.0f * kPi * 440.0f * (float) i / (float) sr);
            auto out = run (p, s, s);
            juce::dsp::FFT fft (16); std::vector<float> w ((size_t) n * 2, 0.0f);
            for (int i = 0; i < n; ++i) w[(size_t) i] = out.getSample (0, i + 8192) * (0.5f - 0.5f * std::cos (2 * kPi * i / n));
            fft.performFrequencyOnlyForwardTransform (w.data());
            auto bandDb = [&] (double f) { double m = 0; for (int k = (int) ((f - 8) * n / sr); k <= (int) ((f + 8) * n / sr); ++k) m = std::max (m, (double) w[(size_t) k]); return 20 * std::log10 (m + 1e-12); };
            std::printf ("[pitch] capa -12 st sobre 440 Hz: 220 Hz a %.1f dB respecto a 440 Hz (la voz original sigue en 440)\n", bandDb (220) - bandDb (440));
        }
        // Granular: produce una nube estable
        {
            ChainIvoProcessor p; p.prepareToPlay (sr, 512); neutral (p);
            setP (p, pid::intensity, 100); setP (p, pid::creaOn, 1); setP (p, pid::creaType, 4); setP (p, pid::creaAmount, 100); setP (p, pid::spaceOn, 0);
            auto out = run (p, voice, voice);
            int bad = 0; double e = 0; for (int i = 0; i < N; ++i) { const float v = out.getSample (0, i); if (! std::isfinite (v)) ++bad; e += v * v; }
            std::printf ("[granular] %d valores no validos | nivel %.1f dB frente a la voz %.1f dB\n", bad, db (std::sqrt (e / N)), db (rms (voice.data(), N)));
        }
    }


    // 15. Reverb IR: la cola sigue a la respuesta de impulso cargada
    if (doAll || only == "ir")
    {
        const juce::File f ("/tmp/claude-0/-home-claude/a71dc4c6-2070-51ff-85e0-7f07bc2eb995/scratchpad/ctest/ir.wav");
        { // IR sintética de 0.8 s: ruido con caída exponencial (T60 ~0.8 s)
            const int n = (int) (0.8 * sr); juce::AudioBuffer<float> b (2, n); juce::Random rnd (9);
            for (int i = 0; i < n; ++i) { const float e = std::pow (10.0f, -3.0f * (float) i / (float) n); b.setSample (0, i, e * (rnd.nextFloat() * 2 - 1)); b.setSample (1, i, e * (rnd.nextFloat() * 2 - 1)); }
            f.deleteFile(); juce::WavAudioFormat w; auto os = f.createOutputStream();
            std::unique_ptr<juce::AudioFormatWriter> wr (w.createWriterFor (os.get(), sr, 2, 24, {}, 0)); os.release(); wr->writeFromAudioSampleBuffer (b, 0, n);
        }
        ChainIvoProcessor p; p.prepareToPlay (sr, 512); neutral (p);
        setP (p, pid::intensity, 100); setP (p, pid::eqOn, 0); setP (p, pid::compOn, 0); setP (p, pid::vocOn, 0);
        setP (p, pid::spaceOn, 1); setP (p, pid::revMix, 50); setP (p, pid::revPredelay, 0); setP (p, pid::revTone, 100); setP (p, pid::dlyMix, 0); setP (p, pid::duck, 0); setP (p, pid::doubler, 0);
        p.loadIR (f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
        std::vector<float> warm ((size_t) (0.5 * sr), 0.0f); run (p, warm, warm);   // deja que la IR se instale
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        const int n = (int) (2.0 * sr); std::vector<float> imp ((size_t) n, 0.0f); for (int i = 0; i < 480; ++i) imp[(size_t) i] = 0.5f * std::sin (2 * kPi * 1000.0f * i / (float) sr);
        auto out = run (p, imp, imp);
        double e1 = 0, e2 = 0, e3 = 0;
        for (int i = (int) (0.05 * sr); i < (int) (0.3 * sr); ++i) e1 += out.getSample (0, i) * out.getSample (0, i);
        for (int i = (int) (0.55 * sr); i < (int) (0.8 * sr); ++i) e2 += out.getSample (0, i) * out.getSample (0, i);
        for (int i = (int) (1.2 * sr); i < (int) (1.45 * sr); ++i) e3 += out.getSample (0, i) * out.getSample (0, i);
        std::printf ("[ir] tipo %s, IR '%s' | cola: 50-300 ms %.1f dB, 550-800 ms %.1f dB, 1.2-1.45 s %.1f dB (la IR dura 0.8 s)\n",
                     p.apvts.getParameter (pid::revType)->getCurrentValueAsText().toRawUTF8(), p.getIRName().toRawUTF8(), 10 * std::log10 (e1 + 1e-20), 10 * std::log10 (e2 + 1e-20), 10 * std::log10 (e3 + 1e-20));
    }


    // 16. Grupo: dos instancias se ven y separan su presencia y pre-delay
    if (doAll || only == "group")
    {
        ChainIvoProcessor a, b; a.prepareToPlay (sr, 512); b.prepareToPlay (sr, 512);
        std::vector<float> loop; for (int k = 0; k < 2; ++k) loop.insert (loop.end(), voice.begin(), voice.end());
        run (a, loop, loop, nullptr, 512, true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (900);
        run (b, loop, loop, nullptr, 512, true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (900);
        b.autoPropose();
        const auto an = b.getAnalysis();
        std::printf ("[group] la segunda instancia ve %d capa(s), posicion %d | pre-delay A %.0f ms, B %.0f ms | por que EQ (B): %s\n",
                     an.groupCount, an.groupIndex, a.apvts.getRawParameterValue (pid::revPredelay)->load(), b.apvts.getRawParameterValue (pid::revPredelay)->load(), b.getExplanations()[0].toRawUTF8());
    }


    // 17. Perfil personal: aprende lo que el usuario cambia después de AUTO y lo aplica la próxima vez
    if (doAll || only == "profile")
    {
        ChainIvoProcessor p; p.prepareToPlay (sr, 512);
        p.profile.reset(); p.profile.setEnabled (true);
        p.setVoiceType (0);                                            // ad-lib, propuesta AUTO
        const float autoRev = p.apvts.getRawParameterValue (pid::revMix)->load();
        setP (p, pid::revMix, autoRev + 20.0f);                         // el usuario sube la reverb y lo deja
        juce::MessageManager::getInstance()->runDispatchLoopUntil (3000);
        p.autoPropose();
        const float again = p.apvts.getRawParameterValue (pid::revMix)->load();
        std::printf ("[profile] AUTO puso reverb %.0f %%, el usuario la subio a %.0f %%; siguiente propuesta: %.0f %% | %s\n", autoRev, autoRev + 20, again,
                     p.profile.describe (voiceTypeNames()).trim().toRawUTF8());
        p.profile.reset();
        // Protección: un valor no válido en la entrada no rompe el plugin
        std::vector<float> badIn (voice.begin(), voice.begin() + 48000); badIn[1000] = std::numeric_limits<float>::quiet_NaN();
        auto out = run (p, badIn, badIn);
        int bad = 0; for (int i = 0; i < out.getNumSamples(); ++i) if (! std::isfinite (out.getSample (0, i))) ++bad;
        std::vector<float> after (voice.begin(), voice.begin() + 96000); auto out2 = run (p, after, after);
        int bad2 = 0; for (int i = 0; i < out2.getNumSamples(); ++i) if (! std::isfinite (out2.getSample (0, i))) ++bad2;
        std::printf ("[guard] NaN en la entrada: bloques recuperados %d, muestras no validas en la salida despues %d (durante: %d)\n", p.dspErrors.load(), bad2, bad);
    }


    // 18. Efectos por palabra y por sección
    if (doAll || only == "word")
    {
        // 8 "palabras" de 250 ms separadas por 150 ms de silencio, en 2 frases
        std::vector<float> w; size_t src = 0; std::vector<std::pair<int,int>> words;
        for (int ph = 0; ph < 2; ++ph)
        {
            for (int k = 0; k < 8; ++k)
            {
                while (src < voice.size() && std::abs (voice[src]) < 0.08f) ++src;
                const int start = (int) w.size();
                for (int i = 0; i < (int) (0.25 * sr); ++i) { const float env = std::min (1.0f, std::min (i, (int) (0.25 * sr) - i) / 240.0f); w.push_back (src < voice.size() ? voice[src++] * env : 0.0f); }
                words.push_back ({ start, (int) w.size() });
                for (int i = 0; i < (int) (0.15 * sr); ++i) w.push_back (0.0f);
            }
            for (int i = 0; i < (int) (0.6 * sr); ++i) w.push_back (0.0f);
        }
        auto runWord = [&] (int target, int fx, int& detected)
        {
            ChainIvoProcessor p; p.prepareToPlay (sr, 256); neutral (p);
            setP (p, pid::intensity, 100); setP (p, pid::eqOn, 0); setP (p, pid::compOn, 0); setP (p, pid::vocOn, 0); setP (p, pid::spaceOn, 0);
            setP (p, pid::wordOn, 1); setP (p, pid::wordTarget, (float) target); setP (p, pid::wordFx, (float) fx); setP (p, pid::wordAmount, 100);
            auto out = run (p, w, w, nullptr, 256);
            detected = p.wordsDetected();
            std::vector<double> diff;
            const int L = p.getLatencySamples(); for (auto [a, b] : words) { double e = 0; for (int i = a + L; i < b + L; ++i) { const double d = out.getSample (0, i) - w[(size_t) (i - L)]; e += d * d; } diff.push_back (e); }
            return diff;
        };
        int det = 0;
        auto d = runWord (2, 2, det);   // filtro en 1 de cada 4
        juce::String pattern; for (auto e : d) pattern << (e > 1e-3 ? "X" : ".");
        std::printf ("[word] 16 palabras, detectadas %d | filtro en '1 de 4': %s (X = palabra con efecto)\n", det, pattern.toRawUTF8());
        d = runWord (4, 3, det); pattern = {}; for (auto e : d) pattern << (e > 1e-3 ? "X" : ".");
        std::printf ("[word] distorsion en '1a de frase': %s\n", pattern.toRawUTF8());
        d = runWord (0, 0, det); double tail = 0;
        // eco: energia en los silencios entre palabras
        std::printf ("[word] eco en todas: diferencia media por palabra %.1f dB\n", 10 * std::log10 (std::accumulate (d.begin(), d.end(), 0.0) / d.size() + 1e-20));
        (void) tail;
        // Secciones: último compás de cada 4, con el transporte en marcha a 120 bpm
        struct Head : juce::AudioPlayHead
        {
            double ppq = 0; juce::Optional<PositionInfo> getPosition() const override
            { PositionInfo pi; pi.setBpm (120.0); pi.setPpqPosition (ppq); pi.setIsPlaying (true); pi.setTimeSignature (TimeSignature { 4, 4 }); return pi; }
        } head;
        ChainIvoProcessor p; p.prepareToPlay (sr, 512); neutral (p); p.setPlayHead (&head);
        setP (p, pid::intensity, 100); setP (p, pid::eqOn, 0); setP (p, pid::compOn, 0); setP (p, pid::vocOn, 0); setP (p, pid::spaceOn, 0);
        setP (p, pid::wordOn, 1); setP (p, pid::wordTarget, 5); setP (p, pid::wordFx, 3); setP (p, pid::wordAmount, 100);
        const int bars = 8, len = (int) (bars * 2.0 * sr); std::vector<float> tone ((size_t) len); for (int i = 0; i < len; ++i) tone[(size_t) i] = 0.2f * std::sin (2 * kPi * 220.0f * i / (float) sr);
        juce::AudioBuffer<float> out (2, len); juce::MidiBuffer midi;
        for (int s0 = 0; s0 < len; s0 += 512)
        {
            const int m = std::min (512, len - s0); juce::AudioBuffer<float> blk (p.getTotalNumInputChannels(), m); blk.clear();
            for (int i = 0; i < m; ++i) { blk.setSample (0, i, tone[(size_t) (s0 + i)]); blk.setSample (1, i, tone[(size_t) (s0 + i)]); }
            head.ppq = (double) s0 / sr * 2.0;
            p.processBlock (blk, midi); out.copyFrom (0, s0, blk, 0, 0, m);
        }
        juce::String bp;
        for (int b = 0; b < bars; ++b) { double e = 0; const int a = (int) (b * 2.0 * sr) + 4800, z = (int) ((b + 1) * 2.0 * sr) - 4800; for (int i = a; i < z; ++i) { const double dd = out.getSample (0, i) - tone[(size_t) (i - p.getLatencySamples())]; e += dd * dd; } bp << (e > 1.0 ? "X" : "."); }
        std::printf ("[seccion] distorsion en 'ultimo compas de cada 4', compases 1-8: %s\n", bp.toRawUTF8());
    }


    // 19. Momento suelto: una frase de 2.5 s en su propia pista, y luego silencio
    if (doAll || only == "short")
    {
        ChainIvoProcessor p; p.prepareToPlay (sr, 512);
        std::vector<float> clip ((size_t) (12 * sr), 0.0f); size_t src = 0;
        while (src < voice.size() && std::abs (voice[src]) < 0.05f) ++src;
        for (int i = 0; i < (int) (2.5 * sr) && src < voice.size(); ++i) clip[(size_t) (1 * sr) + (size_t) i] = voice[src++];
        run (p, clip, clip, nullptr, 512, true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (900);
        const auto a = p.getAnalysis(); const auto c = p.getClassification();
        std::printf ("[short] %.1f s de voz: listo %d, frase suelta %d | tipo %s (%.0f %%) | %s | cadena montada: %s\n", a.activeSeconds, (int) a.ready, (int) a.shortMaterial,
                     voiceTypeName (c.type).toRawUTF8(), c.confidence * 100, c.reason.toRawUTF8(), p.getExplanations()[0].isNotEmpty() ? "si" : "no");
        // Luego la pista sigue con mucha más voz (ad-libs repartidos por la canción): AUTO afina con todo
        std::vector<float> more; for (int k = 0; k < 2; ++k) more.insert (more.end(), voice.begin(), voice.end());
        run (p, more, more, nullptr, 512, true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (900);
        std::printf ("[short] despues, con %.1f s de voz: tipo %s | por que EQ: %s\n", p.getAnalysis().activeSeconds, voiceTypeName ((VoiceType) p.getVoiceType()).toRawUTF8(), p.getExplanations()[0].toRawUTF8());
    }

    // 11. Interfaz
    if (doAll || only == "ui")
    {
        ChainIvoProcessor p; p.prepareToPlay (sr, 512);
        std::vector<float> loop; for (int k = 0; k < 2; ++k) loop.insert (loop.end(), voice.begin(), voice.end());
        run (p, loop, loop, nullptr, 512, true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (900);
        for (int pro = 0; pro < 3; ++pro)
        {
            if (pro == 2) chainivo::ui::saveDarkSkin (true);
            p.setPro (pro >= 1);
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            if (pro == 1) { setP (p, pid::creaOn, 1); setP (p, pid::creaType, 3); setP (p, pid::wordOn, 1); }
            ed->setSize (ChainIvoEditor::kW, ChainIvoEditor::kH);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
            juce::File f (juce::String ("/tmp/claude-0/-home-claude/a71dc4c6-2070-51ff-85e0-7f07bc2eb995/scratchpad/ctest/ui_") + (pro == 2 ? "dark" : (pro ? "pro" : "auto")) + ".png");
            f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
            std::printf ("[ui] %s guardada\n", f.getFileName().toRawUTF8());
            if (pro == 1)
                for (int e : { 4, 5 })
                {
                    std::function<juce::Component* (juce::Component*)> find = [&] (juce::Component* c) -> juce::Component*
                    { if (c->getComponentID() == "tile" + juce::String (e)) return c; for (auto* k : c->getChildren()) if (auto* r = find (k)) return r; return nullptr; };
                    if (auto* t = dynamic_cast<chainivo::ui::ChainTile*> (find (ed.get()))) t->onSelect();
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
                    auto im2 = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
                    juce::File f2 (juce::String ("/tmp/claude-0/-home-claude/a71dc4c6-2070-51ff-85e0-7f07bc2eb995/scratchpad/ctest/ui_pro") + juce::String (e) + ".png");
                    f2.deleteFile(); juce::FileOutputStream os2 (f2); juce::PNGImageFormat().writeImageToStream (im2, os2);
                }
        }
        chainivo::ui::saveDarkSkin (false);
    }
    return 0;
}

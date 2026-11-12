#include "Analyzer.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace chainivo
{
Analyzer::Analyzer() : juce::Thread ("chainIVO analysis")
{
    fl.assign (kFifo, 0.0f); fr.assign (kFifo, 0.0f); fs.assign (kFifo, 0.0f);
    wl.assign (kFft, 0.0f); wr.assign (kFft, 0.0f); ws.assign (kFft, 0.0f);
    window.resize (kFft);
    for (int i = 0; i < kFft; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * kPi * (float) i / (float) kFft);
    work.assign (kFft * 2, 0.0f); work2.assign (kFft * 2, 0.0f);
    phrases.reserve (4096); pitchDiffs.reserve (65536);
}

Analyzer::~Analyzer() { stopThread (2000); }

float Analyzer::bandCentre (int b) { return 63.0f * std::pow (2.0f, (float) b / 3.0f); }

bool Analyzer::spectrumOfFile (const juce::File& file, std::array<float, kBands>& out, std::atomic<bool>& cancel)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate <= 0) return false;
    const double fsr = reader->sampleRate;
    const juce::int64 total = std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (fsr * 360.0));   // hasta 6 minutos
    juce::dsp::FFT f (11);
    std::vector<float> win (kFft), buf (kFft * 2);
    for (int i = 0; i < kFft; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * kPi * (float) i / (float) kFft);
    std::array<int, kBands + 1> edge {};
    for (int b = 0; b <= kBands; ++b) edge[(size_t) b] = juce::jlimit (1, kFft / 2, (int) std::lround (63.0 * std::pow (2.0, ((double) b - 0.5) / 3.0) * kFft / fsr));
    std::array<double, kBands> acc {}; int frames = 0;
    juce::AudioBuffer<float> chunk ((int) std::max (1u, reader->numChannels), kFft);
    for (juce::int64 pos = 0; pos + kFft <= total && ! cancel.load(); pos += kFft)
    {
        reader->read (&chunk, 0, kFft, pos, true, true);
        double e = 0;
        for (int i = 0; i < kFft; ++i)
        {
            float m = 0; for (int c = 0; c < chunk.getNumChannels(); ++c) m += chunk.getSample (c, i);
            m /= (float) chunk.getNumChannels(); e += (double) m * m;
            buf[(size_t) i] = m * win[(size_t) i];
        }
        if (10.0 * std::log10 (e / kFft + 1e-12) < -60.0) continue;   // silencios fuera
        std::fill (buf.begin() + kFft, buf.end(), 0.0f);
        f.performRealOnlyForwardTransform (buf.data(), true);
        double tot = 1e-12; std::array<double, kBands> band {};
        for (int i = 1; i <= kFft / 2; ++i) { const double pw = (double) buf[(size_t) (2 * i)] * buf[(size_t) (2 * i)] + (double) buf[(size_t) (2 * i + 1)] * buf[(size_t) (2 * i + 1)]; buf[(size_t) i] = (float) pw; }
        for (int b = 0; b < kBands; ++b) { for (int i = edge[(size_t) b]; i < edge[(size_t) b + 1]; ++i) band[(size_t) b] += buf[(size_t) i]; tot += band[(size_t) b]; }
        for (int b = 0; b < kBands; ++b) acc[(size_t) b] += band[(size_t) b] / tot;
        ++frames;
    }
    if (frames < 20) return false;
    for (int b = 0; b < kBands; ++b) out[(size_t) b] = powToDb ((float) (acc[(size_t) b] / frames));
    return true;
}

void Analyzer::prepare (double sampleRate)
{
    stopThread (2000);
    sr = sampleRate;
    for (int b = 0; b <= kBands; ++b)
    {
        const float edge = 63.0f * std::pow (2.0f, ((float) b - 0.5f) / 3.0f);
        bandEdge[(size_t) b] = juce::jlimit (1, kFft / 2, (int) std::lround (edge * kFft / sr));
    }
    fifo.reset();
    restartRequested = true;
    prepared = true;
    startThread (juce::Thread::Priority::low);
}

void Analyzer::releaseResources() { stopThread (2000); prepared = false; }

void Analyzer::push (const float* l, const float* r, const float* lead, int n)
{
    if (! prepared) return;
    const auto scope = fifo.write (n);   // si no cabe, escribe lo que pueda (se descarta el resto)
    auto copy = [&] (int start1, int size1, int start2, int size2)
    {
        for (int i = 0; i < size1; ++i) { fl[(size_t) (start1 + i)] = l[i]; fr[(size_t) (start1 + i)] = r[i]; fs[(size_t) (start1 + i)] = lead ? lead[i] : 0.0f; }
        for (int i = 0; i < size2; ++i) { const int k = size1 + i; fl[(size_t) (start2 + i)] = l[k]; fr[(size_t) (start2 + i)] = r[k]; fs[(size_t) (start2 + i)] = lead ? lead[k] : 0.0f; }
    };
    copy (scope.startIndex1, scope.blockSize1, scope.startIndex2, scope.blockSize2);
}

void Analyzer::restart() { restartRequested = true; }

AnalysisResult Analyzer::getResult() const
{
    const juce::ScopedLock sl (resultLock);
    return result;
}

void Analyzer::run()
{
    while (! threadShouldExit())
    {
        if (restartRequested.exchange (false))
        {
            allHist.clear(); activeHist.clear();
            frames = activeFrames = voicedFrames = leadActiveFrames = 0;
            ltas.fill (0); leadLtas.fill (0); sibBandSum.fill (0);
            sibSum = harshSum = mudSum = nasalSum = airSum = centroidSum = 0;
            corrLR = eL = eR = eS = eM = 0;
            runFrames = gapFrames = 0; phrases.clear();
            bothActive = leadOnly = leadSilentFrames = trackWhenLeadSilent = 0;
            ecSum = ecA = ecB = ecAA = ecBB = 0; ecN = 0; pitchDiffs.clear(); levelDiffSum = 0;
            filled = 0;
            const juce::ScopedLock sl (resultLock);
            result = AnalysisResult();
        }

        bool did = false;
        while (fifo.getNumReady() >= kHop && ! threadShouldExit())
        {
            const auto scope = fifo.read (kHop);
            // desplazar la ventana y añadir el salto nuevo
            std::memmove (wl.data(), wl.data() + kHop, sizeof (float) * (kFft - kHop));
            std::memmove (wr.data(), wr.data() + kHop, sizeof (float) * (kFft - kHop));
            std::memmove (ws.data(), ws.data() + kHop, sizeof (float) * (kFft - kHop));
            int k = kFft - kHop;
            for (int i = 0; i < scope.blockSize1; ++i, ++k) { wl[(size_t) k] = fl[(size_t) (scope.startIndex1 + i)]; wr[(size_t) k] = fr[(size_t) (scope.startIndex1 + i)]; ws[(size_t) k] = fs[(size_t) (scope.startIndex1 + i)]; }
            for (int i = 0; i < scope.blockSize2; ++i, ++k) { wl[(size_t) k] = fl[(size_t) (scope.startIndex2 + i)]; wr[(size_t) k] = fr[(size_t) (scope.startIndex2 + i)]; ws[(size_t) k] = fs[(size_t) (scope.startIndex2 + i)]; }
            filled = std::min (kFft, filled + kHop);
            if (filled >= kFft) { analyseFrame(); did = true; }
        }
        if (did) publish();
        wait (40);
    }
}

namespace
{
// Pico de autocorrelación normalizada entre 70 y 900 Hz (tono claro = valor alto) y su frecuencia
std::pair<float, float> periodicity (const float* power, std::vector<float>& tmp, juce::dsp::FFT& fft, int n, double sr)
{
    // power: |X|^2 en formato real (n/2+1 valores) -> autocorrelación por IFFT
    std::fill (tmp.begin(), tmp.end(), 0.0f);
    for (int i = 0; i <= n / 2; ++i) tmp[(size_t) (2 * i)] = power[i];
    fft.performRealOnlyInverseTransform (tmp.data());
    const float r0 = tmp[0];
    if (r0 <= 1.0e-12f) return { 0.0f, 0.0f };
    const int lo = (int) (sr / 900.0), hi = std::min (n / 2 - 1, (int) (sr / 70.0));
    float best = 0.0f; int bestLag = 0;
    for (int lag = lo; lag <= hi; ++lag)
    {
        const float v = tmp[(size_t) lag] / r0 * (float) n / (float) (n - lag);   // corrección del sesgo de la ventana
        if (v > best && tmp[(size_t) lag] >= tmp[(size_t) (lag - 1)] && tmp[(size_t) lag] >= tmp[(size_t) (lag + 1)]) { best = v; bestLag = lag; }
    }
    return { best, bestLag > 0 ? (float) (sr / bestLag) : 0.0f };
}
} // namespace

void Analyzer::analyseFrame()
{
    ++frames;
    // Energías en el tiempo
    double sl = 0, sr2 = 0, slr = 0, ss = 0;
    for (int i = 0; i < kFft; ++i)
    {
        const double a = wl[(size_t) i], b = wr[(size_t) i];
        sl += a * a; sr2 += b * b; slr += a * b; ss += (double) ws[(size_t) i] * ws[(size_t) i];
    }
    const float trackDb = powToDb ((float) ((sl + sr2) * 0.5 / kFft));
    const float leadDb = powToDb ((float) (ss / kFft));
    allHist.add (trackDb);

    // Espectro de la pista (suma mono)
    auto spectrumOf = [&] (const std::vector<float>& a, const std::vector<float>* b, std::vector<float>& out)
    {
        std::fill (out.begin(), out.end(), 0.0f);
        for (int i = 0; i < kFft; ++i) out[(size_t) i] = (b ? 0.5f * (a[(size_t) i] + (*b)[(size_t) i]) : a[(size_t) i]) * window[(size_t) i];
        fft.performRealOnlyForwardTransform (out.data(), true);
        for (int i = 0; i <= kFft / 2; ++i) { const float re = out[(size_t) (2 * i)], im = out[(size_t) (2 * i + 1)]; out[(size_t) i] = re * re + im * im; }
    };
    spectrumOf (wl, &wr, work);
    std::array<float, kBands> band {};
    double total = 1.0e-12, sib = 0, harsh = 0, mud = 0, nasal = 0, air = 0, cent = 0;
    for (int i = 1; i <= kFft / 2; ++i)
    {
        const double p = work[(size_t) i]; const double f = (double) i * sr / kFft;
        if (f < 60.0) continue;
        total += p; cent += p * f;
        if (f >= 5000 && f < 10000) sib += p;
        if (f >= 2500 && f < 5000) harsh += p;
        if (f >= 200 && f < 500) mud += p;
        if (f >= 800 && f < 1500) nasal += p;
        if (f >= 10000 && f < 16000) air += p;
    }
    for (int b = 0; b < kBands; ++b)
    {
        double s = 0; for (int i = bandEdge[(size_t) b]; i < bandEdge[(size_t) b + 1]; ++i) s += work[(size_t) i];
        band[(size_t) b] = (float) s;
    }

    // Actividad: por encima del suelo de ruido medido (+15 dB) y de -60 dBFS
    const float floorNow = allHist.pct (0.1f);
    const bool active = trackDb > std::max (-60.0f, floorNow + 15.0f);
    const bool leadActive = ss > 0.0 && leadDb > -55.0f;

    float pitchTrack = 0.0f;
    if (active)
    {
        ++activeFrames; activeHist.add (trackDb);
        for (int b = 0; b < kBands; ++b) ltas[(size_t) b] += band[(size_t) b] / total;
        sibSum += powToDb ((float) (sib / total)); harshSum += powToDb ((float) (harsh / total));
        mudSum += powToDb ((float) (mud / total)); nasalSum += powToDb ((float) (nasal / total));
        airSum += powToDb ((float) (air / total)); centroidSum += cent / total;
        if (sib / total > 0.25) for (int b = 0; b < kBands; ++b) if (bandCentre (b) >= 4000.0f && bandCentre (b) <= 11000.0f) sibBandSum[(size_t) b] += band[(size_t) b];
        auto [per, f0] = periodicity (work.data(), work2, fft, kFft, sr);
        if (per > 0.55f) { ++voicedFrames; pitchTrack = f0; }
        corrLR += slr; eL += sl; eR += sr2;
        eM += (sl + sr2 + 2 * slr) * 0.25; eS += (sl + sr2 - 2 * slr) * 0.25;
        ++runFrames; gapFrames = 0;
    }
    else
    {
        if (runFrames > 0 && ++gapFrames > (int) (0.15 * sr / kHop))
        {
            if (phrases.size() < 4096) phrases.push_back ((float) (runFrames * kHop / sr));
            runFrames = 0;
        }
    }

    // Voz principal
    if (leadActive)
    {
        ++leadActiveFrames;
        spectrumOf (ws, nullptr, work);
        double lt = 1.0e-12; std::array<float, kBands> lb {};
        for (int i = 2; i <= kFft / 2; ++i) lt += work[(size_t) i];
        for (int b = 0; b < kBands; ++b) { double s = 0; for (int i = bandEdge[(size_t) b]; i < bandEdge[(size_t) b + 1]; ++i) s += work[(size_t) i]; lb[(size_t) b] = (float) s; }
        for (int b = 0; b < kBands; ++b) leadLtas[(size_t) b] += lb[(size_t) b] / lt;
        if (active)
        {
            ++bothActive;
            levelDiffSum += trackDb - leadDb;
            auto [lper, lf0] = periodicity (work.data(), work2, fft, kFft, sr);
            if (lper > 0.55f && pitchTrack > 0.0f && pitchDiffs.size() < 65536)
                pitchDiffs.push_back (12.0f * std::log2 (pitchTrack / lf0));
        }
    }
    else if (ss > 0.0 || leadActiveFrames > 0)
    {
        ++leadSilentFrames;
        if (active) ++trackWhenLeadSilent;
    }
    if (leadActiveFrames > 0 && (active || leadActive))
    {
        const double a = std::max (-70.0f, trackDb), b = std::max (-70.0f, leadDb);
        ecA += a; ecB += b; ecAA += a * a; ecBB += b * b; ecSum += a * b; ++ecN;
    }
}

void Analyzer::publish()
{
    AnalysisResult r;
    const double hopSec = kHop / sr;
    r.totalSeconds = (float) (frames * hopSec);
    r.activeSeconds = (float) (activeFrames * hopSec);
    r.progress = juce::jlimit (0.0f, 1.0f, r.activeSeconds / 8.0f);
    r.ready = r.activeSeconds >= 6.0f;
    if (activeFrames > 0)
    {
        const double n = activeFrames;
        r.levelP50 = activeHist.pct (0.5f); r.levelP90 = activeHist.pct (0.9f);
        r.noiseFloor = allHist.pct (0.05f);
        r.dynamicSpread = activeHist.pct (0.95f) - activeHist.pct (0.1f);
        r.sibilance = (float) (sibSum / n); r.harsh = (float) (harshSum / n); r.mud = (float) (mudSum / n);
        r.nasal = (float) (nasalSum / n); r.air = (float) (airSum / n); r.centroid = (float) (centroidSum / n);
        r.voicedRatio = (float) (voicedFrames / n);
        r.activity = frames > 0 ? (float) activeFrames / (float) frames : 0.0f;
        for (int b = 0; b < kBands; ++b) r.spectrum[(size_t) b] = powToDb ((float) (ltas[(size_t) b] / n));
        int best = -1; double bv = 0;
        for (int b = 0; b < kBands; ++b) if (sibBandSum[(size_t) b] > bv) { bv = sibBandSum[(size_t) b]; best = b; }
        if (best >= 0) r.sibilantFreq = bandCentre (best);
        r.stereoCorrelation = (eL > 0 && eR > 0) ? (float) (corrLR / std::sqrt (eL * eR)) : 1.0f;
        r.sideRatio = (eM + eS) > 0 ? (float) (eS / (eM + eS)) : 0.0f;
    }
    std::vector<float> ph (phrases);
    if (runFrames > 0) ph.push_back ((float) (runFrames * hopSec));
    if (! ph.empty())
    {
        std::sort (ph.begin(), ph.end());
        r.phraseMedian = ph[ph.size() / 2];
        r.phraseShortRatio = (float) std::count_if (ph.begin(), ph.end(), [] (float d) { return d < 0.25f; }) / (float) ph.size();
    }

    r.hasLead = leadActiveFrames * hopSec > 3.0;
    if (r.hasLead)
    {
        for (int b = 0; b < kBands; ++b) r.leadSpectrum[(size_t) b] = powToDb ((float) (leadLtas[(size_t) b] / leadActiveFrames));
        r.overlapWhenLead = (float) bothActive / (float) leadActiveFrames;
        r.overlapWhenSilent = leadSilentFrames > 0 ? (float) trackWhenLeadSilent / (float) leadSilentFrames : 0.0f;
        if (ecN > 10)
        {
            const double ma = ecA / ecN, mb = ecB / ecN;
            const double cov = ecSum / ecN - ma * mb, va = ecAA / ecN - ma * ma, vb = ecBB / ecN - mb * mb;
            r.envelopeCorrelation = (va > 0 && vb > 0) ? (float) (cov / std::sqrt (va * vb)) : 0.0f;
        }
        if (! pitchDiffs.empty())
        {
            std::vector<float> d (pitchDiffs);
            // reducir a la octava más cercana para comparar notas
            for (auto& v : d) v = v - 12.0f * std::round (v / 12.0f);
            std::nth_element (d.begin(), d.begin() + (long) d.size() / 2, d.end());
            r.pitchOffsetSemis = d[d.size() / 2];
            r.pitchAgree = (float) std::count_if (d.begin(), d.end(), [] (float v) { return std::abs (v) < 0.5f; }) / (float) d.size();
        }
        if (bothActive > 0) r.levelVsLead = (float) (levelDiffSum / bothActive);
        // Choque: en 1.5-6 kHz, dónde las dos voces tienen más energía a la vez (cada una relativa a su media)
        float meanT = 0, meanL = 0;
        for (int b = 0; b < kBands; ++b) { meanT += r.spectrum[(size_t) b]; meanL += r.leadSpectrum[(size_t) b]; }
        meanT /= kBands; meanL /= kBands;
        float bestScore = -100.0f;
        for (int b = 0; b < kBands; ++b)
        {
            const float f = bandCentre (b);
            if (f < 1500.0f || f > 6000.0f) continue;
            const float score = std::min (r.spectrum[(size_t) b] - meanT, r.leadSpectrum[(size_t) b] - meanL);
            if (score > bestScore) { bestScore = score; r.conflictBand = b; }
        }
        r.conflictAmount = juce::jlimit (0.0f, 1.0f, (bestScore + 2.0f) / 8.0f) * juce::jlimit (0.0f, 1.0f, r.overlapWhenLead * 1.5f);
    }
    const juce::ScopedLock sl (resultLock);
    result = r;
}
} // namespace chainivo

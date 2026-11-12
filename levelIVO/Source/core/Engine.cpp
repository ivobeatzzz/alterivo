#include "Engine.h"

namespace levelivo
{
namespace
{
std::vector<float> cleanLoudnessFrames (const CleanResult& c, const Analysis& A, double sr)
{
    const int n = c.cleaned.getNumSamples(), nc = c.cleaned.getNumChannels();
    std::vector<float> m ((size_t) n, 0.0f);
    for (int ch = 0; ch < nc; ++ch) { const float* p = c.cleaned.getReadPointer (ch); for (int i = 0; i < n; ++i) m[(size_t) i] += p[i] / (float) nc; }
    dsp::runChain ({ dsp::Biquad::kShelf (sr), dsp::Biquad::kHighpass (sr) }, m);
    auto L = dsp::framePowerDb (m, A.hop, 2 * A.hop, A.nFrames);
    for (auto& v : L) v -= 0.691f;
    return L;
}
float activeMean (const Analysis& A, const std::vector<float>& L, const std::vector<float>* g1, const std::vector<float>* g2)
{
    double s = 0; int c = 0;
    for (int f = 0; f < A.nFrames; ++f)
    {
        if (! A.active[(size_t) f]) continue;
        float v = L[(size_t) f];
        if (g1) v += (*g1)[(size_t) f];
        if (g2) v += (*g2)[(size_t) f];
        s += std::pow (10.0, v / 10.0); ++c;
    }
    return c ? dsp::powToDb (s / c) : -120.0f;
}
// Datos de la limpieza que no cambian al mover la nivelación
struct CleanExtras : CleanResult
{
    std::vector<float> L;       // sonoridad por trama del audio limpio
    std::vector<float> peaks;   // picos por trama del audio limpio
};
}

Engine::Engine() : juce::Thread ("levelIVO engine") { startThread (juce::Thread::Priority::low); }
Engine::~Engine() { cancel = true; signalThreadShouldExit(); wake.signal(); stopThread (8000); }

void Engine::setTake (TakePtr t)
{
    { std::lock_guard<std::mutex> l (inMutex); take = std::move (t); }
    cancel = true; pending.fetch_or (1); wake.signal();
}
void Engine::clear()
{
    { std::lock_guard<std::mutex> l (inMutex); take.reset(); }
    cancel = true; pending.fetch_or (1); wake.signal();
}
void Engine::setCleanParams (const CleanParams& p)
{
    { std::lock_guard<std::mutex> l (inMutex); cp = p; }
    if (stage() == Stage::cleaning) cancel = true;
    pending.fetch_or (2); wake.signal();
}
void Engine::setLevelParams (const LevelParams& p, const std::vector<Edit>& e)
{
    { std::lock_guard<std::mutex> l (inMutex); lp = p; edits = e; }
    pending.fetch_or (4); wake.signal();
}

void Engine::finish (Result& r, const LevelParams& lpar, const std::vector<Edit>& ed)
{
    const auto& A = *r.analysis;
    const auto* cx = static_cast<const CleanExtras*> (r.clean.get());
    auto lev = std::make_shared<LevelResult>();
    Leveler::decide (A, lpar, ed, cx->peaks, cx->gainDb, *lev);
    r.levelLin = render::framesToLinear (lev->gainDb, A.hop, r.take->numSamples());
    // compensación de volumen: todos los modos de escucha suenan igual de fuertes que el original
    const float lo = activeMean (A, A.L, nullptr, nullptr);
    const float ll = activeMean (A, A.L, &lev->gainDb, nullptr);
    const float lc = activeMean (A, cx->L, &cx->gainDb, nullptr);
    const float lf = activeMean (A, cx->L, &cx->gainDb, &lev->gainDb);
    r.comp[0] = 1.0f;
    r.comp[1] = dsp::dbToLin (std::clamp (lo - ll, -18.0f, 18.0f));
    r.comp[2] = dsp::dbToLin (std::clamp (lo - lc, -18.0f, 18.0f));
    r.comp[3] = dsp::dbToLin (std::clamp (lo - lf, -18.0f, 18.0f));
    r.level = lev;
}

ResultPtr Engine::relevel (const ResultPtr& base, const LevelParams& lpar, const std::vector<Edit>& ed)
{
    if (base == nullptr || base->analysis == nullptr || base->clean == nullptr) return base;
    auto r = std::make_shared<Result> (*base);
    finish (*r, lpar, ed);
    r->version = base->version + 1;
    return r;
}

ResultPtr Engine::computeNow (TakePtr t, const CleanParams& c, const LevelParams& l, const std::vector<Edit>& ed)
{
    std::atomic<bool> no { false };
    auto A = std::make_shared<Analysis>();
    Analyzer::analyze (*t, *A, no);
    auto cx = std::make_shared<CleanExtras>();
    Cleaner::process (*t, *A, c, *cx, no);
    cx->L = cleanLoudnessFrames (*cx, *A, t->sampleRate);
    cx->peaks = render::framePeaks (cx->cleaned, A->hop, A->nFrames);
    auto r = std::make_shared<Result>();
    r->take = t; r->analysis = A; r->clean = cx;
    r->cleanLin = render::framesToLinear (cx->gainDb, A->hop, t->numSamples());
    finish (*r, l, ed);
    return r;
}

void Engine::run()
{
    while (! threadShouldExit())
    {
        if (pending.load() == 0) { stageNow = (int) Stage::idle; wake.wait (500); continue; }
        int bits = pending.exchange (0);
        cancel = false;
        TakePtr t; CleanParams c; LevelParams l; std::vector<Edit> ed;
        { std::lock_guard<std::mutex> lk (inMutex); t = take; c = cp; l = lp; ed = edits; }

        if (t == nullptr || t->numSamples() == 0)
        {
            doneAnalysis.reset(); doneClean.reset();
            { std::lock_guard<std::mutex> lk (resMutex); result.reset(); }
            if (onResult) onResult();
            continue;
        }
        if ((bits & 1) || doneAnalysis == nullptr)
        {
            stageNow = (int) Stage::analyzing; prog = 0;
            auto A = std::make_shared<Analysis>();
            if (! Analyzer::analyze (*t, *A, cancel, [this] (float p) { prog = p; })) { pending.fetch_or (bits | 1); continue; }
            doneAnalysis = A; doneClean.reset();
            bits |= 2 | 4;
        }
        if ((bits & 2) || doneClean == nullptr)
        {
            stageNow = (int) Stage::cleaning; prog = 0;
            auto cx = std::make_shared<CleanExtras>();
            if (! Cleaner::process (*t, *doneAnalysis, c, *cx, cancel, [this] (float p) { prog = p; })) { pending.fetch_or (bits | 2); continue; }
            cx->L = cleanLoudnessFrames (*cx, *doneAnalysis, t->sampleRate);
            cx->peaks = render::framePeaks (cx->cleaned, doneAnalysis->hop, doneAnalysis->nFrames);
            doneClean = cx;
            doneCleanLin = render::framesToLinear (cx->gainDb, doneAnalysis->hop, t->numSamples());
            bits |= 4;
        }
        if (cancel.load()) { pending.fetch_or (bits); continue; }
        stageNow = (int) Stage::leveling;
        auto r = std::make_shared<Result>();
        r->take = t; r->analysis = doneAnalysis; r->clean = doneClean; r->cleanLin = doneCleanLin;
        finish (*r, l, ed);
        r->version = ++version;
        { std::lock_guard<std::mutex> lk (resMutex); result = r; }
        if (pending.load() == 0) stageNow = (int) Stage::idle;
        if (onResult) onResult();
    }
}
} // namespace levelivo

#include "Leveler.h"
#include "../dsp/Dsp.h"

namespace levelivo
{
using namespace dsp;

namespace
{
// Zona muerta suave: dentro de ±dead no se corrige (variación natural); fuera, sólo lo que sobra
float softDead (float d, float dead)
{
    const float ad = std::abs (d);
    if (dead <= 1e-3f) return d;
    return (d >= 0 ? 1.0f : -1.0f) * std::max (0.0f, ad - dead) * (1.0f - std::exp (-ad / dead));
}
bool overlaps (int a, int b, int c, int d) { return a < d && c < b; }
}

void Leveler::decide (const Analysis& A, const LevelParams& P, const std::vector<Edit>& edits,
                      const std::vector<float>& framePeakDb, const std::vector<float>& extraGainDb, LevelResult& R)
{
    const int F = A.nFrames;
    R = LevelResult();
    R.gainDb.assign ((size_t) F, 0.0f);
    R.targetDb.assign ((size_t) F, 0.0f);
    R.words.assign (A.words.size(), WordInfo());
    if (F <= 0 || A.words.empty()) return;
    const int hop = A.hop;

    // Ediciones a tramas
    struct FE { int kind, a, b; float v; };
    std::vector<FE> fe;
    for (const auto& e : edits) fe.push_back ({ e.kind, std::clamp (e.a / hop, 0, F), std::clamp ((e.b + hop - 1) / hop, 0, F), e.value });

    // Región de cada trama (para que nada se mezcle entre tramos capturados por separado)
    std::vector<int> regionOf ((size_t) F, 0);
    { int r = -1; for (int f = 0; f < F; ++f) { if (A.regionStart[(size_t) f]) ++r; regionOf[(size_t) f] = std::max (0, r); } }

    // Palabras que cuentan: las de la voz principal
    std::vector<uint8_t> isOther ((size_t) F, 0), isBreath ((size_t) F, 0);
    for (const auto& o : A.others) for (int f = o.a; f < o.b; ++f) isOther[(size_t) f] = 1;
    for (const auto& b : A.breaths) for (int f = b.a; f < b.b; ++f) isBreath[(size_t) f] = 1;

    const int NW = (int) A.words.size();
    std::vector<float> lv ((size_t) NW), cen ((size_t) NW);
    std::vector<uint8_t> use ((size_t) NW, 1);
    for (int i = 0; i < NW; ++i)
    {
        const auto& w = A.words[(size_t) i];
        lv[(size_t) i] = A.segLevel (w);
        cen[(size_t) i] = 0.5f * (w.a + w.b);
        int oth = 0; for (int f = w.a; f < w.b; ++f) oth += isOther[(size_t) f];
        if (oth * 2 > w.len() || w.len() < 4) use[(size_t) i] = 0;
        R.words[(size_t) i].levelDb = lv[(size_t) i];
        R.words[(size_t) i].conf = w.conf;
        for (const auto& e : fe)
            if (overlaps (w.a, w.b, e.a, e.b) && e.kind == Edit::expression) R.words[(size_t) i].expression = true;
    }
    std::vector<float> used;
    for (int i = 0; i < NW; ++i) if (use[(size_t) i]) used.push_back (lv[(size_t) i]);
    if (used.empty()) return;
    const float T = median (used);

    // Objetivo que sigue al beat (opcional): en las partes donde la música sube, la voz acompaña
    std::vector<float> Ti ((size_t) NW, T);
    if (P.beatFollow > 0.0f && ! A.refL.empty())
    {
        std::vector<float> bl ((size_t) NW);
        for (int i = 0; i < NW; ++i) bl[(size_t) i] = energyMeanDb (A.refL.data() + A.words[(size_t) i].a, A.words[(size_t) i].len());
        std::vector<float> bu; for (int i = 0; i < NW; ++i) if (use[(size_t) i]) bu.push_back (bl[(size_t) i]);
        const float bm = median (bu);
        for (int i = 0; i < NW; ++i) Ti[(size_t) i] = T + std::clamp (P.beatFollow * (bl[(size_t) i] - bm), -6.0f, 6.0f);
    }

    // 1) Tendencia: mediana de las palabras en ±2 s (sigue la distancia al micro, no cada palabra)
    std::vector<float> trend ((size_t) NW, T);
    for (int i = 0; i < NW; ++i)
    {
        std::vector<float> nb;
        for (int j = 0; j < NW; ++j)
            if (use[(size_t) j] && std::abs (cen[(size_t) j] - cen[(size_t) i]) <= 400.0f
                && regionOf[(size_t) A.words[(size_t) j].a] == regionOf[(size_t) A.words[(size_t) i].a])
                nb.push_back (lv[(size_t) j]);
        if (! nb.empty()) trend[(size_t) i] = median (nb);
    }
    // 2) Zona muerta adaptada a esta toma: sólo se corrigen las palabras que se salen de su dispersión típica
    std::vector<float> dev;
    for (int i = 0; i < NW; ++i) if (use[(size_t) i]) dev.push_back (std::abs (trend[(size_t) i] - lv[(size_t) i]));
    const float pct = 90.0f - 50.0f * std::clamp (P.sensitivity, 0.0f, 1.0f);
    const float deadWord = std::max (1.0f, percentile (dev, pct));
    const float deadTrend = std::max (0.5f, 2.5f - 2.0f * P.sensitivity);

    std::vector<float> gw ((size_t) NW, 0.0f);
    for (int i = 0; i < NW; ++i)
    {
        auto& info = R.words[(size_t) i];
        info.trendDb = trend[(size_t) i];
        if (! use[(size_t) i]) { info.reason = 0; continue; }
        const float gT = P.phrase * softDead (Ti[(size_t) i] - trend[(size_t) i], deadTrend);
        const float d = trend[(size_t) i] - lv[(size_t) i];
        const float gWd = info.expression ? 0.0f : P.word * A.words[(size_t) i].conf * softDead (d, deadWord);
        float g = P.intensity * (gT + gWd);
        g = std::clamp (g, -P.maxAtt, P.maxGain);
        gw[(size_t) i] = g;
        info.corrDb = g;
        if (std::abs (gWd) > std::abs (gT) && std::abs (gWd) > 0.5f) info.reason = d > 0 ? 2 : 3;
        else if (std::abs (gT) > 0.5f) info.reason = 1;
    }

    // Curva objetivo por trama: valor de cada palabra; huecos dentro de la frase interpolados
    std::vector<float> tgt ((size_t) F, std::numeric_limits<float>::quiet_NaN());
    for (int i = 0; i < NW; ++i)
        if (use[(size_t) i]) for (int f = A.words[(size_t) i].a; f < A.words[(size_t) i].b; ++f) tgt[(size_t) f] = gw[(size_t) i];
    // 3) Sílabas que se pierden frente a la más fuerte de su palabra (sólo realce y limitado)
    if (P.syllable > 0.0f)
    {
        std::vector<float> top ((size_t) NW, -200.0f);
        std::vector<float> sl (A.syllables.size());
        for (size_t s = 0; s < A.syllables.size(); ++s)
        {
            sl[s] = A.segLevel (A.syllables[s]);
            const int w = A.syllables[s].parent;
            if (w >= 0) top[(size_t) w] = std::max (top[(size_t) w], sl[s]);
        }
        for (size_t s = 0; s < A.syllables.size(); ++s)
        {
            const auto& S = A.syllables[s];
            const int w = S.parent;
            if (w < 0 || ! use[(size_t) w] || R.words[(size_t) w].expression) continue;
            const float dd = top[(size_t) w] - sl[s] - 6.0f;
            if (dd <= 0) continue;
            const float add = P.intensity * P.syllable * S.conf * std::min (dd, 4.0f);
            for (int f = S.a; f < S.b; ++f) if (! std::isnan (tgt[(size_t) f])) tgt[(size_t) f] = std::min (tgt[(size_t) f] + add, P.maxGain);
            if (add > 0.5f && R.words[(size_t) w].reason == 0) R.words[(size_t) w].reason = 4;
        }
    }
    for (const auto& ph : A.phrases)
    {
        int last = -1;
        for (int f = ph.a; f < ph.b; ++f)
        {
            if (std::isnan (tgt[(size_t) f])) continue;
            if (last >= 0 && f - last > 1)
                for (int k = last + 1; k < f; ++k)
                {
                    const float t = (float) (k - last) / (float) (f - last);
                    tgt[(size_t) k] = tgt[(size_t) last] + t * (tgt[(size_t) f] - tgt[(size_t) last]);
                }
            last = f;
        }
    }
    // Fuera de la voz: nunca subir (pausas, ruido, respiraciones); se mantiene el valor vecino sólo si es bajada
    {
        float hold = 0;
        for (int f = 0; f < F; ++f)
        {
            if (A.regionStart[(size_t) f]) hold = 0;
            if (! std::isnan (tgt[(size_t) f])) hold = tgt[(size_t) f];
            else tgt[(size_t) f] = std::min (0.0f, hold);
            if (isBreath[(size_t) f] || isOther[(size_t) f]) tgt[(size_t) f] = std::min (tgt[(size_t) f], 0.0f);
        }
    }
    // Ediciones manuales
    for (const auto& e : fe)
    {
        if (e.kind == Edit::offset) for (int f = e.a; f < e.b; ++f) tgt[(size_t) f] += e.v;
        if (e.kind == Edit::lock)   for (int f = e.a; f < e.b; ++f) tgt[(size_t) f] = e.v;
    }
    for (int i = 0; i < NW; ++i)
        for (const auto& e : fe)
            if (overlaps (A.words[(size_t) i].a, A.words[(size_t) i].b, e.a, e.b))
            {
                if (e.kind == Edit::lock) { R.words[(size_t) i].locked = true; R.words[(size_t) i].reason = 6; }
                if (e.kind == Edit::offset) R.words[(size_t) i].reason = 5;
            }
    R.targetDb = tgt;

    // Suavizado: media centrada; en cada ataque la transición termina ANTES del ataque (no lo cruza)
    const int k = std::max (1, (int) std::lround (P.smoothingMs / 5.0f));
    std::vector<float> sm ((size_t) F);
    {
        int ra = 0;
        for (int f = 0; f <= F; ++f)
        {
            if (f == F || (f > 0 && A.regionStart[(size_t) f]))
            {
                const int rb = f, len = rb - ra;
                std::vector<double> cs ((size_t) len + 1, 0.0);
                for (int i = 0; i < len; ++i) cs[(size_t) i + 1] = cs[(size_t) i] + tgt[(size_t) (ra + i)];
                for (int i = 0; i < len; ++i)
                {
                    const int a = std::max (0, i - k), b = std::min (len, i + k + 1);
                    sm[(size_t) (ra + i)] = (float) ((cs[(size_t) b] - cs[(size_t) a]) / (b - a));
                }
                ra = f;
            }
        }
    }
    if (P.consonantProtect)
        for (int o : A.onsets)
        {
            const int a = std::max (0, o - k), t = std::min (F - 1, o + 2);
            if (o >= F) continue;
            bool crosses = false; for (int f = a + 1; f <= o; ++f) if (A.regionStart[(size_t) f]) crosses = true;
            if (crosses) continue;
            const float v0 = sm[(size_t) a], v1 = tgt[(size_t) t];
            for (int f = a; f <= o; ++f) sm[(size_t) f] = v0 + (v1 - v0) * (float) (f - a) / (float) std::max (1, o - a);
            // dentro del ataque la ganancia queda quieta
            for (int f = o; f <= std::min (F - 1, o + 3); ++f) sm[(size_t) f] = v1;
        }

    // Objetivo de sonoridad (opcional): desplazamiento estático
    if (P.targetOn)
    {
        double s = 0; int n = 0;
        for (int f = 0; f < F; ++f) if (A.active[(size_t) f]) { s += std::pow (10.0, (A.L[(size_t) f] + sm[(size_t) f]) / 10.0); ++n; }
        if (n > 0) R.staticOffset = std::clamp (P.targetLufs - powToDb (s / n), -24.0f, 24.0f);
        for (auto& v : sm) v += R.staticOffset;
    }

    // Protección de picos dentro de la propia curva: donde el pico pasaría del techo, se baja lo justo
    if (P.peakProtect && (int) framePeakDb.size() >= F)
    {
        std::vector<float> over ((size_t) F, 0.0f);
        auto extra = [&] (int f) { return (int) extraGainDb.size() > f ? extraGainDb[(size_t) f] : 0.0f; };
        bool any = false;
        for (int f = 0; f < F; ++f) { over[(size_t) f] = std::max (0.0f, framePeakDb[(size_t) f] + extra (f) + sm[(size_t) f] - P.ceilingDb); any |= over[(size_t) f] > 0; }
        if (any)
        {
            std::vector<float> dil ((size_t) F, 0.0f);
            for (int f = 0; f < F; ++f) { float m = 0; for (int j = std::max (0, f - k); j <= std::min (F - 1, f + k); ++j) m = std::max (m, over[(size_t) j]); dil[(size_t) f] = m; }
            std::vector<double> cs ((size_t) F + 1, 0.0);
            for (int f = 0; f < F; ++f) cs[(size_t) f + 1] = cs[(size_t) f] + dil[(size_t) f];
            for (int f = 0; f < F; ++f)
            {
                const int a = std::max (0, f - k), b = std::min (F, f + k + 1);
                sm[(size_t) f] -= (float) ((cs[(size_t) b] - cs[(size_t) a]) / (b - a));
                sm[(size_t) f] = std::min (sm[(size_t) f], P.ceilingDb - framePeakDb[(size_t) f] - extra (f));   // garantía
            }
        }
    }
    R.gainDb = sm;

    // Estadísticas
    double acc = 0; int na = 0;
    for (int f = 0; f < F; ++f) if (A.active[(size_t) f]) { acc += std::abs (sm[(size_t) f] - R.staticOffset); ++na; }
    R.meanAbsCorr = na ? (float) (acc / na) : 0.0f;
    std::vector<float> d0, d1;
    for (int i = 0; i < NW; ++i)
    {
        if (! use[(size_t) i]) continue;
        const auto& w = A.words[(size_t) i];
        double g = 0; for (int f = w.a; f < w.b; ++f) g += sm[(size_t) f];
        g /= std::max (1, w.len());
        d0.push_back (lv[(size_t) i] - trend[(size_t) i]);
        d1.push_back (lv[(size_t) i] + (float) g - R.staticOffset - T);
    }
    auto sd = [] (const std::vector<float>& v) { if (v.size() < 2) return 0.0f; double m = 0; for (float x : v) m += x; m /= v.size(); double s = 0; for (float x : v) s += (x - m) * (x - m); return (float) std::sqrt (s / v.size()); };
    std::vector<float> d0b;
    for (int i = 0; i < NW; ++i) if (use[(size_t) i]) d0b.push_back (lv[(size_t) i] - T);
    R.spreadBefore = sd (d0b); R.spreadAfter = sd (d1);
}
} // namespace levelivo

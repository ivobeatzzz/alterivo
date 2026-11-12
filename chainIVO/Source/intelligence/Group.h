// Coordinación entre instancias de chainIVO en el mismo proyecto (mismo proceso del DAW).
// Cada instancia publica (desde el hilo de la interfaz, nunca desde el audio) su tipo de voz y su espectro medio;
// las demás lo usan para no acumular frecuencias ni espacios iguales. Si el DAW aísla cada plugin en su propio
// proceso, cada instancia simplemente se ve sola (funciona igual, sin coordinación).
#pragma once
#include "../analysis/Analyzer.h"
#include <map>

namespace chainivo
{
struct GroupMember
{
    juce::String name;
    int type = -1;
    bool hasSpectrum = false;
    std::array<float, kBands> spectrum {};
    double lastSeenMs = 0.0;
};

class GroupRegistry
{
public:
    int join() { const juce::ScopedLock sl (lock); const int id = ++nextId; members[id] = {}; members[id].lastSeenMs = juce::Time::getMillisecondCounterHiRes(); return id; }
    void leave (int id) { const juce::ScopedLock sl (lock); members.erase (id); }
    void update (int id, const GroupMember& m) { const juce::ScopedLock sl (lock); members[id] = m; members[id].lastSeenMs = juce::Time::getMillisecondCounterHiRes(); }
    // Las demás instancias activas (vistas en los últimos 10 s) y la posición de ésta en el grupo
    std::vector<GroupMember> others (int id, int& myIndex) const
    {
        const juce::ScopedLock sl (lock);
        std::vector<GroupMember> v; myIndex = 0; const double now = juce::Time::getMillisecondCounterHiRes();
        for (auto& [k, m] : members)
        {
            if (now - m.lastSeenMs > 10000.0) continue;
            if (k == id) continue;
            if (k < id) ++myIndex;
            v.push_back (m);
        }
        return v;
    }
private:
    juce::CriticalSection lock;
    std::map<int, GroupMember> members;
    int nextId = 0;
};
} // namespace chainivo

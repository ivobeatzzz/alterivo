// reverseIVO — MOTOR EN SEGUNDO PLANO. Recibe peticiones de generación y las procesa en su propio hilo.
// Si llega una petición nueva mientras genera, la anterior se cancela: siempre gana el último ajuste.
// El hilo de audio nunca espera aquí.
#pragma once
#include <juce_core/juce_core.h>
#include "../render/Renderer.h"

namespace reverseivo
{
class Engine : private juce::Thread
{
public:
    Engine() : juce::Thread ("reverseIVO engine") { startThread (juce::Thread::Priority::low); }
    ~Engine() override { cancel = true; signalThreadShouldExit(); notify(); stopThread (4000); }

    struct Job
    {
        SourcePtr source;
        int regionStart = 0, regionEnd = 0;
        RenderSettings settings;
        Tempo tempo;
        int generation = 0;
    };

    void request (Job j)
    {
        {
            const juce::ScopedLock l (lock);
            pending = std::make_unique<Job> (std::move (j));
        }
        cancel = true;   // lo que se esté generando ya no vale
        notify();
    }

    // hilo de mensajes: recoge el último resultado terminado (si hay)
    ResultPtr take()
    {
        const juce::ScopedLock l (lock);
        auto r = finished; finished.reset();
        return r;
    }
    bool busy() const { return working.load() || hasPending(); }
    double lastMs() const { return renderMs.load(); }
    juce::String lastError() const { const juce::ScopedLock l (lock); return error; }

private:
    bool hasPending() const { const juce::ScopedLock l (lock); return pending != nullptr; }
    void run() override;

    juce::CriticalSection lock;
    std::unique_ptr<Job> pending;
    ResultPtr finished;
    juce::String error;
    std::atomic<bool> cancel { false }, working { false };
    std::atomic<double> renderMs { 0.0 };
};
} // namespace reverseivo

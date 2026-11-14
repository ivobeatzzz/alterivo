#include "Engine.h"

namespace reverseivo
{
void Engine::run()
{
    while (! threadShouldExit())
    {
        std::unique_ptr<Job> job;
        {
            const juce::ScopedLock l (lock);
            job = std::move (pending);
        }
        if (job == nullptr) { wait (-1); continue; }
        cancel = false;
        working = true;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        std::shared_ptr<Result> res;
        try
        {
            if (job->source != nullptr)
                res = render (*job->source, job->regionStart, job->regionEnd, job->settings, job->tempo, &cancel);
        }
        catch (const std::exception& e)
        {
            const juce::ScopedLock l (lock); error = e.what();
        }
        working = false;
        if (res != nullptr && ! cancel.load())
        {
            res->generation = job->generation;   // se marca con su número de generación
            renderMs = juce::Time::getMillisecondCounterHiRes() - t0;
            const juce::ScopedLock l (lock);
            finished = std::move (res);
            error.clear();
        }
    }
}
} // namespace reverseivo

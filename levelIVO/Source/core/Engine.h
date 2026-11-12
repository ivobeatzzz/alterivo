// levelIVO — MOTOR en segundo plano. Encadena detección → limpieza → decisión de nivel y publica un Result
// inmutable. Cada etapa sólo se repite cuando cambia lo que la afecta: mover un control de nivel no vuelve
// a analizar ni a limpiar; cambiar la limpieza no vuelve a analizar.
#pragma once
#include "../render/Result.h"
#include <juce_core/juce_core.h>
#include <mutex>

namespace levelivo
{
class Engine : private juce::Thread
{
public:
    Engine();
    ~Engine() override;

    enum class Stage { idle = 0, analyzing, cleaning, leveling };

    void setTake (TakePtr t);
    void setCleanParams (const CleanParams& p);
    void setLevelParams (const LevelParams& p, const std::vector<Edit>& edits);
    void clear();

    ResultPtr getResult() const { std::lock_guard<std::mutex> l (resMutex); return result; }
    Stage stage() const { return (Stage) stageNow.load(); }
    float progress() const { return prog.load(); }
    bool busy() const { return stage() != Stage::idle || pending.load() != 0; }

    // Se llama (desde el hilo del motor) cada vez que hay un resultado nuevo
    std::function<void()> onResult;

    // Uso sin hilo (pruebas y exportación): hace todo en el hilo que llama
    static ResultPtr computeNow (TakePtr take, const CleanParams&, const LevelParams&, const std::vector<Edit>&);
    static ResultPtr relevel (const ResultPtr& base, const LevelParams&, const std::vector<Edit>&);

private:
    void run() override;
    static void finish (Result& r, const LevelParams& lp, const std::vector<Edit>& ed);

    mutable std::mutex resMutex, inMutex;
    ResultPtr result;
    TakePtr take;
    CleanParams cp; LevelParams lp; std::vector<Edit> edits;
    std::atomic<int> pending { 0 };   // bits: 1 análisis, 2 limpieza, 4 nivel
    std::atomic<bool> cancel { false };
    std::atomic<int> stageNow { 0 };
    std::atomic<float> prog { 0 };
    juce::WaitableEvent wake;
    // lo que ya está hecho
    AnalysisPtr doneAnalysis;
    std::shared_ptr<const CleanResult> doneClean;
    std::vector<float> doneCleanLin, doneCleanPeaks;
    int version = 0;
};
} // namespace levelivo

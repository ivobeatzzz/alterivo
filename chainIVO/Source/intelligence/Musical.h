// MUSICAL INTELLIGENCE — tipos de voz, estilos (géneros) y recetas de cadena.
// Una receta describe la INTENCIÓN (qué motores, qué carácter, cuánto); el ChainBuilder la adapta a lo que el
// análisis ha medido en la grabación (graves, sibilancia, dureza, nivel, choque con la voz principal...).
#pragma once
#include <juce_core/juce_core.h>
#include "../analysis/Analyzer.h"
#include "../engines/CompEngine.h"
#include "../engines/HarmonicsEngine.h"
#include "../engines/SpatialEngine.h"
#include "../engines/CreativeEngine.h"

namespace chainivo
{
enum class VoiceType { adlib = 0, coro, doble, segunda, armonia, respuesta, introOutro, chop, susurro, grito, hablada, textura, ambiente, efecto, count };
const juce::StringArray& voiceTypeNames();          // en español, para la interfaz
juce::String voiceTypeName (VoiceType t);

struct GenreProfile
{
    const char* name;
    const char* family;
    float bright;      // dB de aire/presencia extra (-3..+4)
    float grit;        // saturación 0..1
    HarmType harm;
    RevType rev;
    float revSize;     // multiplica decay
    float revAmt;      // multiplica la mezcla
    float delayAmt;    // multiplica el delay
    float width;       // anchura extra 0..1
    float press;       // compresión extra en dB de GR
};
int genreCount();
const GenreProfile& genre (int index);
int genreIndex (const juce::String& name);

struct Recipe
{
    juce::String name, intention, tags;
    VoiceType type = VoiceType::adlib;
    int genre = -1;                // -1: el que elija el usuario
    // EQ (intenciones; el builder las adapta)
    float hpfMul = 1.0f, presence = 1.5f, air = 1.5f, body = 0.0f;
    // Dinámica
    CompType comp = CompType::vca; float grTarget = 5.0f, ratio = 4.0f, attack = 5.0f, release = 100.0f, compMix = 1.0f;
    // Armónicos
    bool harmOn = false; HarmType harm = HarmType::tube; float drive = 0.25f, harmMix = 1.0f, harmTone = 0.0f;
    // Control vocal
    float deessScale = 1.0f, harsh = 0.3f, breath = 0.4f;
    // Espacio
    RevType rev = RevType::plate; float revMix = 0.15f, decay = 1.5f, predelay = 20.0f, revTone = 0.6f;
    DlyType dly = DlyType::digital; int dlyNote = 5; float dlyFb = 0.3f, dlyMix = 0.0f, duck = 0.5f, width = 1.0f, doubler = 0.0f;
    // Creativo
    bool creaOn = false; CreaType crea = CreaType::stutter; int creaNote = 4; float creaAmount = 0.5f;
};

// Receta base para un tipo de voz (cuando el usuario pide "propuesta automática" sin preset)
Recipe defaultRecipe (VoiceType t);

struct Classification
{
    VoiceType type = VoiceType::adlib;
    float confidence = 0.0f;      // 0..1
    juce::String reason;          // por qué (en una frase)
    bool needsLead = false;       // hay tipos que no se pueden distinguir sin la voz principal
};
Classification classify (const AnalysisResult&);

struct BuildResult
{
    std::vector<std::pair<juce::String, float>> values;   // id de parámetro -> valor en sus unidades
    std::array<juce::String, 6> why;                      // explicación por motor
    int enginesOn = 0;
};
BuildResult buildChain (const Recipe&, const AnalysisResult&, int genreIdx);
} // namespace chainivo

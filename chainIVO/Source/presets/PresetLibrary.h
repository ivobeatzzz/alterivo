// PRESET ENGINE — presets IVO (recetas que se adaptan a cada grabación) y presets del usuario
// (copia exacta de los parámetros). Los presets IVO nunca se modifican: el usuario guarda variantes.
#pragma once
#include "../intelligence/Musical.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace chainivo
{
struct PresetInfo
{
    juce::String name, category, genreName, intention, tags;
    bool factory = true;
    int recipeIndex = -1;          // presets IVO
    juce::File file;               // presets del usuario
};

class PresetLibrary
{
public:
    PresetLibrary();
    const std::vector<Recipe>& recipes() const { return factory; }
    std::vector<PresetInfo> all() const;                         // IVO + usuario
    std::vector<PresetInfo> search (const juce::String& text, const juce::String& category, bool favouritesOnly) const;

    // Presets del usuario (archivos .chainivo en la carpeta de documentos del usuario)
    static juce::File userFolder();
    bool saveUser (const juce::String& name, const juce::ValueTree& state, const juce::String& category);
    static juce::ValueTree loadUser (const juce::File&);
    bool importFile (const juce::File& f);
    bool exportFile (const juce::String& name, const juce::File& dest) const;

    // Favoritos (por nombre)
    bool isFavourite (const juce::String& name) const;
    void toggleFavourite (const juce::String& name);

    static juce::StringArray categories();
    static juce::String categoryFor (VoiceType);

private:
    std::vector<Recipe> factory;
    juce::StringArray favourites;
    void loadFavourites();
    void saveFavourites() const;
};
} // namespace chainivo

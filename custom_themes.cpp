#include "custom_themes.h"
#include "app_config.h"

namespace
{
    std::vector<EditorTheme> g_allThemes;
    bool g_loaded = false;

    void Load()
    {
        g_allThemes = AllThemes(); // themes.h's compiled-in list
        std::vector<EditorTheme> custom = AppConfig::GetCustomThemes();
        g_allThemes.insert(g_allThemes.end(), custom.begin(), custom.end());
        g_loaded = true;
    }

    void EnsureLoaded()
    {
        if (!g_loaded) Load();
    }

    // Persists just the custom (non-built-in) portion of g_allThemes.
    void Persist()
    {
        size_t builtinCount = AllThemes().size();
        std::vector<EditorTheme> custom(g_allThemes.begin() + builtinCount, g_allThemes.end());
        AppConfig::SaveCustomThemes(custom);
    }
}

namespace CustomThemes
{

const std::vector<EditorTheme> &All()
{
    EnsureLoaded();
    return g_allThemes;
}

bool IsCustom(size_t index)
{
    return index >= AllThemes().size() && index < All().size();
}

size_t Add(const EditorTheme &theme)
{
    EnsureLoaded();
    g_allThemes.push_back(theme);
    Persist();
    return g_allThemes.size() - 1;
}

bool Update(size_t index, const EditorTheme &theme)
{
    EnsureLoaded();
    if (!IsCustom(index)) return false;
    g_allThemes[index] = theme;
    Persist();
    return true;
}

bool Remove(size_t index)
{
    EnsureLoaded();
    if (!IsCustom(index)) return false;
    g_allThemes.erase(g_allThemes.begin() + (long)index);
    Persist();
    return true;
}

void Reload()
{
    Load();
}

} // namespace CustomThemes

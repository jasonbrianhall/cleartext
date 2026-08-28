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
}

namespace CustomThemes
{

const std::vector<EditorTheme> &All()
{
    if (!g_loaded) Load();
    return g_allThemes;
}

bool IsCustom(size_t index)
{
    return index >= AllThemes().size() && index < All().size();
}

void Reload()
{
    Load();
}

} // namespace CustomThemes

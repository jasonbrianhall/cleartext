#include "app_config.h"
#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/gdicmn.h>

namespace
{
    // A single file-backed config rather than wx's default wxConfig
    // backend, so behavior -- and the on-disk format -- matches on both
    // platforms instead of Windows silently using the registry.
    wxString ConfigFilePath()
    {
        wxString dir = wxStandardPaths::Get().GetUserConfigDir() +
            wxFileName::GetPathSeparator() + "ClearText";
        if (!wxDirExists(dir))
            wxFileName::Mkdir(dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
        return dir + wxFileName::GetPathSeparator() + "cleartext.conf";
    }
}

namespace AppConfig
{

void Init()
{
    wxConfigBase::Set(new wxFileConfig("ClearText", wxEmptyString,
        ConfigFilePath(), wxEmptyString, wxCONFIG_USE_LOCAL_FILE));
}

void Shutdown()
{
    delete wxConfigBase::Set(nullptr); // flush + free the config Init() installed
}

int GetThemeIndex(int defaultValue)
{
    wxConfigBase *cfg = wxConfigBase::Get(false);
    if (!cfg) return defaultValue;
    long value = defaultValue;
    cfg->Read("Theme", &value, (long)defaultValue);
    return (int)value;
}

int GetFontSize(int defaultValue)
{
    wxConfigBase *cfg = wxConfigBase::Get(false);
    if (!cfg) return defaultValue;
    long value = defaultValue;
    cfg->Read("FontSize", &value, (long)defaultValue);
    return (int)value;
}

bool GetShowWhitespace(bool defaultValue)
{
    wxConfigBase *cfg = wxConfigBase::Get(false);
    if (!cfg) return defaultValue;
    bool value = defaultValue;
    cfg->Read("ShowWhitespace", &value, defaultValue);
    return value;
}

bool GetTrimTrailingWhitespace(bool defaultValue)
{
    wxConfigBase *cfg = wxConfigBase::Get(false);
    if (!cfg) return defaultValue;
    bool value = defaultValue;
    cfg->Read("TrimTrailingWhitespace", &value, defaultValue);
    return value;
}

wxArrayString GetLastSessionFiles()
{
    wxArrayString files;
    wxConfigBase *cfg = wxConfigBase::Get(false);
    if (!cfg) return files;

    long count = 0;
    cfg->Read("LastSession/Count", &count, 0L);
    for (long i = 0; i < count; i++)
    {
        wxString path;
        if (cfg->Read(wxString::Format("LastSession/File%ld", i), &path) && !path.IsEmpty())
            files.Add(path);
    }
    return files;
}

wxArrayString GetRecentFiles(size_t maxCount)
{
    wxArrayString files;
    wxConfigBase *cfg = wxConfigBase::Get(false);
    if (!cfg) return files;

    long count = 0;
    cfg->Read("RecentFiles/Count", &count, 0L);
    for (long i = 0; i < count && files.size() < maxCount; i++)
    {
        wxString path;
        if (cfg->Read(wxString::Format("RecentFiles/File%ld", i), &path) && !path.IsEmpty())
            files.Add(path);
    }
    return files;
}

void SaveSession(int themeIndex, int fontSize, bool showWhitespace,
                  bool trimTrailingWhitespace, const wxArrayString &lastSessionFiles)
{
    wxConfigBase *cfg = wxConfigBase::Get(false);
    if (!cfg) return;

    cfg->Write("Theme", (long)themeIndex);
    cfg->Write("FontSize", (long)fontSize);
    cfg->Write("ShowWhitespace", showWhitespace);
    cfg->Write("TrimTrailingWhitespace", trimTrailingWhitespace);

    cfg->DeleteGroup("LastSession");
    cfg->Write("LastSession/Count", (long)lastSessionFiles.size());
    for (size_t i = 0; i < lastSessionFiles.size(); i++)
        cfg->Write(wxString::Format("LastSession/File%zu", i), lastSessionFiles[i]);

    cfg->Flush();
}

void SaveRecentFiles(const wxArrayString &recentFiles)
{
    wxConfigBase *cfg = wxConfigBase::Get(false);
    if (!cfg) return;

    cfg->DeleteGroup("RecentFiles");
    cfg->Write("RecentFiles/Count", (long)recentFiles.size());
    for (size_t i = 0; i < recentFiles.size(); i++)
        cfg->Write(wxString::Format("RecentFiles/File%zu", i), recentFiles[i]);
    cfg->Flush();
}

WindowGeometry GetWindowGeometry()
{
    WindowGeometry geom;
    wxConfigBase *cfg = wxConfigBase::Get(false);
    if (!cfg) return geom;

    long x = -1, y = -1, w = geom.width, h = geom.height;
    bool maximized = false;
    cfg->Read("Window/X", &x, -1L);
    cfg->Read("Window/Y", &y, -1L);
    cfg->Read("Window/Width", &w, (long)geom.width);
    cfg->Read("Window/Height", &h, (long)geom.height);
    cfg->Read("Window/Maximized", &maximized, false);

    geom.x = (int)x;
    geom.y = (int)y;
    if (w > 0) geom.width = (int)w;
    if (h > 0) geom.height = (int)h;
    geom.maximized = maximized;
    return geom;
}

void SaveWindowGeometry(const WindowGeometry &geometry)
{
    wxConfigBase *cfg = wxConfigBase::Get(false);
    if (!cfg) return;

    cfg->Write("Window/X", (long)geometry.x);
    cfg->Write("Window/Y", (long)geometry.y);
    cfg->Write("Window/Width", (long)geometry.width);
    cfg->Write("Window/Height", (long)geometry.height);
    cfg->Write("Window/Maximized", geometry.maximized);
    cfg->Flush();
}

namespace
{
    wxString ColourToHex(const wxColour &c)
    {
        return wxString::Format("#%02X%02X%02X", c.Red(), c.Green(), c.Blue());
    }

    // Falls back to `fallback` for anything unset/unparseable, so a
    // hand-edited [CustomThemes] entry that's missing a key or two still
    // produces a usable (if partially default-colored) theme instead of
    // failing outright.
    wxColour HexToColour(const wxString &hex, const wxColour &fallback)
    {
        wxColour c;
        if (!hex.IsEmpty() && c.Set(hex)) return c;
        return fallback;
    }
}

std::vector<EditorTheme> GetCustomThemes()
{
    std::vector<EditorTheme> themes;
    wxConfigBase *cfg = wxConfigBase::Get(false);
    if (!cfg) return themes;

    long count = 0;
    cfg->Read("CustomThemes/Count", &count, 0L);
    for (long i = 0; i < count; i++)
    {
        wxString prefix = wxString::Format("CustomThemes/%ld/", i);

        wxString name;
        if (!cfg->Read(prefix + "Name", &name) || name.IsEmpty())
            name = wxString::Format("Custom %ld", i + 1);

        EditorTheme th;
        th.name           = name;
        th.background     = HexToColour(cfg->Read(prefix + "Background", ""), *wxBLACK);
        th.foreground     = HexToColour(cfg->Read(prefix + "Foreground", ""), *wxWHITE);
        th.caret          = HexToColour(cfg->Read(prefix + "Caret", ""), *wxWHITE);
        th.selectionBg    = HexToColour(cfg->Read(prefix + "SelectionBg", ""), wxColour(60, 60, 60));
        th.marginBg       = HexToColour(cfg->Read(prefix + "MarginBg", ""), wxColour(40, 40, 40));
        th.marginFg       = HexToColour(cfg->Read(prefix + "MarginFg", ""), *wxWHITE);
        th.comment        = HexToColour(cfg->Read(prefix + "Comment", ""), wxColour(100, 150, 100));
        th.number         = HexToColour(cfg->Read(prefix + "Number", ""), wxColour(200, 150, 100));
        th.string         = HexToColour(cfg->Read(prefix + "String", ""), wxColour(150, 200, 150));
        th.preprocessor   = HexToColour(cfg->Read(prefix + "Preprocessor", ""), wxColour(150, 150, 200));
        th.keyword        = HexToColour(cfg->Read(prefix + "Keyword", ""), wxColour(100, 150, 220));
        th.keyword2       = HexToColour(cfg->Read(prefix + "Keyword2", ""), wxColour(200, 100, 200));
        th.operatorColor  = HexToColour(cfg->Read(prefix + "OperatorColor", ""), *wxWHITE);
        th.tag            = HexToColour(cfg->Read(prefix + "Tag", ""), wxColour(220, 150, 100));
        th.attribute      = HexToColour(cfg->Read(prefix + "Attribute", ""), wxColour(150, 200, 220));
        th.markupCode     = HexToColour(cfg->Read(prefix + "MarkupCode", ""), wxColour(200, 200, 150));
        themes.push_back(th);
    }
    return themes;
}

void SaveCustomThemes(const std::vector<EditorTheme> &themes)
{
    wxConfigBase *cfg = wxConfigBase::Get(false);
    if (!cfg) return;

    cfg->DeleteGroup("CustomThemes");
    cfg->Write("CustomThemes/Count", (long)themes.size());
    for (size_t i = 0; i < themes.size(); i++)
    {
        wxString prefix = wxString::Format("CustomThemes/%zu/", i);
        const EditorTheme &th = themes[i];
        cfg->Write(prefix + "Name", th.name);
        cfg->Write(prefix + "Background", ColourToHex(th.background));
        cfg->Write(prefix + "Foreground", ColourToHex(th.foreground));
        cfg->Write(prefix + "Caret", ColourToHex(th.caret));
        cfg->Write(prefix + "SelectionBg", ColourToHex(th.selectionBg));
        cfg->Write(prefix + "MarginBg", ColourToHex(th.marginBg));
        cfg->Write(prefix + "MarginFg", ColourToHex(th.marginFg));
        cfg->Write(prefix + "Comment", ColourToHex(th.comment));
        cfg->Write(prefix + "Number", ColourToHex(th.number));
        cfg->Write(prefix + "String", ColourToHex(th.string));
        cfg->Write(prefix + "Preprocessor", ColourToHex(th.preprocessor));
        cfg->Write(prefix + "Keyword", ColourToHex(th.keyword));
        cfg->Write(prefix + "Keyword2", ColourToHex(th.keyword2));
        cfg->Write(prefix + "OperatorColor", ColourToHex(th.operatorColor));
        cfg->Write(prefix + "Tag", ColourToHex(th.tag));
        cfg->Write(prefix + "Attribute", ColourToHex(th.attribute));
        cfg->Write(prefix + "MarkupCode", ColourToHex(th.markupCode));
    }
    cfg->Flush();
}

} // namespace AppConfig

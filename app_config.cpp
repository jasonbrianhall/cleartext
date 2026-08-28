#include "app_config.h"
#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

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

} // namespace AppConfig

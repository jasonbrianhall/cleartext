#pragma once

#include <wx/string.h>
#include <wx/arrstr.h>

// Centralizes every read/write of ClearText's on-disk settings file
// (~/.config/ClearText/cleartext.conf on Linux, %APPDATA%\ClearText\cleartext.conf
// on Windows) behind a small typed API, so key names and defaults live in
// exactly one place instead of being scattered across the app.
namespace AppConfig
{
    // Installs the file-backed wxConfigBase as the process-wide default.
    // Call once, before reading any setting or creating the main frame.
    void Init();

    // Flushes and releases the config backend installed by Init(). Call
    // once, at shutdown.
    void Shutdown();

    int GetThemeIndex(int defaultValue);
    int GetFontSize(int defaultValue);
    bool GetShowWhitespace(bool defaultValue);
    bool GetTrimTrailingWhitespace(bool defaultValue);
    wxArrayString GetLastSessionFiles();
    wxArrayString GetRecentFiles(size_t maxCount);

    // Persists everything except the recent-files list (which has its own
    // save path below) in one shot. Called on shutdown.
    void SaveSession(int themeIndex, int fontSize, bool showWhitespace,
                      bool trimTrailingWhitespace, const wxArrayString &lastSessionFiles);

    // Recent files change during normal use, not just at shutdown, so
    // they're persisted on their own schedule rather than only from
    // SaveSession -- a crash shouldn't lose the list.
    void SaveRecentFiles(const wxArrayString &recentFiles);
}

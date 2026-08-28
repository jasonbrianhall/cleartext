#pragma once

#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/stc/stc.h>
#include <wx/fdrepdlg.h>
#include <wx/datetime.h>
#include <vector>
#include "highlighting.h"

// The main (and only) top-level window: a tabbed text editor built on
// wxStyledTextCtrl, with menu-driven file/edit/view commands, per-tab
// language/theme highlighting (see highlighting.h), find/replace, and
// session persistence (see app_config.h). A second launch of ClearText
// reaches this frame's public methods via the IPC layer (see ipc.h).
class ClearTextFrame : public wxFrame
{
public:
    ClearTextFrame();

    // Opens `path` in a new tab, reading its content from disk. Public so
    // it can be called for files passed on the command line, and by the
    // IPC layer when a second launch hands off its files.
    void OpenFilePath(const wxString &path);

    // Closes tab 0 if it's still the untouched, unsaved "Untitled" tab
    // created by the constructor -- used after opening files from argv (or
    // an IPC hand-off) so we don't leave a spare blank tab in front of them.
    void CloseInitialBlankTabIfUnused();

private:
    struct TabData
    {
        wxString filePath;
        bool modified = false;
        // Language::Auto means "detect from filePath's extension" (the
        // default); any other value is a user override from the Language
        // menu that ignores the extension until changed back to Auto.
        Language language = Language::Auto;
        // The file's on-disk modification time as of the last open/save/
        // reload. Invalid (default-constructed) means "not tracked" -- an
        // unsaved-only tab, or one that hasn't been touched yet.
        wxDateTime fileModTime;
    };

    static const size_t kMaxRecentFiles = 10;

    wxNotebook *m_notebook;
    wxMenu *m_languageMenu = nullptr;
    wxMenu *m_recentMenu = nullptr;
    std::vector<TabData> m_tabData;
    wxArrayString m_recentFiles;
    wxFindReplaceData m_findData{wxFR_DOWN};
    wxFindReplaceDialog *m_findReplaceDialog = nullptr;
    bool m_wrapAround = true;
    bool m_showWhitespace = false;
    bool m_trimTrailingWhitespace = true;

    // --- Tab / editor-control plumbing ---
    wxStyledTextCtrl *CurrentText();
    wxStyledTextCtrl *PageText(int index);
    void SetupEditor(wxStyledTextCtrl *stc);
    void UpdateMarginWidth(wxStyledTextCtrl *stc);
    void AddTab(const wxString &title, const wxString &content = "", const wxString &filePath = "");
    void UpdateTabLabel(int index);
    void UpdateTitle();
    Language EffectiveLanguage(int index);
    void UpdateLanguageMenuChecks();
    int IndexOf(wxStyledTextCtrl *stc);

    // --- Scintilla event handlers ---
    void OnSavePointLeft(wxStyledTextEvent &event);
    void OnSavePointReached(wxStyledTextEvent &event);
    void OnLineCountChange(wxStyledTextEvent &event);
    void OnEditorUpdateUI(wxStyledTextEvent &event);
    void UpdateBraceHighlight(wxStyledTextCtrl *stc);
    void UpdateStatusBarPosition(wxStyledTextCtrl *stc);
    void OnMarginClick(wxStyledTextEvent &event);
    void OnPageChanged(wxBookCtrlEvent &event);
    void OnActivate(wxActivateEvent &event);
    void CheckExternalModification(int index);

    // --- Theme / language ---
    void ReapplyHighlightingToAllTabs();
    void OnSetTheme(wxCommandEvent &event);
    void OnSetLanguage(wxCommandEvent &event);

    // --- Tab lifecycle: new / close / save / reload ---
    void OnNewTab(wxCommandEvent &event);
    void OnCloseTab(wxCommandEvent &event);
    bool CloseTab(int index, bool addNewIfEmpty = true);
    bool SaveTab(int index);
    void TrimTrailingWhitespace(wxStyledTextCtrl *stc);
    void OnSaveAll(wxCommandEvent &event);
    void OnReload(wxCommandEvent &event);
    bool ReloadTab(int index, bool force);
    void OnOpen(wxCommandEvent &event);
    void OnSave(wxCommandEvent &event);
    void OnSaveAs(wxCommandEvent &event);
    void OnExit(wxCommandEvent &event);
    void OnCloseWindow(wxCloseEvent &event);

    // --- Session / recent-files persistence (see app_config.h) ---
    void SaveSession(const wxArrayString &openFiles);
    void LoadFrameSettings();
    void SaveRecentFiles();
    void AddToRecentFiles(const wxString &path);
    void RebuildRecentFilesMenu();
    void OnOpenRecent(wxCommandEvent &event);
    void OnClearRecentFiles(wxCommandEvent &event);

    // --- Basic edit commands ---
    void OnUndo(wxCommandEvent &event);
    void OnRedo(wxCommandEvent &event);
    void OnCut(wxCommandEvent &event);
    void OnCopy(wxCommandEvent &event);
    void OnPaste(wxCommandEvent &event);
    void OnSelectAll(wxCommandEvent &event);

    void OnPrint(wxCommandEvent &event);
    void OnAbout(wxCommandEvent &event);

    // --- Find / replace / go to line ---
    bool FindInEditor(wxStyledTextCtrl *stc, const wxString &text, int flags, bool forward);
    void ShowFindDialog(bool replace);
    void OnFindMenu(wxCommandEvent &event);
    void OnReplaceMenu(wxCommandEvent &event);
    void OnToggleWrapAround(wxCommandEvent &event);
    void OnGoToLine(wxCommandEvent &event);
    wxString NotFoundMessage(const wxString &text);
    void OnFindNext(wxCommandEvent &event);
    void OnFindDialogEvent(wxFindDialogEvent &event);

    // --- View menu: wrap / whitespace / zoom / full screen ---
    void OnToggleWordWrap(wxCommandEvent &event);
    void OnToggleShowWhitespace(wxCommandEvent &event);
    void OnToggleTrimTrailingWhitespace(wxCommandEvent &event);
    void ChangeFontSize(int delta);
    void OnToggleFullScreen(wxCommandEvent &event);
    void OnZoomIn(wxCommandEvent &event);
    void OnZoomOut(wxCommandEvent &event);
    void OnZoomReset(wxCommandEvent &event);
    void OnMouseWheel(wxMouseEvent &event);
};

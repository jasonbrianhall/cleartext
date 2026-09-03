#pragma once

#include <wx/wx.h>
#include <wx/aui/auibook.h>
#include <wx/stc/stc.h>
#include <wx/fdrepdlg.h>
#include <wx/datetime.h>
#include <vector>
#include "highlighting.h"
#include "encoding.h"

class FindInFilesDialog;

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

    // --- Read-only tab access + navigation, used by FindInFilesDialog ---
    int GetTabCount();
    wxString GetTabLabel(int index);    // "Untitled" or the file's name
    wxString GetTabFilePath(int index); // may be empty (unsaved tab)
    wxString GetTabText(int index);
    void GoToTabAndLine(int index, int line);
    void OpenFilePathAndGoToLine(const wxString &path, int line);

private:
    struct TabData
    {
        wxString filePath;
        bool modified = false;
        // Language::Auto means "detect from filePath's extension" (the
        // default); any other value is a user override from the Language
        // menu that ignores the extension until changed back to Auto.
        Language language = Language::Auto;
        // TextEncoding::Encoding::Auto means "use whatever ReadFile()
        // detected at load time" (see detectedEncoding, below); any other
        // value is a user override from the View > Encoding menu, picked
        // when the auto-detected guess came out wrong.
        TextEncoding::Encoding encoding = TextEncoding::Encoding::Auto;
        // What this tab's content was actually decoded as -- either
        // ReadFile()'s BOM/UTF-8-validity guess, or the explicit choice
        // once overridden. Utf8 for a new/never-loaded ("Untitled") tab.
        TextEncoding::Encoding detectedEncoding = TextEncoding::Encoding::Utf8;
        // The file's on-disk modification time as of the last open/save/
        // reload. Invalid (default-constructed) means "not tracked" -- an
        // unsaved-only tab, or one that hasn't been touched yet.
        wxDateTime fileModTime;
    };

    static const size_t kMaxRecentFiles = 10;

    wxAuiNotebook *m_notebook;
    wxMenu *m_languageMenu = nullptr;
    wxMenu *m_recentMenu = nullptr;
    wxMenu *m_themeMenu = nullptr;
    wxMenu *m_encodingMenu = nullptr;
    std::vector<TabData> m_tabData;
    wxArrayString m_recentFiles;
    wxFindReplaceData m_findData{wxFR_DOWN};
    wxFindReplaceDialog *m_findReplaceDialog = nullptr;
    FindInFilesDialog *m_findInFilesDialog = nullptr;
    bool m_wrapAround = true;
    bool m_showWhitespace = false;
    bool m_trimTrailingWhitespace = true;
    // True only while CloseTab() is removing a page. DeletePage() can
    // synchronously select another page -- including the pinned "+" tab,
    // if the one being closed was the last real tab -- and fire
    // PAGE_CHANGED; this tells OnPageChanged not to react to that itself,
    // since CloseTab's own addNewIfEmpty logic is the single place that
    // decides whether a fresh "Untitled" tab is warranted.
    bool m_closingTab = false;

    // --- Tab / editor-control plumbing ---
    wxStyledTextCtrl *CurrentText();
    wxStyledTextCtrl *PageText(int index);
    void SetupEditor(wxStyledTextCtrl *stc);
    void UpdateMarginWidth(wxStyledTextCtrl *stc);
    // Forces Scintilla to (re)compute fold levels for the whole document
    // -- see the .cpp for why this is needed at all. Call after SetText
    // or ApplyHighlighting.
    void RefreshFolding(wxStyledTextCtrl *stc);
    void AddTab(const wxString &title, const wxString &content = "", const wxString &filePath = "",
                TextEncoding::Encoding detectedEncoding = TextEncoding::Encoding::Utf8);
    void UpdateTabLabel(int index);
    void UpdateTitle();
    Language EffectiveLanguage(int index);
    void UpdateLanguageMenuChecks();
    // The encoding actually in effect for this tab: its override if one's
    // been set via View > Encoding, otherwise whatever was auto-detected
    // at load time (see TabData::detectedEncoding).
    TextEncoding::Encoding EffectiveEncoding(int index);
    void UpdateEncodingMenuChecks();
    void GoToLineInTab(int index, int line);
    int IndexOf(wxStyledTextCtrl *stc);

    // --- Scintilla event handlers ---
    void OnSavePointLeft(wxStyledTextEvent &event);
    void OnSavePointReached(wxStyledTextEvent &event);
    void OnLineCountChange(wxStyledTextEvent &event);
    void OnEditorUpdateUI(wxStyledTextEvent &event);
    void UpdateBraceHighlight(wxStyledTextCtrl *stc);
    void UpdateStatusBarPosition(wxStyledTextCtrl *stc);
    void OnMarginClick(wxStyledTextEvent &event);
    void OnEditorContextMenu(wxContextMenuEvent &event);
    // Auto-closes brackets/quotes as they're typed: intercepted here
    // (before Scintilla inserts the character) so a selection can be
    // wrapped instead of replaced, and typing a closer that's already
    // there just steps over it instead of doubling it up.
    void OnEditorChar(wxKeyEvent &event);
    // Auto-closes an HTML/XML tag right after its opening ">" is typed
    // (skipped for closing tags, self-closing tags, and void elements
    // like <br>). Runs after insertion, unlike OnEditorChar, since it
    // needs to see the tag name that was just typed.
    void OnEditorCharAdded(wxStyledTextEvent &event);
    void OnPageChanged(wxAuiNotebookEvent &event);
    void OnPageClose(wxAuiNotebookEvent &event);
    void OnPageBeginDrag(wxAuiNotebookEvent &event);
    // True if the notebook page at `index` is the pinned "+" tab (an empty
    // panel) rather than a real editor tab (a wxStyledTextCtrl) -- see
    // AddTab, which always keeps it last.
    bool IsAddTabPage(int index);
    void OnActivate(wxActivateEvent &event);
    void CheckExternalModification(int index);

    // --- Theme / language ---
    void ReapplyHighlightingToAllTabs();
    void RebuildThemeMenu();
    void UpdateThemeActionStates();
    void OnSetTheme(wxCommandEvent &event);
    void OnNewTheme(wxCommandEvent &event);
    void OnEditTheme(wxCommandEvent &event);
    void OnDeleteTheme(wxCommandEvent &event);
    void OnSetLanguage(wxCommandEvent &event);
    void OnSetEncoding(wxCommandEvent &event);

    // --- Tab lifecycle: new / close / save / reload ---
    void OnNewTab(wxCommandEvent &event);
    void OnCloseTab(wxCommandEvent &event);
    bool CloseTab(int index, bool addNewIfEmpty = true);
    bool SaveTab(int index);
    void TrimTrailingWhitespace(wxStyledTextCtrl *stc);
    void OnSaveAll(wxCommandEvent &event);
    void OnReload(wxCommandEvent &event);
    bool ReloadTab(int index, bool force);
    // Shared by ReloadTab (reload preserving the tab's current encoding
    // choice) and OnSetEncoding (reload re-decoding as a newly-picked
    // encoding). Re-reads `index`'s file from disk as `encoding` (Auto
    // means re-detect), discarding any in-editor changes; with `force`
    // false and the tab modified, confirms with the user first.
    bool ReloadTabAs(int index, TextEncoding::Encoding encoding, bool force);
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
    void OnCopyAsHtml(wxCommandEvent &event);
    void OnPaste(wxCommandEvent &event);
    void OnSelectAll(wxCommandEvent &event);

    void OnPrint(wxCommandEvent &event);
    void OnAbout(wxCommandEvent &event);
    // Shows line/word/character counts for the current tab -- the whole
    // document, or just the selection when one is active.
    void OnStatistics(wxCommandEvent &event);

    // --- Find / replace / go to line / find in files ---
    bool FindInEditor(wxStyledTextCtrl *stc, const wxString &text, int flags, bool forward);
    void ShowFindDialog(bool replace);
    void OnFindMenu(wxCommandEvent &event);
    void OnReplaceMenu(wxCommandEvent &event);
    void OnToggleWrapAround(wxCommandEvent &event);
    void OnGoToLine(wxCommandEvent &event);
    void OnFindInFiles(wxCommandEvent &event);
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


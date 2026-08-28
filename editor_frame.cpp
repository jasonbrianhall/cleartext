#include "editor_frame.h"
#include "themes.h"
#include "highlighting.h"
#include "printing.h"
#include "app_config.h"
#include "custom_themes.h"
#include "encoding.h"
#include "find_in_files.h"
#include <wx/filename.h>
#include <wx/print.h>
#include <wx/aboutdlg.h>
#include <wx/numdlg.h>
#include <wx/dnd.h>

namespace
{
    // True for the ASCII characters Scintilla's brace-matching understands.
    // Used by OnEditorUpdateUI to decide whether the caret is next to a brace.
    bool IsBraceChar(int ch)
    {
        return ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '{' || ch == '}';
    }

    enum
    {
        ID_NewTab = wxID_HIGHEST + 1,
        ID_CloseTab,
        ID_FindNext,
        ID_WrapAround,
        ID_WordWrap,
        ID_ToggleFullScreen,
        ID_GoToLine,
        ID_FindInFiles,
        ID_SaveAll,
        ID_Reload,
        ID_ShowWhitespace,
        ID_TrimTrailingWhitespace,
        ID_ReloadCustomThemes,
        ID_ThemeBase = wxID_HIGHEST + 100,        // one radio id per entry in CustomThemes::All()...
        ID_LanguageBase = ID_ThemeBase + 128,     // ...then one per entry in AllLanguages()...
        ID_RecentFileBase = ID_LanguageBase + 64, // ...then one per recent-file slot...
        ID_ClearRecentFiles = ID_RecentFileBase + 32 // ...leaving room for kMaxRecentFiles entries.
    };

    // Lets the user drop files from their OS file manager anywhere on the
    // window -- the frame background, the tab bar, or an editor's text
    // area (each gets its own instance; see the constructor and AddTab).
    class FileDropTarget : public wxFileDropTarget
    {
    public:
        explicit FileDropTarget(ClearTextFrame *frame) : m_frame(frame) {}

        bool OnDropFiles(wxCoord, wxCoord, const wxArrayString &filenames) override
        {
            for (const wxString &f : filenames)
                m_frame->OpenFilePath(f);
            if (!filenames.IsEmpty())
                m_frame->CloseInitialBlankTabIfUnused();
            return true;
        }

    private:
        ClearTextFrame *m_frame;
    };
}

// ============================================================================
// Construction / menu setup
// ============================================================================

ClearTextFrame::ClearTextFrame()
    : wxFrame(nullptr, wxID_ANY, "ClearText", wxDefaultPosition, wxSize(800, 600))
{
    LoadFrameSettings(); // recent files + whitespace toggles -- reads the config ClearTextApp::OnInit() already installed before this ctor runs

    wxMenuBar *menuBar = new wxMenuBar();

    wxMenu *fileMenu = new wxMenu();
    fileMenu->Append(ID_NewTab, "New Tab\tCtrl+T");
    fileMenu->Append(wxID_OPEN, "Open...\tCtrl+O");
    m_recentMenu = new wxMenu();
    fileMenu->AppendSubMenu(m_recentMenu, "Open Recent");
    RebuildRecentFilesMenu();
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_SAVE, "Save\tCtrl+S");
    fileMenu->Append(wxID_SAVEAS, "Save As...\tCtrl+Shift+S");
    fileMenu->Append(ID_SaveAll, "Save All\tCtrl+Alt+S");
    fileMenu->Append(ID_Reload, "Reload from Disk\tCtrl+Shift+R");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_PRINT, "Print...\tCtrl+P");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_CloseTab, "Close Tab\tCtrl+W");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, "Exit\tAlt+F4");
    menuBar->Append(fileMenu, "&File");

    wxMenu *editMenu = new wxMenu();
    editMenu->Append(wxID_UNDO, "Undo\tCtrl+Z");
    editMenu->Append(wxID_REDO, "Redo\tCtrl+Y");
    editMenu->AppendSeparator();
    editMenu->Append(wxID_CUT, "Cut\tCtrl+X");
    editMenu->Append(wxID_COPY, "Copy\tCtrl+C");
    editMenu->Append(wxID_PASTE, "Paste\tCtrl+V");
    editMenu->AppendSeparator();
    editMenu->Append(wxID_SELECTALL, "Select All\tCtrl+A");
    editMenu->AppendSeparator();
    editMenu->Append(wxID_FIND, "Find...\tCtrl+F");
    editMenu->Append(ID_FindNext, "Find Next\tF3");
    editMenu->Append(wxID_REPLACE, "Replace...\tCtrl+H");
    wxMenuItem *wrapItem = editMenu->AppendCheckItem(ID_WrapAround, "Wrap Around");
    wrapItem->Check(true);
    editMenu->AppendSeparator();
    editMenu->Append(ID_GoToLine, "Go To Line...\tCtrl+G");
    editMenu->Append(ID_FindInFiles, "Find in Files...\tCtrl+Shift+F");
    menuBar->Append(editMenu, "&Edit");

    wxMenu *viewMenu = new wxMenu();
    wxMenuItem *wordWrapItem = viewMenu->AppendCheckItem(ID_WordWrap, "Word Wrap");
    wordWrapItem->Check(true);
    wxMenuItem *wsItem = viewMenu->AppendCheckItem(ID_ShowWhitespace, "Show Whitespace");
    wsItem->Check(m_showWhitespace);
    wxMenuItem *trimItem = viewMenu->AppendCheckItem(ID_TrimTrailingWhitespace, "Trim Trailing Whitespace on Save");
    trimItem->Check(m_trimTrailingWhitespace);
    viewMenu->AppendSeparator();
    viewMenu->Append(wxID_ZOOM_IN, "Increase Font Size\tCtrl+=");
    viewMenu->Append(wxID_ZOOM_OUT, "Decrease Font Size\tCtrl+-");
    viewMenu->Append(wxID_ZOOM_100, "Reset Font Size\tCtrl+0");
    viewMenu->AppendSeparator();
    viewMenu->Append(ID_ToggleFullScreen, "Full Screen\tF11");
    viewMenu->AppendSeparator();

    wxMenu *themeMenu = new wxMenu();
    m_themeMenu = themeMenu;
    RebuildThemeMenu();
    viewMenu->AppendSubMenu(themeMenu, "Theme");

    // Per-tab override of ApplyHighlighting's language, regardless of
    // the file's extension. "Auto-Detect" (the default) restores
    // extension-based detection; checkmarks are kept in sync with the
    // active tab in OnPageChanged/UpdateLanguageMenuChecks.
    m_languageMenu = new wxMenu();
    const std::vector<LanguageInfo> &languages = AllLanguages();
    for (size_t i = 0; i < languages.size(); i++)
        m_languageMenu->AppendRadioItem(ID_LanguageBase + (int)i, languages[i].label);
    m_languageMenu->Check(ID_LanguageBase, true); // Auto-Detect, index 0
    viewMenu->AppendSubMenu(m_languageMenu, "Language");

    menuBar->Append(viewMenu, "&View");

    wxMenu *helpMenu = new wxMenu();
    helpMenu->Append(wxID_ABOUT, "About ClearText...");
    menuBar->Append(helpMenu, "&Help");

    SetMenuBar(menuBar);
    CreateStatusBar();

#ifdef WIN32
    // Loads the icon embedded via cleartext.rc/windres. Safe no-op if
    // the exe wasn't built with the resource (e.g. a dev build without
    // icon.ico present) — SetIcon with an invalid wxIcon just does nothing.
    SetIcon(wxIcon("appicon"));
#endif

    // Restores the window's last position/size (see OnCloseWindow, which
    // saves it). Applied before Show() -- main.cpp calls that after
    // construction -- so there's no visible jump to the saved geometry.
    AppConfig::WindowGeometry geom = AppConfig::GetWindowGeometry();
    SetSize(geom.width, geom.height);
    if (geom.x != -1 && geom.y != -1)
        SetPosition(wxPoint(geom.x, geom.y));
    if (geom.maximized)
        Maximize(true);

    // Lets files be dropped from the OS file manager anywhere on the
    // window; each editor tab also gets its own instance (see AddTab).
    SetDropTarget(new FileDropTarget(this));

    m_notebook = new wxAuiNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxAUI_NB_TOP | wxAUI_NB_TAB_SPLIT | wxAUI_NB_TAB_MOVE | wxAUI_NB_SCROLL_BUTTONS |
        wxAUI_NB_CLOSE_ON_ALL_TABS | wxAUI_NB_MIDDLE_CLICK_CLOSE | wxAUI_NB_WINDOWLIST_BUTTON);
    m_notebook->SetDropTarget(new FileDropTarget(this));
    AddTab("Untitled");

    Bind(wxEVT_MENU, &ClearTextFrame::OnNewTab, this, ID_NewTab);
    Bind(wxEVT_MENU, &ClearTextFrame::OnCloseTab, this, ID_CloseTab);
    Bind(wxEVT_MENU, &ClearTextFrame::OnOpen, this, wxID_OPEN);
    Bind(wxEVT_MENU, &ClearTextFrame::OnSave, this, wxID_SAVE);
    Bind(wxEVT_MENU, &ClearTextFrame::OnSaveAs, this, wxID_SAVEAS);
    Bind(wxEVT_MENU, &ClearTextFrame::OnSaveAll, this, ID_SaveAll);
    Bind(wxEVT_MENU, &ClearTextFrame::OnReload, this, ID_Reload);
    Bind(wxEVT_MENU, &ClearTextFrame::OnExit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &ClearTextFrame::OnUndo, this, wxID_UNDO);
    Bind(wxEVT_MENU, &ClearTextFrame::OnRedo, this, wxID_REDO);
    Bind(wxEVT_MENU, &ClearTextFrame::OnCut, this, wxID_CUT);
    Bind(wxEVT_MENU, &ClearTextFrame::OnCopy, this, wxID_COPY);
    Bind(wxEVT_MENU, &ClearTextFrame::OnPaste, this, wxID_PASTE);
    Bind(wxEVT_MENU, &ClearTextFrame::OnSelectAll, this, wxID_SELECTALL);
    Bind(wxEVT_MENU, &ClearTextFrame::OnPrint, this, wxID_PRINT);
    Bind(wxEVT_MENU, &ClearTextFrame::OnAbout, this, wxID_ABOUT);
    Bind(wxEVT_MENU, &ClearTextFrame::OnFindMenu, this, wxID_FIND);
    Bind(wxEVT_MENU, &ClearTextFrame::OnReplaceMenu, this, wxID_REPLACE);
    Bind(wxEVT_MENU, &ClearTextFrame::OnFindNext, this, ID_FindNext);
    Bind(wxEVT_MENU, &ClearTextFrame::OnGoToLine, this, ID_GoToLine);
    Bind(wxEVT_MENU, &ClearTextFrame::OnFindInFiles, this, ID_FindInFiles);
    Bind(wxEVT_MENU, &ClearTextFrame::OnToggleWrapAround, this, ID_WrapAround);
    Bind(wxEVT_MENU, &ClearTextFrame::OnToggleWordWrap, this, ID_WordWrap);
    Bind(wxEVT_MENU, &ClearTextFrame::OnToggleShowWhitespace, this, ID_ShowWhitespace);
    Bind(wxEVT_MENU, &ClearTextFrame::OnToggleTrimTrailingWhitespace, this, ID_TrimTrailingWhitespace);
    Bind(wxEVT_MENU, &ClearTextFrame::OnZoomIn, this, wxID_ZOOM_IN);
    Bind(wxEVT_MENU, &ClearTextFrame::OnZoomOut, this, wxID_ZOOM_OUT);
    Bind(wxEVT_MENU, &ClearTextFrame::OnZoomReset, this, wxID_ZOOM_100);
    Bind(wxEVT_MENU, &ClearTextFrame::OnToggleFullScreen, this, ID_ToggleFullScreen);
    Bind(wxEVT_MENU, &ClearTextFrame::OnSetTheme, this, ID_ThemeBase, ID_LanguageBase - 1);
    Bind(wxEVT_MENU, &ClearTextFrame::OnReloadCustomThemes, this, ID_ReloadCustomThemes);
    Bind(wxEVT_MENU, &ClearTextFrame::OnSetLanguage, this, ID_LanguageBase,
         ID_LanguageBase + (int)AllLanguages().size() - 1);
    Bind(wxEVT_MENU, &ClearTextFrame::OnOpenRecent, this, ID_RecentFileBase,
         ID_RecentFileBase + (int)kMaxRecentFiles - 1);
    Bind(wxEVT_MENU, &ClearTextFrame::OnClearRecentFiles, this, ID_ClearRecentFiles);
    Bind(wxEVT_FIND, &ClearTextFrame::OnFindDialogEvent, this);
    Bind(wxEVT_FIND_NEXT, &ClearTextFrame::OnFindDialogEvent, this);
    Bind(wxEVT_FIND_REPLACE, &ClearTextFrame::OnFindDialogEvent, this);
    Bind(wxEVT_FIND_REPLACE_ALL, &ClearTextFrame::OnFindDialogEvent, this);
    Bind(wxEVT_FIND_CLOSE, &ClearTextFrame::OnFindDialogEvent, this);
    Bind(wxEVT_AUINOTEBOOK_PAGE_CHANGED, &ClearTextFrame::OnPageChanged, this);
    Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE, &ClearTextFrame::OnPageClose, this);
    Bind(wxEVT_CLOSE_WINDOW, &ClearTextFrame::OnCloseWindow, this);
    Bind(wxEVT_ACTIVATE, &ClearTextFrame::OnActivate, this);
}

// ============================================================================
// File open / initial-tab handling (public)
// ============================================================================

void ClearTextFrame::OpenFilePath(const wxString &path)
{
    if (!wxFileExists(path))
    {
        // No file there yet — open a blank tab pointed at this path so
        // Save writes it there directly, rather than erroring out.
        AddTab(wxFileName(path).GetFullName(), "", path);
        return;
    }

    wxString content;
    if (!TextEncoding::ReadFile(path, content))
    {
        wxMessageBox("Could not open file:\n" + path, "Error", wxOK | wxICON_ERROR, this);
        return;
    }

    AddTab(wxFileName(path).GetFullName(), content, path);
    AddToRecentFiles(path);
}

void ClearTextFrame::CloseInitialBlankTabIfUnused()
{
    if (m_notebook->GetPageCount() < 2) return;
    if (!m_tabData[0].filePath.IsEmpty()) return;
    if (m_tabData[0].modified) return;
    if (!PageText(0)->IsEmpty()) return;

    CloseTab(0, false);
}

// ============================================================================
// Read-only tab access + navigation (public, used by FindInFilesDialog)
// ============================================================================

int ClearTextFrame::GetTabCount()
{
    return (int)m_tabData.size();
}

wxString ClearTextFrame::GetTabLabel(int index)
{
    return m_tabData[index].filePath.IsEmpty()
        ? "Untitled" : wxFileName(m_tabData[index].filePath).GetFullName();
}

wxString ClearTextFrame::GetTabFilePath(int index)
{
    return m_tabData[index].filePath;
}

wxString ClearTextFrame::GetTabText(int index)
{
    return PageText(index)->GetText();
}

// Selects `index` and moves its caret to `line` (1-based). Used both for
// jumping to an already-open tab (Find in Files) and after opening a file
// found by name/extension elsewhere.
void ClearTextFrame::GoToLineInTab(int index, int line)
{
    m_notebook->SetSelection(index);
    wxStyledTextCtrl *stc = PageText(index);
    stc->GotoLine(line - 1);
    stc->EnsureCaretVisible();
    stc->SetFocus();
}

void ClearTextFrame::GoToTabAndLine(int index, int line)
{
    if (index < 0 || index >= (int)m_tabData.size()) return; // tab may have closed since the match was found
    GoToLineInTab(index, line);
}

void ClearTextFrame::OpenFilePathAndGoToLine(const wxString &path, int line)
{
    // If the file is already open in a tab, jump there instead of opening
    // a second tab on the same file.
    wxFileName target(path);
    target.MakeAbsolute();
    for (size_t i = 0; i < m_tabData.size(); i++)
    {
        if (m_tabData[i].filePath.IsEmpty()) continue;
        wxFileName existing(m_tabData[i].filePath);
        existing.MakeAbsolute();
        if (existing == target)
        {
            GoToLineInTab((int)i, line);
            return;
        }
    }

    OpenFilePath(path);
    GoToLineInTab((int)m_tabData.size() - 1, line);
}

// ============================================================================
// Tab / editor-control plumbing
// ============================================================================

wxStyledTextCtrl *ClearTextFrame::CurrentText()
{
    int sel = m_notebook->GetSelection();
    if (sel == wxNOT_FOUND) return nullptr;
    return (wxStyledTextCtrl*)m_notebook->GetPage(sel);
}

wxStyledTextCtrl *ClearTextFrame::PageText(int index)
{
    return (wxStyledTextCtrl*)m_notebook->GetPage(index);
}

void ClearTextFrame::SetupEditor(wxStyledTextCtrl *stc)
{
    // Line number margin
    stc->SetMarginType(0, wxSTC_MARGIN_NUMBER);
    stc->SetMarginWidth(1, 0); // hide the unused symbol margin

    // Fold margin: a clickable +/- gutter (see OnMarginClick) driven by
    // each lexer's own fold-level computation. Harmless no-op for
    // lexers/content that don't produce foldable structure.
    stc->SetMarginType(2, wxSTC_MARGIN_SYMBOL);
    stc->SetMarginMask(2, wxSTC_MASK_FOLDERS);
    stc->SetMarginWidth(2, 16);
    stc->SetMarginSensitive(2, true);
    stc->SetProperty("fold", "1");
    stc->SetProperty("fold.compact", "1");
    stc->SetFoldFlags(wxSTC_FOLDFLAG_LINEBEFORE_CONTRACTED | wxSTC_FOLDFLAG_LINEAFTER_CONTRACTED);

    stc->SetTabWidth(4);
    stc->SetUseTabs(false);
    stc->SetWrapMode(wxSTC_WRAP_WORD);
    stc->SetViewWhiteSpace(m_showWhitespace ? wxSTC_WS_VISIBLEALWAYS : wxSTC_WS_INVISIBLE);
}

// Grows the line-number margin as the document gains more digits
void ClearTextFrame::UpdateMarginWidth(wxStyledTextCtrl *stc)
{
    int lines = stc->GetLineCount();
    wxString widest = wxString::Format("_%d", lines);
    stc->SetMarginWidth(0, stc->TextWidth(wxSTC_STYLE_LINENUMBER, widest));
}

void ClearTextFrame::AddTab(const wxString &title, const wxString &content, const wxString &filePath)
{
    // Push tab metadata BEFORE AddPage, since AddPage synchronously fires
    // a page-changed event whose handler reads m_tabData by index.
    m_tabData.push_back(TabData());
    m_tabData.back().filePath = filePath;
    if (!filePath.IsEmpty() && wxFileExists(filePath))
        m_tabData.back().fileModTime = wxFileName(filePath).GetModificationTime();

    wxStyledTextCtrl *stc = new wxStyledTextCtrl(m_notebook, wxID_ANY);
    SetupEditor(stc);
    ApplyHighlighting(stc, EffectiveLanguage((int)m_tabData.size() - 1));
    if (!content.IsEmpty())
        stc->SetText(content);
    stc->EmptyUndoBuffer();
    stc->SetSavePoint(); // mark this freshly-loaded content as "unmodified"

    stc->Bind(wxEVT_STC_SAVEPOINTLEFT, &ClearTextFrame::OnSavePointLeft, this);
    stc->Bind(wxEVT_STC_SAVEPOINTREACHED, &ClearTextFrame::OnSavePointReached, this);
    stc->Bind(wxEVT_STC_CHANGE, &ClearTextFrame::OnLineCountChange, this);
    stc->Bind(wxEVT_MOUSEWHEEL, &ClearTextFrame::OnMouseWheel, this);
    stc->Bind(wxEVT_STC_UPDATEUI, &ClearTextFrame::OnEditorUpdateUI, this);
    stc->Bind(wxEVT_STC_MARGINCLICK, &ClearTextFrame::OnMarginClick, this);
    stc->SetDropTarget(new FileDropTarget(this));

    m_notebook->AddPage(stc, title, true);
    UpdateMarginWidth(stc);
    UpdateTitle();
}

void ClearTextFrame::UpdateTabLabel(int index)
{
    wxString label = m_tabData[index].filePath.IsEmpty()
        ? "Untitled" : wxFileName(m_tabData[index].filePath).GetFullName();
    if (m_tabData[index].modified) label += " *";
    m_notebook->SetPageText(index, label);
}

void ClearTextFrame::UpdateTitle()
{
    int sel = m_notebook->GetSelection();
    if (sel == wxNOT_FOUND || sel >= (int)m_tabData.size()) { SetTitle("ClearText"); return; }
    wxString name = m_tabData[sel].filePath.IsEmpty()
        ? "Untitled" : wxFileName(m_tabData[sel].filePath).GetFullName();
    SetTitle(name + " - ClearText");
}

// Resolves a tab's actual highlighting language: its explicit override
// if one was picked from the Language menu, otherwise whatever the
// file's extension detects to.
Language ClearTextFrame::EffectiveLanguage(int index)
{
    if (m_tabData[index].language != Language::Auto)
        return m_tabData[index].language;
    return DetectLanguageFromExtension(m_tabData[index].filePath);
}

// Keeps the Language menu's radio checkmark in sync with the active
// tab's override (or Auto-Detect, if it has none).
void ClearTextFrame::UpdateLanguageMenuChecks()
{
    int sel = m_notebook->GetSelection();
    Language lang = (sel == wxNOT_FOUND) ? Language::Auto : m_tabData[sel].language;

    const std::vector<LanguageInfo> &languages = AllLanguages();
    for (size_t i = 0; i < languages.size(); i++)
    {
        if (languages[i].id == lang)
        {
            m_languageMenu->Check(ID_LanguageBase + (int)i, true);
            break;
        }
    }
}

int ClearTextFrame::IndexOf(wxStyledTextCtrl *stc)
{
    for (size_t i = 0; i < m_notebook->GetPageCount(); i++)
        if (m_notebook->GetPage(i) == stc) return (int)i;
    return wxNOT_FOUND;
}

// ============================================================================
// Scintilla event handlers
// ============================================================================

// Scintilla's own "save point" tracks modification relative to the last
// SetSavePoint() call, rather than every text-change event — so loading
// a file's initial content, or undoing back to a saved state, doesn't
// falsely mark the tab as modified.
void ClearTextFrame::OnSavePointLeft(wxStyledTextEvent &event)
{
    wxStyledTextCtrl *stc = (wxStyledTextCtrl*)event.GetEventObject();
    int index = IndexOf(stc);
    if (index != wxNOT_FOUND && !m_tabData[index].modified)
    {
        m_tabData[index].modified = true;
        UpdateTabLabel(index);
    }
    event.Skip();
}

void ClearTextFrame::OnSavePointReached(wxStyledTextEvent &event)
{
    wxStyledTextCtrl *stc = (wxStyledTextCtrl*)event.GetEventObject();
    int index = IndexOf(stc);
    if (index != wxNOT_FOUND && m_tabData[index].modified)
    {
        m_tabData[index].modified = false;
        UpdateTabLabel(index);
    }
    event.Skip();
}

void ClearTextFrame::OnLineCountChange(wxStyledTextEvent &event)
{
    wxStyledTextCtrl *stc = (wxStyledTextCtrl*)event.GetEventObject();
    UpdateMarginWidth(stc);
    event.Skip();
}

// Fires on caret movement, selection changes, and edits alike -- the
// one Scintilla event that covers everything the status bar and brace
// highlight need to track.
void ClearTextFrame::OnEditorUpdateUI(wxStyledTextEvent &event)
{
    wxStyledTextCtrl *stc = (wxStyledTextCtrl*)event.GetEventObject();
    UpdateBraceHighlight(stc);
    if (stc == CurrentText())
        UpdateStatusBarPosition(stc);
    event.Skip();
}

// Highlights the brace pair around the caret (checking both the
// character under it and just before it, since after typing a brace
// the caret sits immediately past it) and flags an unmatched one in red.
void ClearTextFrame::UpdateBraceHighlight(wxStyledTextCtrl *stc)
{
    int pos = stc->GetCurrentPos();
    int ch = pos < stc->GetTextLength() ? stc->GetCharAt(pos) : 0;

    if (!IsBraceChar(ch) && pos > 0)
    {
        int chBefore = stc->GetCharAt(pos - 1);
        if (IsBraceChar(chBefore)) { pos -= 1; ch = chBefore; }
    }

    if (IsBraceChar(ch))
    {
        int match = stc->BraceMatch(pos);
        if (match != wxSTC_INVALID_POSITION)
            stc->BraceHighlight(pos, match);
        else
            stc->BraceBadLight(pos);
    }
    else
    {
        stc->BraceHighlight(wxSTC_INVALID_POSITION, wxSTC_INVALID_POSITION);
    }
}

void ClearTextFrame::UpdateStatusBarPosition(wxStyledTextCtrl *stc)
{
    if (!stc) { SetStatusText(""); return; }

    int pos = stc->GetCurrentPos();
    int line = stc->LineFromPosition(pos) + 1;
    int col = stc->GetColumn(pos) + 1;
    wxString status = wxString::Format("Line %d, Col %d", line, col);

    int selLen = stc->GetSelectionEnd() - stc->GetSelectionStart();
    if (selLen > 0)
        status += wxString::Format("   |   %d selected", selLen);

    SetStatusText(status);
}

// Toggles the clicked fold point open/closed.
void ClearTextFrame::OnMarginClick(wxStyledTextEvent &event)
{
    if (event.GetMargin() != 2) { event.Skip(); return; }
    wxStyledTextCtrl *stc = (wxStyledTextCtrl*)event.GetEventObject();
    stc->ToggleFold(stc->LineFromPosition(event.GetPosition()));
}

void ClearTextFrame::OnPageChanged(wxAuiNotebookEvent &event)
{
    UpdateTitle();
    UpdateLanguageMenuChecks();

    int sel = m_notebook->GetSelection();
    if (sel != wxNOT_FOUND)
    {
        CheckExternalModification(sel);
        UpdateStatusBarPosition(PageText(sel));
    }
    event.Skip();
}

// wxAuiNotebook's own close button (and middle-click) would otherwise
// close the tab itself, bypassing the unsaved-changes prompt and leaving
// m_tabData out of sync with the notebook. Veto its default handling and
// route through CloseTab instead, so the X behaves exactly like Ctrl+W /
// File > Close Tab.
void ClearTextFrame::OnPageClose(wxAuiNotebookEvent &event)
{
    event.Veto();
    CloseTab(event.GetSelection());
}

// When the window regains focus, re-check the active tab for changes
// made by another program while ClearText was in the background.
void ClearTextFrame::OnActivate(wxActivateEvent &event)
{
    if (event.GetActive())
    {
        int sel = m_notebook->GetSelection();
        if (sel != wxNOT_FOUND) CheckExternalModification(sel);
    }
    event.Skip();
}

// Warns and offers to reload if the file backing `index` has a newer
// modification time on disk than what we last read/wrote ourselves.
void ClearTextFrame::CheckExternalModification(int index)
{
    const wxString &path = m_tabData[index].filePath;
    if (path.IsEmpty() || !wxFileExists(path)) return;

    wxDateTime diskTime = wxFileName(path).GetModificationTime();
    if (!diskTime.IsValid() || !m_tabData[index].fileModTime.IsValid()) return;
    if (diskTime <= m_tabData[index].fileModTime) return;

    // Record the new time before prompting so a "No" answer doesn't
    // trigger the same prompt again on every focus/tab switch.
    m_tabData[index].fileModTime = diskTime;

    int result = wxMessageBox(
        "\"" + wxFileName(path).GetFullName() + "\" has changed on disk.\n\nReload it?",
        "File Changed", wxYES_NO | wxICON_WARNING, this);
    if (result == wxYES)
        ReloadTab(index, true);
}

// ============================================================================
// Theme / language
// ============================================================================

// Re-applies highlighting (theme + font size + lexer) to every open tab.
// Used whenever a whole-window setting changes, so it's a one-shot
// change rather than something each tab tracks separately.
void ClearTextFrame::ReapplyHighlightingToAllTabs()
{
    for (size_t i = 0; i < m_notebook->GetPageCount(); i++)
    {
        wxStyledTextCtrl *stc = PageText((int)i);
        ApplyHighlighting(stc, EffectiveLanguage((int)i));
        UpdateMarginWidth(stc);
    }
}

// Clears and repopulates the Theme submenu from CustomThemes::All()
// (built-in themes.h entries followed by any user-defined ones from the
// config file). Called at startup and whenever that list's size can have
// changed, i.e. after OnReloadCustomThemes.
void ClearTextFrame::RebuildThemeMenu()
{
    while (m_themeMenu->GetMenuItemCount() > 0)
        m_themeMenu->Destroy(m_themeMenu->FindItemByPosition(0));

    const std::vector<EditorTheme> &themes = CustomThemes::All();
    for (size_t i = 0; i < themes.size(); i++)
    {
        wxMenuItem *item = m_themeMenu->AppendRadioItem(ID_ThemeBase + (int)i, themes[i].name);
        if ((int)i == GetThemeIndex()) item->Check(true);
    }
    m_themeMenu->AppendSeparator();
    m_themeMenu->Append(ID_ReloadCustomThemes, "Reload Custom Themes");
}

void ClearTextFrame::OnSetTheme(wxCommandEvent &event)
{
    int idx = event.GetId() - ID_ThemeBase;
    if (idx < 0 || idx >= (int)CustomThemes::All().size()) return;
    SetThemeIndex(idx);
    ReapplyHighlightingToAllTabs();
}

// Re-reads [CustomThemes] from the config file (picking up hand-edited
// additions/changes without a restart) and rebuilds the Theme submenu to
// match. If the previously-selected theme no longer exists, highlighting.h's
// CurrentTheme() falls back to theme 0 on its own.
void ClearTextFrame::OnReloadCustomThemes(wxCommandEvent &event)
{
    CustomThemes::Reload();
    RebuildThemeMenu();
    ReapplyHighlightingToAllTabs();
}

// Applies an explicit Language-menu choice to the current tab only,
// overriding extension-based detection until it's set back to
// Auto-Detect (or another file is opened in a fresh tab).
void ClearTextFrame::OnSetLanguage(wxCommandEvent &event)
{
    int sel = m_notebook->GetSelection();
    if (sel == wxNOT_FOUND) return;

    int idx = event.GetId() - ID_LanguageBase;
    const std::vector<LanguageInfo> &languages = AllLanguages();
    if (idx < 0 || idx >= (int)languages.size()) return;

    m_tabData[sel].language = languages[idx].id;
    ApplyHighlighting(PageText(sel), EffectiveLanguage(sel));
    UpdateMarginWidth(PageText(sel));
}

// ============================================================================
// Tab lifecycle: new / close / save / reload
// ============================================================================

void ClearTextFrame::OnNewTab(wxCommandEvent &event)
{
    AddTab("Untitled");
}

void ClearTextFrame::OnCloseTab(wxCommandEvent &event)
{
    CloseTab(m_notebook->GetSelection());
}

bool ClearTextFrame::CloseTab(int index, bool addNewIfEmpty)
{
    if (index == wxNOT_FOUND) return true;

    if (m_tabData[index].modified)
    {
        int result = wxMessageBox(
            "Save changes before closing this tab?",
            "Unsaved Changes", wxYES_NO | wxCANCEL | wxICON_QUESTION, this);
        if (result == wxCANCEL) return false;
        if (result == wxYES)
        {
            m_notebook->SetSelection(index);
            if (!SaveTab(index)) return false;
        }
    }

    m_notebook->DeletePage(index);
    m_tabData.erase(m_tabData.begin() + index);

    if (addNewIfEmpty && m_notebook->GetPageCount() == 0)
        AddTab("Untitled");

    UpdateTitle();
    return true;
}

bool ClearTextFrame::SaveTab(int index)
{
    wxStyledTextCtrl *stc = PageText(index);

    if (m_tabData[index].filePath.IsEmpty())
    {
        wxFileDialog dlg(this, "Save As", "", "", "Text files (*.txt)|*.txt|All files (*.*)|*.*",
            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dlg.ShowModal() == wxID_CANCEL) return false;
        m_tabData[index].filePath = dlg.GetPath();
        ApplyHighlighting(stc, EffectiveLanguage(index));
        UpdateMarginWidth(stc);
    }

    if (m_trimTrailingWhitespace)
        TrimTrailingWhitespace(stc);

    if (!TextEncoding::WriteFile(m_tabData[index].filePath, stc->GetText()))
    {
        wxMessageBox("Failed to save file.", "Error", wxOK | wxICON_ERROR, this);
        return false;
    }

    m_tabData[index].modified = false;
    m_tabData[index].fileModTime = wxFileName(m_tabData[index].filePath).GetModificationTime();
    stc->SetSavePoint();
    UpdateTabLabel(index);
    UpdateTitle();
    AddToRecentFiles(m_tabData[index].filePath);
    return true;
}

// Strips trailing spaces/tabs from every line (never touching the
// line-ending characters themselves), as a single undo-able edit.
// Called from SaveTab when the "Trim Trailing Whitespace on Save"
// option is on.
void ClearTextFrame::TrimTrailingWhitespace(wxStyledTextCtrl *stc)
{
    int caretPos = stc->GetCurrentPos();
    int lineCount = stc->GetLineCount();

    stc->BeginUndoAction();
    for (int line = 0; line < lineCount; line++)
    {
        int lineStart = stc->PositionFromLine(line);
        int lineEnd = stc->GetLineEndPosition(line); // excludes EOL chars
        int trimStart = lineEnd;
        while (trimStart > lineStart)
        {
            int ch = stc->GetCharAt(trimStart - 1);
            if (ch != ' ' && ch != '\t') break;
            trimStart--;
        }
        if (trimStart < lineEnd)
        {
            stc->SetTargetStart(trimStart);
            stc->SetTargetEnd(lineEnd);
            stc->ReplaceTarget("");
        }
    }
    stc->EndUndoAction();

    stc->SetCurrentPos(wxMin(caretPos, stc->GetTextLength()));
}

void ClearTextFrame::OnSaveAll(wxCommandEvent &event)
{
    for (size_t i = 0; i < m_tabData.size(); i++)
        if (m_tabData[i].modified)
            if (!SaveTab((int)i)) return; // cancelled/failed -- leave the rest untouched
}

void ClearTextFrame::OnReload(wxCommandEvent &event)
{
    int sel = m_notebook->GetSelection();
    if (sel == wxNOT_FOUND) return;
    ReloadTab(sel, false);
}

// Re-reads `index`'s file from disk, discarding any in-editor changes.
// With `force` false and the tab modified, confirms with the user first
// (used by the Reload menu item); `force` true skips the prompt (used
// by CheckExternalModification, which has already asked).
bool ClearTextFrame::ReloadTab(int index, bool force)
{
    const wxString &path = m_tabData[index].filePath;
    if (path.IsEmpty() || !wxFileExists(path)) return false;

    if (!force && m_tabData[index].modified)
    {
        int result = wxMessageBox(
            "Discard unsaved changes and reload \"" + wxFileName(path).GetFullName() +
                "\" from disk?",
            "Reload File", wxYES_NO | wxICON_QUESTION, this);
        if (result != wxYES) return false;
    }

    wxString content;
    if (!TextEncoding::ReadFile(path, content))
    {
        wxMessageBox("Could not reload file:\n" + path, "Error", wxOK | wxICON_ERROR, this);
        return false;
    }

    wxStyledTextCtrl *stc = PageText(index);
    int firstVisible = stc->GetFirstVisibleLine();
    stc->SetText(content);
    stc->EmptyUndoBuffer();
    stc->SetSavePoint();
    stc->SetFirstVisibleLine(firstVisible);

    m_tabData[index].modified = false;
    m_tabData[index].fileModTime = wxFileName(path).GetModificationTime();
    UpdateTabLabel(index);
    UpdateTitle();
    return true;
}

void ClearTextFrame::OnOpen(wxCommandEvent &event)
{
    wxFileDialog dlg(this, "Open File", "", "", "Text files (*.txt)|*.txt|All files (*.*)|*.*",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_CANCEL) return;

    OpenFilePath(dlg.GetPath());
}

void ClearTextFrame::OnSave(wxCommandEvent &event)
{
    SaveTab(m_notebook->GetSelection());
}

void ClearTextFrame::OnSaveAs(wxCommandEvent &event)
{
    int sel = m_notebook->GetSelection();
    if (sel == wxNOT_FOUND) return;
    m_tabData[sel].filePath = "";
    SaveTab(sel);
}

void ClearTextFrame::OnExit(wxCommandEvent &event)
{
    Close();
}

void ClearTextFrame::OnCloseWindow(wxCloseEvent &event)
{
    // Snapshot which files are open *before* closing anything, so a
    // mid-close cancel (unsaved-changes prompt) doesn't leave us with a
    // half-updated list to persist.
    wxArrayString openFiles;
    for (const TabData &tab : m_tabData)
        if (!tab.filePath.IsEmpty())
            openFiles.Add(tab.filePath);

    while (m_notebook->GetPageCount() > 0)
    {
        if (!CloseTab(0, false)) // don't re-add "Untitled" while shutting down
        {
            event.Veto();
            return;
        }
    }

    SaveSession(openFiles);

    // Keeps the saved geometry from collapsing to the maximized window's
    // own coordinates: reuse whatever was last saved for the "restored"
    // rect if the window is currently maximized.
    AppConfig::WindowGeometry geom = AppConfig::GetWindowGeometry();
    geom.maximized = IsMaximized();
    if (!geom.maximized)
    {
        wxSize sz = GetSize();
        wxPoint pos = GetPosition();
        geom.width = sz.GetWidth();
        geom.height = sz.GetHeight();
        geom.x = pos.x;
        geom.y = pos.y;
    }
    AppConfig::SaveWindowGeometry(geom);

    if (m_findReplaceDialog)
    {
        m_findReplaceDialog->Destroy();
        m_findReplaceDialog = nullptr;
    }
    if (m_findInFilesDialog)
    {
        m_findInFilesDialog->Destroy();
        m_findInFilesDialog = nullptr;
    }

    Destroy();
}

// ============================================================================
// Session / recent-files persistence (see app_config.h)
// ============================================================================

// Persists the active theme and the set of files that were open, so the
// next launch (with no files on the command line) can restore them.
void ClearTextFrame::SaveSession(const wxArrayString &openFiles)
{
    AppConfig::SaveSession(GetThemeIndex(), GetFontSize(), m_showWhitespace,
                            m_trimTrailingWhitespace, openFiles);
}

// Reads recent-files and whitespace-toggle state from the config file
// ClearTextApp::OnInit() already installed as the process default, before
// this frame's menus (which display that state) are built.
void ClearTextFrame::LoadFrameSettings()
{
    m_recentFiles = AppConfig::GetRecentFiles(kMaxRecentFiles);
    m_showWhitespace = AppConfig::GetShowWhitespace(false);
    m_trimTrailingWhitespace = AppConfig::GetTrimTrailingWhitespace(true);
}

void ClearTextFrame::SaveRecentFiles()
{
    AppConfig::SaveRecentFiles(m_recentFiles);
}

// Moves `path` to the front of the recent-files list (adding it if new),
// caps the list at kMaxRecentFiles, and persists immediately -- called
// whenever a tab gets a concrete on-disk path via open or save.
void ClearTextFrame::AddToRecentFiles(const wxString &path)
{
    if (path.IsEmpty()) return;

    wxFileName fn(path);
    fn.MakeAbsolute();
    wxString full = fn.GetFullPath();

    int existing = m_recentFiles.Index(full);
    if (existing != wxNOT_FOUND) m_recentFiles.RemoveAt(existing);
    m_recentFiles.Insert(full, 0);
    while (m_recentFiles.size() > kMaxRecentFiles)
        m_recentFiles.RemoveAt(m_recentFiles.size() - 1);

    RebuildRecentFilesMenu();
    SaveRecentFiles();
}

// Clears and repopulates m_recentMenu's items from m_recentFiles.
void ClearTextFrame::RebuildRecentFilesMenu()
{
    while (m_recentMenu->GetMenuItemCount() > 0)
        m_recentMenu->Destroy(m_recentMenu->FindItemByPosition(0));

    if (m_recentFiles.IsEmpty())
    {
        wxMenuItem *empty = m_recentMenu->Append(wxID_ANY, "(No Recent Files)");
        empty->Enable(false);
        return;
    }

    for (size_t i = 0; i < m_recentFiles.size(); i++)
    {
        wxString label = wxString::Format("&%d %s", (int)i + 1, m_recentFiles[i]);
        m_recentMenu->Append(ID_RecentFileBase + (int)i, label);
    }
    m_recentMenu->AppendSeparator();
    m_recentMenu->Append(ID_ClearRecentFiles, "Clear Recent Files");
}

void ClearTextFrame::OnOpenRecent(wxCommandEvent &event)
{
    int idx = event.GetId() - ID_RecentFileBase;
    if (idx < 0 || idx >= (int)m_recentFiles.size()) return;

    wxString path = m_recentFiles[idx];
    if (!wxFileExists(path))
    {
        wxMessageBox("File no longer exists:\n" + path, "Open Recent", wxOK | wxICON_WARNING, this);
        m_recentFiles.RemoveAt(idx);
        RebuildRecentFilesMenu();
        SaveRecentFiles();
        return;
    }
    OpenFilePath(path);
}

void ClearTextFrame::OnClearRecentFiles(wxCommandEvent &event)
{
    m_recentFiles.Clear();
    RebuildRecentFilesMenu();
    SaveRecentFiles();
}

// ============================================================================
// Basic edit commands
// ============================================================================

void ClearTextFrame::OnUndo(wxCommandEvent &event) { if (auto *t = CurrentText()) t->Undo(); }
void ClearTextFrame::OnRedo(wxCommandEvent &event) { if (auto *t = CurrentText()) t->Redo(); }
void ClearTextFrame::OnCut(wxCommandEvent &event) { if (auto *t = CurrentText()) t->Cut(); }
void ClearTextFrame::OnCopy(wxCommandEvent &event) { if (auto *t = CurrentText()) t->Copy(); }
void ClearTextFrame::OnPaste(wxCommandEvent &event) { if (auto *t = CurrentText()) t->Paste(); }
void ClearTextFrame::OnSelectAll(wxCommandEvent &event) { if (auto *t = CurrentText()) t->SelectAll(); }

void ClearTextFrame::OnPrint(wxCommandEvent &event)
{
    wxStyledTextCtrl *stc = CurrentText();
    if (!stc) return;

    int sel = m_notebook->GetSelection();
    wxString title = m_tabData[sel].filePath.IsEmpty()
        ? "Untitled" : wxFileName(m_tabData[sel].filePath).GetFullName();

    wxPrintDialogData printDialogData;
    wxPrinter printer(&printDialogData);
    TextPrintout printout(stc->GetText(), title);

    if (!printer.Print(this, &printout, true) &&
        printer.GetLastError() == wxPRINTER_ERROR)
    {
        wxMessageBox("There was an error printing.\n"
            "Check your printer setup.", "Printing Error", wxOK | wxICON_ERROR, this);
    }
}

void ClearTextFrame::OnAbout(wxCommandEvent &event)
{
    wxAboutDialogInfo info;
    info.SetName("ClearText");
    info.SetVersion("1.0");
    info.SetDescription("A simple multi-tab text editor built with wxWidgets.");
    info.SetCopyright("(C) 2026");
    info.SetLicense(
        "MIT License\n"
        "\n"
        "Permission is hereby granted, free of charge, to any person\n"
        "obtaining a copy of this software and associated documentation\n"
        "files (the \"Software\"), to deal in the Software without\n"
        "restriction, including without limitation the rights to use,\n"
        "copy, modify, merge, publish, distribute, sublicense, and/or\n"
        "sell copies of the Software, and to permit persons to whom the\n"
        "Software is furnished to do so, subject to the following\n"
        "conditions:\n"
        "\n"
        "The above copyright notice and this permission notice shall be\n"
        "included in all copies or substantial portions of the Software.\n"
        "\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY\n"
        "KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE\n"
        "WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE\n"
        "AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT\n"
        "HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,\n"
        "WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING\n"
        "FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR\n"
        "OTHER DEALINGS IN THE SOFTWARE."
    );
    wxAboutBox(info, this);
}

// ============================================================================
// Find / replace / go to line
// ============================================================================

// Searches for `text` starting from the current selection, wrapping
// around the document if not found before the end (or start).
bool ClearTextFrame::FindInEditor(wxStyledTextCtrl *stc, const wxString &text, int flags, bool forward)
{
    if (!stc || text.IsEmpty()) return false;

    int searchFlags = 0;
    if (flags & wxFR_MATCHCASE) searchFlags |= wxSTC_FIND_MATCHCASE;
    if (flags & wxFR_WHOLEWORD) searchFlags |= wxSTC_FIND_WHOLEWORD;
    stc->SetSearchFlags(searchFlags);

    int docLen = stc->GetTextLength();
    int foundPos;

    if (forward)
    {
        int startPos = stc->GetSelectionEnd();
        stc->SetTargetStart(startPos);
        stc->SetTargetEnd(docLen);
        foundPos = stc->SearchInTarget(text);
        if (foundPos == -1 && m_wrapAround) // wrap to the top
        {
            stc->SetTargetStart(0);
            stc->SetTargetEnd(docLen);
            foundPos = stc->SearchInTarget(text);
        }
    }
    else
    {
        int startPos = stc->GetSelectionStart();
        stc->SetTargetStart(startPos);
        stc->SetTargetEnd(0);
        foundPos = stc->SearchInTarget(text);
        if (foundPos == -1 && m_wrapAround) // wrap to the bottom
        {
            stc->SetTargetStart(docLen);
            stc->SetTargetEnd(0);
            foundPos = stc->SearchInTarget(text);
        }
    }

    if (foundPos == -1) return false;

    stc->SetSelection(foundPos, stc->GetTargetEnd());
    stc->EnsureCaretVisible();
    return true;
}

void ClearTextFrame::ShowFindDialog(bool replace)
{
    if (m_findReplaceDialog)
    {
        m_findReplaceDialog->Destroy();
        m_findReplaceDialog = nullptr;
    }

    int style = replace ? wxFR_REPLACEDIALOG : 0;
    m_findReplaceDialog = new wxFindReplaceDialog(
        this, &m_findData, replace ? "Replace" : "Find", style);
    m_findReplaceDialog->Show(true);
}

void ClearTextFrame::OnFindMenu(wxCommandEvent &event) { ShowFindDialog(false); }
void ClearTextFrame::OnReplaceMenu(wxCommandEvent &event) { ShowFindDialog(true); }
void ClearTextFrame::OnToggleWrapAround(wxCommandEvent &event) { m_wrapAround = event.IsChecked(); }

void ClearTextFrame::OnGoToLine(wxCommandEvent &event)
{
    wxStyledTextCtrl *stc = CurrentText();
    if (!stc) return;

    long maxLine = stc->GetLineCount();
    long current = stc->LineFromPosition(stc->GetCurrentPos()) + 1;
    long line = wxGetNumberFromUser(
        wxString::Format("Line number (1-%ld):", maxLine),
        "Line:", "Go To Line", current, 1, maxLine, this);
    if (line == -1) return; // cancelled

    stc->GotoLine((int)line - 1);
    stc->EnsureCaretVisible();
    stc->SetFocus();
}

// Opens (creating it the first time) the persistent Find in Files dialog.
// Closing it via its own window X just hides it (see FindInFilesDialog::
// OnClose), so search text and results survive being reopened.
void ClearTextFrame::OnFindInFiles(wxCommandEvent &event)
{
    if (!m_findInFilesDialog)
        m_findInFilesDialog = new FindInFilesDialog(this);
    m_findInFilesDialog->Show();
    m_findInFilesDialog->Raise();
    m_findInFilesDialog->FocusSearchField();
}

wxString ClearTextFrame::NotFoundMessage(const wxString &text)
{
    return m_wrapAround
        ? "\"" + text + "\" not found."
        : "\"" + text + "\" not found (wrap around is off).";
}

void ClearTextFrame::OnFindNext(wxCommandEvent &event)
{
    wxStyledTextCtrl *stc = CurrentText();
    wxString text = m_findData.GetFindString();
    if (!stc || text.IsEmpty())
    {
        ShowFindDialog(false);
        return;
    }
    bool forward = (m_findData.GetFlags() & wxFR_DOWN) != 0;
    if (!FindInEditor(stc, text, m_findData.GetFlags(), forward))
        wxMessageBox(NotFoundMessage(text), "Find", wxOK | wxICON_INFORMATION, this);
}

void ClearTextFrame::OnFindDialogEvent(wxFindDialogEvent &event)
{
    wxEventType type = event.GetEventType();

    if (type == wxEVT_FIND_CLOSE)
    {
        wxFindReplaceDialog *dlg = event.GetDialog();
        if (dlg) dlg->Destroy();
        if (dlg == m_findReplaceDialog) m_findReplaceDialog = nullptr;
        return;
    }

    wxStyledTextCtrl *stc = CurrentText();
    if (!stc) return;

    wxString findText = event.GetFindString();
    int flags = event.GetFlags();
    bool forward = (flags & wxFR_DOWN) != 0;

    if (type == wxEVT_FIND || type == wxEVT_FIND_NEXT)
    {
        if (!FindInEditor(stc, findText, flags, forward))
            wxMessageBox(NotFoundMessage(findText), "Find", wxOK | wxICON_INFORMATION, this);
    }
    else if (type == wxEVT_FIND_REPLACE)
    {
        wxString replaceText = event.GetReplaceString();
        wxString sel = stc->GetSelectedText();
        bool matchCase = (flags & wxFR_MATCHCASE) != 0;
        bool selMatches = matchCase ? (sel == findText) : (sel.CmpNoCase(findText) == 0);

        if (selMatches && !sel.IsEmpty())
            stc->ReplaceSelection(replaceText);

        FindInEditor(stc, findText, flags, forward);
    }
    else if (type == wxEVT_FIND_REPLACE_ALL)
    {
        if (findText.IsEmpty()) return;

        wxString replaceText = event.GetReplaceString();
        int searchFlags = 0;
        if (flags & wxFR_MATCHCASE) searchFlags |= wxSTC_FIND_MATCHCASE;
        if (flags & wxFR_WHOLEWORD) searchFlags |= wxSTC_FIND_WHOLEWORD;
        stc->SetSearchFlags(searchFlags);

        stc->BeginUndoAction();
        int count = 0;
        int searchFrom = 0;
        int docLen = stc->GetTextLength();
        while (true)
        {
            stc->SetTargetStart(searchFrom);
            stc->SetTargetEnd(docLen);
            int foundPos = stc->SearchInTarget(findText);
            if (foundPos == -1) break;

            stc->ReplaceTarget(replaceText);
            count++;

            int consumed = foundPos + (int)replaceText.length();
            searchFrom = consumed;
            docLen = stc->GetTextLength();
        }
        stc->EndUndoAction();

        wxMessageBox(wxString::Format("Replaced %d occurrence(s).", count),
            "Replace All", wxOK | wxICON_INFORMATION, this);
    }
}

// ============================================================================
// View menu: wrap / whitespace / zoom / full screen
// ============================================================================

void ClearTextFrame::OnToggleWordWrap(wxCommandEvent &event)
{
    int mode = event.IsChecked() ? wxSTC_WRAP_WORD : wxSTC_WRAP_NONE;
    for (size_t i = 0; i < m_notebook->GetPageCount(); i++)
        PageText((int)i)->SetWrapMode(mode);
}

void ClearTextFrame::OnToggleShowWhitespace(wxCommandEvent &event)
{
    m_showWhitespace = event.IsChecked();
    int mode = m_showWhitespace ? wxSTC_WS_VISIBLEALWAYS : wxSTC_WS_INVISIBLE;
    for (size_t i = 0; i < m_notebook->GetPageCount(); i++)
        PageText((int)i)->SetViewWhiteSpace(mode);
}

void ClearTextFrame::OnToggleTrimTrailingWhitespace(wxCommandEvent &event)
{
    m_trimTrailingWhitespace = event.IsChecked();
}

// These resize the actual base font (persisted, whole-window) rather
// than using wxStyledTextCtrl's own per-tab visual zoom, so the size a
// person picks is what gets saved and restored next launch.
void ClearTextFrame::ChangeFontSize(int delta)
{
    int newSize = GetFontSize() + delta;
    if (newSize < kMinFontSize || newSize > kMaxFontSize) return;
    SetFontSize(newSize);
    ReapplyHighlightingToAllTabs();
}

void ClearTextFrame::OnToggleFullScreen(wxCommandEvent &event)
{
    // Keep the menu bar visible in full screen (unlike the default
    // wxFULLSCREEN_ALL, which hides it along with the toolbar/statusbar).
    ShowFullScreen(!IsFullScreen(), wxFULLSCREEN_NOTOOLBAR | wxFULLSCREEN_NOSTATUSBAR |
        wxFULLSCREEN_NOBORDER | wxFULLSCREEN_NOCAPTION);
}
void ClearTextFrame::OnZoomIn(wxCommandEvent &event) { ChangeFontSize(+1); }
void ClearTextFrame::OnZoomOut(wxCommandEvent &event) { ChangeFontSize(-1); }

void ClearTextFrame::OnZoomReset(wxCommandEvent &event)
{
    SetFontSize(kDefaultFontSize);
    ReapplyHighlightingToAllTabs();
}

// Scintilla natively zooms on Ctrl+scroll-wheel, but that's separate
// per-tab state we don't persist. Intercept it here and redirect to the
// same whole-window, saved font-size change the menu items use, instead
// of letting the two zoom systems drift apart.
void ClearTextFrame::OnMouseWheel(wxMouseEvent &event)
{
    if (event.ControlDown())
    {
        ChangeFontSize(event.GetWheelRotation() > 0 ? +1 : -1);
        return;
    }
    event.Skip();
}

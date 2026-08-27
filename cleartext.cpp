#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/filename.h>
#include <wx/stc/stc.h>
#include <wx/print.h>
#include <wx/aboutdlg.h>
#include <wx/fdrepdlg.h>
#include <wx/snglinst.h>
#include <wx/ipc.h>
#include <wx/fileconf.h>
#include <wx/stdpaths.h>
#include <vector>
#include "themes.h"

// Simple printout that paginates plain text across pages using the
// device context's own text-measurement, so it scales to any paper size.
class TextPrintout : public wxPrintout
{
public:
    TextPrintout(const wxString &text, const wxString &title)
        : wxPrintout(title), m_text(text)
    {
        m_lines = wxSplit(m_text, '\n');
    }

    void OnPreparePrinting() override
    {
        wxDC *dc = GetDC();
        if (!dc) return;

        // Printer DCs run at a much higher DPI than the screen, so without
        // scaling, point sizes come out tiny relative to the page. Scale so
        // that our font size maps to its true physical size on paper.
        wxSize ppiPrinter, ppiScreen;
        GetPPIPrinter(&ppiPrinter.x, &ppiPrinter.y);
        GetPPIScreen(&ppiScreen.x, &ppiScreen.y);
        if (ppiScreen.x == 0) ppiScreen = wxSize(96, 96);
        float scale = (float)ppiPrinter.x / (float)ppiScreen.x;

        int pageW, pageH;
        GetPageSizePixels(&pageW, &pageH);
        wxSize dcSize = dc->GetSize();
        float pageScale = pageW > 0 ? (float)dcSize.x / (float)pageW : 1.0f;

        m_scale = scale * pageScale;
        dc->SetUserScale(m_scale, m_scale);
    }

    bool OnPrintPage(int page) override
    {
        wxDC *dc = GetDC();
        if (!dc) return false;

        dc->SetFont(wxFont(m_fontPointSize, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        int lineHeight = dc->GetCharHeight() + 4;

        int firstLine = (page - 1) * m_linesPerPage;
        int lastLine = wxMin(firstLine + m_linesPerPage, (int)m_lines.size());

        int y = 0;
        for (int i = firstLine; i < lastLine; i++)
        {
            dc->DrawText(m_lines[i], 0, y);
            y += lineHeight;
        }
        return true;
    }

    void GetPageInfo(int *minPage, int *maxPage, int *pageFrom, int *pageTo) override
    {
        wxDC *dc = GetDC();
        if (!dc)
        {
            *minPage = *maxPage = *pageFrom = *pageTo = 1;
            return;
        }

        // Usable page height in the DC's *scaled* (logical) units.
        int pageW, pageH;
        GetPageSizePixels(&pageW, &pageH);
        int usableH = m_scale > 0 ? (int)(pageH / m_scale) : pageH;

        dc->SetFont(wxFont(m_fontPointSize, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        int lineHeight = dc->GetCharHeight() + 4;
        if (lineHeight <= 0) lineHeight = 1;
        m_linesPerPage = wxMax(1, usableH / lineHeight);

        int pages = wxMax(1, (int)((m_lines.size() + m_linesPerPage - 1) / m_linesPerPage));
        *minPage = 1;
        *maxPage = pages;
        *pageFrom = 1;
        *pageTo = pages;
    }

    bool HasPage(int page) override
    {
        return page <= wxMax(1, (int)((m_lines.size() + m_linesPerPage - 1) / m_linesPerPage));
    }

private:
    wxString m_text;
    wxArrayString m_lines;
    int m_linesPerPage = 1;
    int m_fontPointSize = 14;
    float m_scale = 1.0f;
};

// ============================================================================
// PERSISTED SETTINGS (theme + last-session file list)
// ============================================================================

// A single file-backed config (~/.config/ClearText/cleartext.conf on Linux,
// %APPDATA%\ClearText\cleartext.conf on Windows) rather than wx's default
// wxConfig backend, so behavior — and the on-disk format — matches on both
// platforms instead of Windows silently using the registry.
static wxString GetConfigFilePath()
{
    wxString dir = wxStandardPaths::Get().GetUserConfigDir() +
        wxFileName::GetPathSeparator() + "ClearText";
    if (!wxDirExists(dir))
        wxFileName::Mkdir(dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    return dir + wxFileName::GetPathSeparator() + "cleartext.conf";
}

// ============================================================================
// SYNTAX HIGHLIGHTING (Scintilla built-in lexers, chosen by file extension)
// ============================================================================

// Index into AllThemes() for the currently active theme, applied to every
// tab (existing and new). A plain process-wide global rather than a frame
// member since the lexer helper functions below are free functions shared
// by every editor instance.
static int g_themeIndex = 0;

// Base editor font size in points, applied to every tab the same way the
// theme is. Adjusted via View > Increase/Decrease/Reset Font Size.
static const int kMinFontSize = 6;
static const int kMaxFontSize = 36;
static const int kDefaultFontSize = 10;
static int g_fontSize = kDefaultFontSize;

static const EditorTheme &CurrentTheme()
{
    return AllThemes()[g_themeIndex];
}

static void SetCommonStyleDefaults(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();

    // Named lvalue: some wx builds (e.g. mingw wx 3.0) declare
    // StyleSetFont(int, wxFont&) as a non-const reference, which can't
    // bind to a temporary.
    wxFont editorFont(g_fontSize, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    stc->StyleSetFont(wxSTC_STYLE_DEFAULT, editorFont);
    stc->StyleSetBackground(wxSTC_STYLE_DEFAULT, th.background);
    stc->StyleSetForeground(wxSTC_STYLE_DEFAULT, th.foreground);
    stc->StyleClearAll(); // propagates STYLE_DEFAULT (font/color) to every style

    // Scintilla has its own separate, per-tab "zoom" concept (e.g. from
    // Ctrl+scroll-wheel) layered on top of the base font size. It's never
    // persisted, so if it's left non-zero here the displayed size silently
    // drifts from — and stops matching — the saved FontSize preference.
    stc->SetZoom(0);

    stc->SetCaretForeground(th.caret);
    stc->SetSelBackground(true, th.selectionBg);
    stc->StyleSetBackground(wxSTC_STYLE_LINENUMBER, th.marginBg);
    stc->StyleSetForeground(wxSTC_STYLE_LINENUMBER, th.marginFg);
    stc->SetWhitespaceBackground(true, th.background);
    stc->SetWhitespaceForeground(true, th.marginFg);
}

static void ApplyPlainText(wxStyledTextCtrl *stc)
{
    stc->SetLexer(wxSTC_LEX_NULL);
    SetCommonStyleDefaults(stc);
}

static void ApplyCppStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_CPP);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "if else for while do return break continue switch case default "
        "class struct public private protected namespace using template "
        "typename const static virtual override new delete nullptr enum "
        "union sizeof this throw try catch friend inline operator explicit "
        "extern auto volatile unsigned signed short long void int float "
        "double char bool true false import export function var let");

    stc->StyleSetForeground(wxSTC_C_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_C_COMMENTLINE, th.comment);
    stc->StyleSetForeground(wxSTC_C_COMMENTDOC, th.comment);
    stc->StyleSetForeground(wxSTC_C_COMMENTLINEDOC, th.comment);
    stc->StyleSetForeground(wxSTC_C_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_C_STRING, th.string);
    stc->StyleSetForeground(wxSTC_C_CHARACTER, th.string);
    stc->StyleSetForeground(wxSTC_C_STRINGEOL, th.string);
    stc->StyleSetForeground(wxSTC_C_PREPROCESSOR, th.preprocessor);
    stc->StyleSetForeground(wxSTC_C_WORD, th.keyword);
    stc->StyleSetBold(wxSTC_C_WORD, true);
    stc->StyleSetForeground(wxSTC_C_WORD2, th.keyword2);
    stc->StyleSetForeground(wxSTC_C_OPERATOR, th.operatorColor);
}

static void ApplyPythonStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_PYTHON);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "False None True and as assert async await break class continue "
        "def del elif else except finally for from global if import in is "
        "lambda nonlocal not or pass raise return try while with yield");

    stc->StyleSetForeground(wxSTC_P_COMMENTLINE, th.comment);
    stc->StyleSetForeground(wxSTC_P_COMMENTBLOCK, th.comment);
    stc->StyleSetForeground(wxSTC_P_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_P_STRING, th.string);
    stc->StyleSetForeground(wxSTC_P_CHARACTER, th.string);
    stc->StyleSetForeground(wxSTC_P_TRIPLE, th.string);
    stc->StyleSetForeground(wxSTC_P_TRIPLEDOUBLE, th.string);
    stc->StyleSetForeground(wxSTC_P_WORD, th.keyword);
    stc->StyleSetBold(wxSTC_P_WORD, true);
    stc->StyleSetForeground(wxSTC_P_CLASSNAME, th.keyword2);
    stc->StyleSetBold(wxSTC_P_CLASSNAME, true);
    stc->StyleSetForeground(wxSTC_P_DEFNAME, th.keyword2);
    stc->StyleSetBold(wxSTC_P_DEFNAME, true);
    stc->StyleSetForeground(wxSTC_P_OPERATOR, th.operatorColor);
    stc->StyleSetForeground(wxSTC_P_DECORATOR, th.preprocessor);
}

static void ApplyMarkupStyles(wxStyledTextCtrl *stc, bool isXml)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(isXml ? wxSTC_LEX_XML : wxSTC_LEX_HTML);
    SetCommonStyleDefaults(stc);

    stc->StyleSetForeground(wxSTC_H_TAG, th.tag);
    stc->StyleSetBold(wxSTC_H_TAG, true);
    stc->StyleSetForeground(wxSTC_H_TAGEND, th.tag);
    stc->StyleSetForeground(wxSTC_H_ATTRIBUTE, th.number);
    stc->StyleSetForeground(wxSTC_H_ATTRIBUTEUNKNOWN, th.number);
    stc->StyleSetForeground(wxSTC_H_DOUBLESTRING, th.string);
    stc->StyleSetForeground(wxSTC_H_SINGLESTRING, th.string);
    stc->StyleSetForeground(wxSTC_H_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_H_ENTITY, th.preprocessor);
    stc->StyleSetForeground(wxSTC_H_NUMBER, th.number);
}

static void ApplyMarkdownStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_MARKDOWN);
    SetCommonStyleDefaults(stc);

    // Headers 1-6 all share the same "heading" color; only weight/size vary
    // a little so bigger headings still read as bigger at a glance.
    int headerStyles[] = {
        wxSTC_MARKDOWN_HEADER1, wxSTC_MARKDOWN_HEADER2, wxSTC_MARKDOWN_HEADER3,
        wxSTC_MARKDOWN_HEADER4, wxSTC_MARKDOWN_HEADER5, wxSTC_MARKDOWN_HEADER6
    };
    for (int i = 0; i < 6; i++)
    {
        stc->StyleSetForeground(headerStyles[i], th.tag);
        stc->StyleSetBold(headerStyles[i], true);
        stc->StyleSetSize(headerStyles[i], 10 + (6 - i)); // H1 largest, H6 smallest
    }

    stc->StyleSetBold(wxSTC_MARKDOWN_STRONG1, true);
    stc->StyleSetBold(wxSTC_MARKDOWN_STRONG2, true);
    stc->StyleSetItalic(wxSTC_MARKDOWN_EM1, true);
    stc->StyleSetItalic(wxSTC_MARKDOWN_EM2, true);
    stc->StyleSetForeground(wxSTC_MARKDOWN_STRIKEOUT, th.comment);
    stc->StyleSetForeground(wxSTC_MARKDOWN_BLOCKQUOTE, th.comment);
    stc->StyleSetItalic(wxSTC_MARKDOWN_BLOCKQUOTE, true);
    stc->StyleSetForeground(wxSTC_MARKDOWN_PRECHAR, th.preprocessor);
    stc->StyleSetForeground(wxSTC_MARKDOWN_ULIST_ITEM, th.preprocessor);
    stc->StyleSetForeground(wxSTC_MARKDOWN_OLIST_ITEM, th.preprocessor);
    stc->StyleSetForeground(wxSTC_MARKDOWN_HRULE, th.comment);
    stc->StyleSetForeground(wxSTC_MARKDOWN_LINK, th.attribute);
    stc->StyleSetForeground(wxSTC_MARKDOWN_CODE, th.markupCode);
    stc->StyleSetForeground(wxSTC_MARKDOWN_CODE2, th.markupCode);
    stc->StyleSetForeground(wxSTC_MARKDOWN_CODEBK, th.markupCode);
}

// Picks a lexer + color palette based on the file's extension; falls back
// to plain, unstyled text for unrecognized or missing extensions.
static void ApplyHighlighting(wxStyledTextCtrl *stc, const wxString &filePath)
{
    wxString ext = filePath.IsEmpty() ? "" : wxFileName(filePath).GetExt().Lower();

    if (ext == "c" || ext == "cpp" || ext == "cc" || ext == "cxx" ||
        ext == "h" || ext == "hpp" || ext == "hxx" ||
        ext == "java" || ext == "js" || ext == "cs")
        ApplyCppStyles(stc);
    else if (ext == "py")
        ApplyPythonStyles(stc);
    else if (ext == "html" || ext == "htm")
        ApplyMarkupStyles(stc, false);
    else if (ext == "xml")
        ApplyMarkupStyles(stc, true);
    else if (ext == "md" || ext == "markdown")
        ApplyMarkdownStyles(stc);
    else
        ApplyPlainText(stc);

    stc->Colourise(0, -1); // force a full re-lex now that styles/keywords changed
}

enum
{
    ID_NewTab = wxID_HIGHEST + 1,
    ID_CloseTab,
    ID_FindNext,
    ID_WrapAround,
    ID_WordWrap,
    ID_ToggleFullScreen,
    ID_ThemeBase // must stay last: one radio menu id per entry in AllThemes()
};

class ClearTextFrame : public wxFrame
{
public:
    ClearTextFrame() : wxFrame(nullptr, wxID_ANY, "ClearText", wxDefaultPosition, wxSize(800, 600))
    {
        wxMenuBar *menuBar = new wxMenuBar();

        wxMenu *fileMenu = new wxMenu();
        fileMenu->Append(ID_NewTab, "New Tab\tCtrl+T");
        fileMenu->Append(wxID_OPEN, "Open...\tCtrl+O");
        fileMenu->Append(wxID_SAVE, "Save\tCtrl+S");
        fileMenu->Append(wxID_SAVEAS, "Save As...\tCtrl+Shift+S");
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
        menuBar->Append(editMenu, "&Edit");

        wxMenu *viewMenu = new wxMenu();
        wxMenuItem *wordWrapItem = viewMenu->AppendCheckItem(ID_WordWrap, "Word Wrap");
        wordWrapItem->Check(true);
        viewMenu->AppendSeparator();
        viewMenu->Append(wxID_ZOOM_IN, "Increase Font Size\tCtrl+=");
        viewMenu->Append(wxID_ZOOM_OUT, "Decrease Font Size\tCtrl+-");
        viewMenu->Append(wxID_ZOOM_100, "Reset Font Size\tCtrl+0");
        viewMenu->AppendSeparator();
        viewMenu->Append(ID_ToggleFullScreen, "Full Screen\tF11");
        menuBar->Append(viewMenu, "&View");

        wxMenu *themeMenu = new wxMenu();
        const std::vector<EditorTheme> &themes = AllThemes();
        for (size_t i = 0; i < themes.size(); i++)
        {
            wxMenuItem *item = themeMenu->AppendRadioItem(ID_ThemeBase + (int)i, themes[i].name);
            if ((int)i == g_themeIndex) item->Check(true);
        }
        menuBar->Append(themeMenu, "&Theme");

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

        m_notebook = new wxNotebook(this, wxID_ANY);
        AddTab("Untitled");

        Bind(wxEVT_MENU, &ClearTextFrame::OnNewTab, this, ID_NewTab);
        Bind(wxEVT_MENU, &ClearTextFrame::OnCloseTab, this, ID_CloseTab);
        Bind(wxEVT_MENU, &ClearTextFrame::OnOpen, this, wxID_OPEN);
        Bind(wxEVT_MENU, &ClearTextFrame::OnSave, this, wxID_SAVE);
        Bind(wxEVT_MENU, &ClearTextFrame::OnSaveAs, this, wxID_SAVEAS);
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
        Bind(wxEVT_MENU, &ClearTextFrame::OnToggleWrapAround, this, ID_WrapAround);
        Bind(wxEVT_MENU, &ClearTextFrame::OnToggleWordWrap, this, ID_WordWrap);
        Bind(wxEVT_MENU, &ClearTextFrame::OnZoomIn, this, wxID_ZOOM_IN);
        Bind(wxEVT_MENU, &ClearTextFrame::OnZoomOut, this, wxID_ZOOM_OUT);
        Bind(wxEVT_MENU, &ClearTextFrame::OnZoomReset, this, wxID_ZOOM_100);
        Bind(wxEVT_MENU, &ClearTextFrame::OnToggleFullScreen, this, ID_ToggleFullScreen);
        Bind(wxEVT_MENU, &ClearTextFrame::OnSetTheme, this, ID_ThemeBase,
             ID_ThemeBase + (int)AllThemes().size() - 1);
        Bind(wxEVT_FIND, &ClearTextFrame::OnFindDialogEvent, this);
        Bind(wxEVT_FIND_NEXT, &ClearTextFrame::OnFindDialogEvent, this);
        Bind(wxEVT_FIND_REPLACE, &ClearTextFrame::OnFindDialogEvent, this);
        Bind(wxEVT_FIND_REPLACE_ALL, &ClearTextFrame::OnFindDialogEvent, this);
        Bind(wxEVT_FIND_CLOSE, &ClearTextFrame::OnFindDialogEvent, this);
        Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &ClearTextFrame::OnPageChanged, this);
        Bind(wxEVT_CLOSE_WINDOW, &ClearTextFrame::OnCloseWindow, this);
    }

    // Opens `path` in a new tab, reading its content from disk. Public so
    // it can be called for files passed on the command line.
    void OpenFilePath(const wxString &path)
    {
        if (!wxFileExists(path))
        {
            // No file there yet — open a blank tab pointed at this path so
            // Save writes it there directly, rather than erroring out.
            AddTab(wxFileName(path).GetFullName(), "", path);
            return;
        }

        wxString content;
        wxFile file(path);
        if (file.IsOpened())
            file.ReadAll(&content);
        else
        {
            wxMessageBox("Could not open file:\n" + path, "Error", wxOK | wxICON_ERROR, this);
            return;
        }

        AddTab(wxFileName(path).GetFullName(), content, path);
    }

    // Closes tab 0 if it's still the untouched, unsaved "Untitled" tab
    // created by the constructor — used after opening files from argv so
    // we don't leave a spare blank tab sitting in front of them.
    void CloseInitialBlankTabIfUnused()
    {
        if (m_notebook->GetPageCount() < 2) return;
        if (!m_tabData[0].filePath.IsEmpty()) return;
        if (m_tabData[0].modified) return;
        if (!PageText(0)->IsEmpty()) return;

        CloseTab(0, false);
    }

private:
    struct TabData
    {
        wxString filePath;
        bool modified = false;
    };

    wxNotebook *m_notebook;
    std::vector<TabData> m_tabData;
    wxFindReplaceData m_findData{wxFR_DOWN};
    wxFindReplaceDialog *m_findReplaceDialog = nullptr;
    bool m_wrapAround = true;

    wxStyledTextCtrl* CurrentText()
    {
        int sel = m_notebook->GetSelection();
        if (sel == wxNOT_FOUND) return nullptr;
        return (wxStyledTextCtrl*)m_notebook->GetPage(sel);
    }

    wxStyledTextCtrl* PageText(int index)
    {
        return (wxStyledTextCtrl*)m_notebook->GetPage(index);
    }

    void SetupEditor(wxStyledTextCtrl *stc)
    {
        // Line number margin
        stc->SetMarginType(0, wxSTC_MARGIN_NUMBER);
        stc->SetMarginWidth(1, 0); // hide folding/symbol margin

        stc->SetTabWidth(4);
        stc->SetUseTabs(false);
        stc->SetWrapMode(wxSTC_WRAP_WORD);
    }

    // Grows the line-number margin as the document gains more digits
    void UpdateMarginWidth(wxStyledTextCtrl *stc)
    {
        int lines = stc->GetLineCount();
        wxString widest = wxString::Format("_%d", lines);
        stc->SetMarginWidth(0, stc->TextWidth(wxSTC_STYLE_LINENUMBER, widest));
    }

    void AddTab(const wxString &title, const wxString &content = "", const wxString &filePath = "")
    {
        // Push tab metadata BEFORE AddPage, since AddPage synchronously fires
        // a page-changed event whose handler reads m_tabData by index.
        m_tabData.push_back({filePath, false});

        wxStyledTextCtrl *stc = new wxStyledTextCtrl(m_notebook, wxID_ANY);
        SetupEditor(stc);
        ApplyHighlighting(stc, filePath);
        if (!content.IsEmpty())
            stc->SetText(content);
        stc->EmptyUndoBuffer();
        stc->SetSavePoint(); // mark this freshly-loaded content as "unmodified"

        stc->Bind(wxEVT_STC_SAVEPOINTLEFT, &ClearTextFrame::OnSavePointLeft, this);
        stc->Bind(wxEVT_STC_SAVEPOINTREACHED, &ClearTextFrame::OnSavePointReached, this);
        stc->Bind(wxEVT_STC_CHANGE, &ClearTextFrame::OnLineCountChange, this);
        stc->Bind(wxEVT_MOUSEWHEEL, &ClearTextFrame::OnMouseWheel, this);

        m_notebook->AddPage(stc, title, true);
        UpdateMarginWidth(stc);
        UpdateTitle();
    }

    void UpdateTabLabel(int index)
    {
        wxString label = m_tabData[index].filePath.IsEmpty()
            ? "Untitled" : wxFileName(m_tabData[index].filePath).GetFullName();
        if (m_tabData[index].modified) label += " *";
        m_notebook->SetPageText(index, label);
    }

    void UpdateTitle()
    {
        int sel = m_notebook->GetSelection();
        if (sel == wxNOT_FOUND || sel >= (int)m_tabData.size()) { SetTitle("ClearText"); return; }
        wxString name = m_tabData[sel].filePath.IsEmpty()
            ? "Untitled" : wxFileName(m_tabData[sel].filePath).GetFullName();
        SetTitle(name + " - ClearText");
    }

    int IndexOf(wxStyledTextCtrl *stc)
    {
        for (size_t i = 0; i < m_notebook->GetPageCount(); i++)
            if (m_notebook->GetPage(i) == stc) return (int)i;
        return wxNOT_FOUND;
    }

    // Scintilla's own "save point" tracks modification relative to the last
    // SetSavePoint() call, rather than every text-change event — so loading
    // a file's initial content, or undoing back to a saved state, doesn't
    // falsely mark the tab as modified.
    void OnSavePointLeft(wxStyledTextEvent &event)
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

    void OnSavePointReached(wxStyledTextEvent &event)
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

    void OnLineCountChange(wxStyledTextEvent &event)
    {
        wxStyledTextCtrl *stc = (wxStyledTextCtrl*)event.GetEventObject();
        UpdateMarginWidth(stc);
        event.Skip();
    }

    void OnPageChanged(wxBookCtrlEvent &event)
    {
        UpdateTitle();
        event.Skip();
    }

    // Re-applies highlighting (theme + font size + lexer) to every open tab.
    // Used whenever a whole-window setting changes, so it's a one-shot
    // change rather than something each tab tracks separately.
    void ReapplyHighlightingToAllTabs()
    {
        for (size_t i = 0; i < m_notebook->GetPageCount(); i++)
        {
            wxStyledTextCtrl *stc = PageText((int)i);
            ApplyHighlighting(stc, m_tabData[i].filePath);
            UpdateMarginWidth(stc);
        }
    }

    void OnSetTheme(wxCommandEvent &event)
    {
        int idx = event.GetId() - ID_ThemeBase;
        if (idx < 0 || idx >= (int)AllThemes().size()) return;
        g_themeIndex = idx;
        ReapplyHighlightingToAllTabs();
    }

    void OnNewTab(wxCommandEvent &event)
    {
        AddTab("Untitled");
    }

    void OnCloseTab(wxCommandEvent &event)
    {
        CloseTab(m_notebook->GetSelection());
    }

    bool CloseTab(int index, bool addNewIfEmpty = true)
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

    bool SaveTab(int index)
    {
        wxStyledTextCtrl *stc = PageText(index);

        if (m_tabData[index].filePath.IsEmpty())
        {
            wxFileDialog dlg(this, "Save As", "", "", "Text files (*.txt)|*.txt|All files (*.*)|*.*",
                wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
            if (dlg.ShowModal() == wxID_CANCEL) return false;
            m_tabData[index].filePath = dlg.GetPath();
            ApplyHighlighting(stc, m_tabData[index].filePath);
            UpdateMarginWidth(stc);
        }

        wxFile file(m_tabData[index].filePath, wxFile::write);
        if (!file.IsOpened() || !file.Write(stc->GetText()))
        {
            wxMessageBox("Failed to save file.", "Error", wxOK | wxICON_ERROR, this);
            return false;
        }

        m_tabData[index].modified = false;
        stc->SetSavePoint();
        UpdateTabLabel(index);
        UpdateTitle();
        return true;
    }

    void OnOpen(wxCommandEvent &event)
    {
        wxFileDialog dlg(this, "Open File", "", "", "Text files (*.txt)|*.txt|All files (*.*)|*.*",
            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() == wxID_CANCEL) return;

        OpenFilePath(dlg.GetPath());
    }

    void OnSave(wxCommandEvent &event)
    {
        SaveTab(m_notebook->GetSelection());
    }

    void OnSaveAs(wxCommandEvent &event)
    {
        int sel = m_notebook->GetSelection();
        if (sel == wxNOT_FOUND) return;
        m_tabData[sel].filePath = "";
        SaveTab(sel);
    }

    void OnExit(wxCommandEvent &event)
    {
        Close();
    }

    void OnCloseWindow(wxCloseEvent &event)
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

        if (m_findReplaceDialog)
        {
            m_findReplaceDialog->Destroy();
            m_findReplaceDialog = nullptr;
        }

        Destroy();
    }

    // Persists the active theme and the set of files that were open, so the
    // next launch (with no files on the command line) can restore them.
    void SaveSession(const wxArrayString &openFiles)
    {
        wxConfigBase *cfg = wxConfigBase::Get(false);
        if (!cfg) return;

        cfg->Write("Theme", (long)g_themeIndex);
        cfg->Write("FontSize", (long)g_fontSize);

        cfg->DeleteGroup("LastSession");
        cfg->Write("LastSession/Count", (long)openFiles.size());
        for (size_t i = 0; i < openFiles.size(); i++)
            cfg->Write(wxString::Format("LastSession/File%zu", i), openFiles[i]);

        cfg->Flush();
    }

    void OnUndo(wxCommandEvent &event) { if (auto *t = CurrentText()) t->Undo(); }
    void OnRedo(wxCommandEvent &event) { if (auto *t = CurrentText()) t->Redo(); }
    void OnCut(wxCommandEvent &event) { if (auto *t = CurrentText()) t->Cut(); }
    void OnCopy(wxCommandEvent &event) { if (auto *t = CurrentText()) t->Copy(); }
    void OnPaste(wxCommandEvent &event) { if (auto *t = CurrentText()) t->Paste(); }
    void OnSelectAll(wxCommandEvent &event) { if (auto *t = CurrentText()) t->SelectAll(); }

    void OnPrint(wxCommandEvent &event)
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

    void OnAbout(wxCommandEvent &event)
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

    // Searches for `text` starting from the current selection, wrapping
    // around the document if not found before the end (or start).
    bool FindInEditor(wxStyledTextCtrl *stc, const wxString &text, int flags, bool forward)
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

    void ShowFindDialog(bool replace)
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

    void OnFindMenu(wxCommandEvent &event) { ShowFindDialog(false); }
    void OnReplaceMenu(wxCommandEvent &event) { ShowFindDialog(true); }
    void OnToggleWrapAround(wxCommandEvent &event) { m_wrapAround = event.IsChecked(); }

    void OnToggleWordWrap(wxCommandEvent &event)
    {
        int mode = event.IsChecked() ? wxSTC_WRAP_WORD : wxSTC_WRAP_NONE;
        for (size_t i = 0; i < m_notebook->GetPageCount(); i++)
            PageText((int)i)->SetWrapMode(mode);
    }

    // These resize the actual base font (persisted, whole-window) rather
    // than using wxStyledTextCtrl's own per-tab visual zoom, so the size a
    // person picks is what gets saved and restored next launch.
    void ChangeFontSize(int delta)
    {
        int newSize = g_fontSize + delta;
        if (newSize < kMinFontSize || newSize > kMaxFontSize) return;
        g_fontSize = newSize;
        ReapplyHighlightingToAllTabs();
    }

    void OnToggleFullScreen(wxCommandEvent &event)
    {
        // Keep the menu bar visible in full screen (unlike the default
        // wxFULLSCREEN_ALL, which hides it along with the toolbar/statusbar).
        ShowFullScreen(!IsFullScreen(), wxFULLSCREEN_NOTOOLBAR | wxFULLSCREEN_NOSTATUSBAR |
            wxFULLSCREEN_NOBORDER | wxFULLSCREEN_NOCAPTION);
    }
    void OnZoomIn(wxCommandEvent &event) { ChangeFontSize(+1); }
    void OnZoomOut(wxCommandEvent &event) { ChangeFontSize(-1); }

    void OnZoomReset(wxCommandEvent &event)
    {
        g_fontSize = kDefaultFontSize;
        ReapplyHighlightingToAllTabs();
    }

    // Scintilla natively zooms on Ctrl+scroll-wheel, but that's separate
    // per-tab state we don't persist. Intercept it here and redirect to the
    // same whole-window, saved font-size change the menu items use, instead
    // of letting the two zoom systems drift apart.
    void OnMouseWheel(wxMouseEvent &event)
    {
        if (event.ControlDown())
        {
            ChangeFontSize(event.GetWheelRotation() > 0 ? +1 : -1);
            return;
        }
        event.Skip();
    }

    wxString NotFoundMessage(const wxString &text)
    {
        return m_wrapAround
            ? "\"" + text + "\" not found."
            : "\"" + text + "\" not found (wrap around is off).";
    }

    void OnFindNext(wxCommandEvent &event)
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

    void OnFindDialogEvent(wxFindDialogEvent &event)
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
};

// ============================================================================
// SINGLE-INSTANCE / IPC
// A second launch of ClearText hands its file arguments to the already-running
// instance (as new tabs) instead of opening a second window.
// ============================================================================

// A local TCP port is used on Windows (no AF_UNIX support in older
// toolchains); on Linux/macOS a per-user Unix domain socket is used instead,
// which — like gedit's D-Bus based activation — is scoped to this session
// and user rather than a guessable, potentially-colliding network port.
#ifdef WIN32
static const wxString IPC_SERVICE = "47230";
#else
static const wxString IPC_SERVICE =
    wxString::Format("/tmp/cleartext-ipc-%s", wxGetUserId());
#endif
static const wxString IPC_TOPIC = "cleartext";

class ClearTextConnection : public wxConnection
{
public:
    explicit ClearTextConnection(ClearTextFrame *frame) : m_frame(frame) {}

    bool OnExec(const wxString &topic, const wxString &data) override
    {
        if (topic != IPC_TOPIC) return false;

        if (!data.IsEmpty())
        {
            m_frame->OpenFilePath(data);
            m_frame->CloseInitialBlankTabIfUnused();
        }

        if (m_frame->IsIconized()) m_frame->Iconize(false);
        m_frame->Raise();
        m_frame->RequestUserAttention();
        return true;
    }

private:
    ClearTextFrame *m_frame;
};

class ClearTextServer : public wxServer
{
public:
    explicit ClearTextServer(ClearTextFrame *frame) : m_frame(frame) {}

    wxConnectionBase *OnAcceptConnection(const wxString &topic) override
    {
        if (topic != IPC_TOPIC) return nullptr;
        return new ClearTextConnection(m_frame);
    }

private:
    ClearTextFrame *m_frame;
};

// Tries to hand `files` off to an already-running instance. Returns true if
// a running instance accepted the connection (whether or not `files` was
// empty — an empty list just raises the existing window).
static bool SendToRunningInstance(const wxArrayString &files)
{
    wxClient client;
    wxConnectionBase *conn = client.MakeConnection("localhost", IPC_SERVICE, IPC_TOPIC);
    if (!conn) return false;

    if (files.IsEmpty())
    {
        conn->Execute("");
    }
    else
    {
        for (const wxString &f : files)
        {
            wxFileName fn(f);
            fn.MakeAbsolute(); // resolve against our cwd, not the running instance's
            conn->Execute(fn.GetFullPath());
        }
    }

    conn->Disconnect();
    delete conn;
    return true;
}

class ClearTextApp : public wxApp
{
public:
    bool OnInit() override
    {
        m_instanceChecker = new wxSingleInstanceChecker(
            "ClearText-" + wxGetUserId());

        wxArrayString filesToOpen;
        for (int i = 1; i < argc; i++)
            filesToOpen.Add(argv[i]);

        if (m_instanceChecker->IsAnotherRunning())
        {
            if (SendToRunningInstance(filesToOpen))
            {
                delete m_instanceChecker;
                m_instanceChecker = nullptr;
                return false; // hand-off succeeded, don't open a new window
            }
            // Fall through and open our own window if IPC didn't work
            // (e.g. the other instance is stuck/unresponsive).
        }

        LoadConfig(); // sets g_themeIndex + m_lastSessionFiles before any tab is created

        ClearTextFrame *frame = new ClearTextFrame();

#ifndef WIN32
        if (wxFileExists(IPC_SERVICE))
        {
            wxLogNull noLog; // stale socket removal is best-effort, not worth a popup
            wxRemoveFile(IPC_SERVICE);
        }
#endif
        m_server = new ClearTextServer(frame);
        if (!m_server->Create(IPC_SERVICE))
        {
            delete m_server;
            m_server = nullptr; // non-fatal: this instance just won't receive hand-offs
        }

        // Files explicitly passed on the command line always win. Only when
        // there are none do we fall back to whatever was open at last exit.
        if (filesToOpen.IsEmpty())
            filesToOpen = m_lastSessionFiles;

        for (const wxString &f : filesToOpen)
            frame->OpenFilePath(f);
        if (!filesToOpen.IsEmpty())
            frame->CloseInitialBlankTabIfUnused();

        frame->Show();
        return true;
    }

    int OnExit() override
    {
        delete m_server;
        delete m_instanceChecker;
        delete wxConfigBase::Set(nullptr); // flush + free the config we installed in OnInit
        return wxApp::OnExit();
    }

private:
    wxSingleInstanceChecker *m_instanceChecker = nullptr;
    ClearTextServer *m_server = nullptr;
    wxArrayString m_lastSessionFiles;

    // Installs the on-disk config as the process-wide default (so
    // wxConfigBase::Get() works anywhere, e.g. ClearTextFrame::SaveSession),
    // then reads back the saved theme and last-session file list.
    void LoadConfig()
    {
        wxConfigBase::Set(new wxFileConfig("ClearText", wxEmptyString,
            GetConfigFilePath(), wxEmptyString, wxCONFIG_USE_LOCAL_FILE));
        wxConfigBase *cfg = wxConfigBase::Get();

        long savedTheme = 0;
        cfg->Read("Theme", &savedTheme, 0L);
        if (savedTheme >= 0 && savedTheme < (long)AllThemes().size())
            g_themeIndex = (int)savedTheme;

        long savedFontSize = kDefaultFontSize;
        cfg->Read("FontSize", &savedFontSize, (long)kDefaultFontSize);
        if (savedFontSize >= kMinFontSize && savedFontSize <= kMaxFontSize)
            g_fontSize = (int)savedFontSize;

        long count = 0;
        cfg->Read("LastSession/Count", &count, 0L);
        for (long i = 0; i < count; i++)
        {
            wxString path;
            if (cfg->Read(wxString::Format("LastSession/File%ld", i), &path) && !path.IsEmpty())
                m_lastSessionFiles.Add(path);
        }
    }
};

wxIMPLEMENT_APP(ClearTextApp);

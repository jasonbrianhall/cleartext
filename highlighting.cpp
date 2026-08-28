#include "highlighting.h"
#include "themes.h"
#include "custom_themes.h"
#include <wx/filename.h>

// ----------------------------------------------------------------------
// Theme + font size state
// ----------------------------------------------------------------------

// Index into AllThemes() for the currently active theme, applied to every
// tab (existing and new). A plain module-wide global rather than a frame
// member since the style-application functions below are free functions
// shared by every editor instance.
static int g_themeIndex = 0;

// Base editor font size in points, applied to every tab the same way the
// theme is. Adjusted via View > Increase/Decrease/Reset Font Size.
const int kMinFontSize = 6;
const int kMaxFontSize = 36;
const int kDefaultFontSize = 10;
static int g_fontSize = kDefaultFontSize;

int GetThemeIndex() { return g_themeIndex; }
void SetThemeIndex(int index) { g_themeIndex = index; }

int GetFontSize() { return g_fontSize; }
void SetFontSize(int size) { g_fontSize = size; }

static const EditorTheme &CurrentTheme()
{
    const std::vector<EditorTheme> &themes = CustomThemes::All();
    int idx = g_themeIndex;
    if (idx < 0 || idx >= (int)themes.size()) idx = 0; // config drift (e.g. a removed custom theme)
    return themes[idx];
}

// ----------------------------------------------------------------------
// Language <-> extension / display-name mapping
// ----------------------------------------------------------------------

const std::vector<LanguageInfo> &AllLanguages()
{
    static const std::vector<LanguageInfo> languages = {
        {Language::Auto, "Auto-Detect"},
        {Language::PlainText, "Plain Text"},
        {Language::Cpp, "C / C++"},
        {Language::Python, "Python"},
        {Language::Html, "HTML"},
        {Language::Xml, "XML"},
        {Language::Markdown, "Markdown"},
    };
    return languages;
}

wxString LanguageDisplayName(Language lang)
{
    for (const LanguageInfo &info : AllLanguages())
        if (info.id == lang) return info.label;
    return "Plain Text";
}

Language DetectLanguageFromExtension(const wxString &filePath)
{
    wxString ext = filePath.IsEmpty() ? "" : wxFileName(filePath).GetExt().Lower();

    if (ext == "c" || ext == "cpp" || ext == "cc" || ext == "cxx" ||
        ext == "h" || ext == "hpp" || ext == "hxx" ||
        ext == "java" || ext == "js" || ext == "cs")
        return Language::Cpp;
    if (ext == "py")
        return Language::Python;
    if (ext == "html" || ext == "htm")
        return Language::Html;
    if (ext == "xml")
        return Language::Xml;
    if (ext == "md" || ext == "markdown")
        return Language::Markdown;
    return Language::PlainText;
}

// ----------------------------------------------------------------------
// Per-language style application
// ----------------------------------------------------------------------

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
    // drifts from -- and stops matching -- the saved FontSize preference.
    stc->SetZoom(0);

    stc->SetCaretForeground(th.caret);
    stc->SetSelBackground(true, th.selectionBg);
    stc->StyleSetBackground(wxSTC_STYLE_LINENUMBER, th.marginBg);
    stc->StyleSetForeground(wxSTC_STYLE_LINENUMBER, th.marginFg);
    stc->SetWhitespaceBackground(true, th.background);
    stc->SetWhitespaceForeground(true, th.marginFg);

    // Current-line highlight: a subtle background band that follows the
    // caret, reusing the margin color so it reads as "highlighted" without
    // clashing with the selection color.
    stc->SetCaretLineVisible(true);
    stc->SetCaretLineBackground(th.marginBg);

    // Matched/unmatched brace highlighting (see ClearTextFrame::OnEditorUpdateUI,
    // which calls BraceHighlight/BraceBadLight as the caret moves).
    stc->StyleSetForeground(wxSTC_STYLE_BRACELIGHT, th.keyword2);
    stc->StyleSetBackground(wxSTC_STYLE_BRACELIGHT, th.selectionBg);
    stc->StyleSetBold(wxSTC_STYLE_BRACELIGHT, true);
    stc->StyleSetForeground(wxSTC_STYLE_BRACEBAD, wxColour(220, 60, 60));
    stc->StyleSetBold(wxSTC_STYLE_BRACEBAD, true);

    // Fold margin markers (see ClearTextFrame::SetupEditor for the margin
    // itself, and OnMarginClick for toggling). Plain +/- glyphs rather than
    // the boxed/connector style, so they don't need extra theme colors.
    stc->SetFoldMarginColour(true, th.marginBg);
    stc->SetFoldMarginHiColour(true, th.marginBg);
    stc->MarkerDefine(wxSTC_MARKNUM_FOLDEROPEN, wxSTC_MARK_MINUS, th.marginFg, th.marginBg);
    stc->MarkerDefine(wxSTC_MARKNUM_FOLDER, wxSTC_MARK_PLUS, th.marginFg, th.marginBg);
    stc->MarkerDefine(wxSTC_MARKNUM_FOLDERSUB, wxSTC_MARK_EMPTY, th.marginFg, th.marginBg);
    stc->MarkerDefine(wxSTC_MARKNUM_FOLDERTAIL, wxSTC_MARK_EMPTY, th.marginFg, th.marginBg);
    stc->MarkerDefine(wxSTC_MARKNUM_FOLDEREND, wxSTC_MARK_PLUS, th.marginFg, th.marginBg);
    stc->MarkerDefine(wxSTC_MARKNUM_FOLDEROPENMID, wxSTC_MARK_MINUS, th.marginFg, th.marginBg);
    stc->MarkerDefine(wxSTC_MARKNUM_FOLDERMIDTAIL, wxSTC_MARK_EMPTY, th.marginFg, th.marginBg);
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

void ApplyHighlighting(wxStyledTextCtrl *stc, Language lang)
{
    switch (lang)
    {
        case Language::Cpp:      ApplyCppStyles(stc); break;
        case Language::Python:   ApplyPythonStyles(stc); break;
        case Language::Html:     ApplyMarkupStyles(stc, false); break;
        case Language::Xml:      ApplyMarkupStyles(stc, true); break;
        case Language::Markdown: ApplyMarkdownStyles(stc); break;
        case Language::Auto: // shouldn't happen -- caller should have resolved it
        case Language::PlainText:
        default:
            ApplyPlainText(stc);
            break;
    }

    stc->Colourise(0, -1); // force a full re-lex now that styles/keywords changed
}

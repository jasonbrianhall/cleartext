#include "highlighting.h"
#include "themes.h"
#include "custom_themes.h"
#include <wx/filename.h>

// ----------------------------------------------------------------------
// Theme + font size state
// ----------------------------------------------------------------------

// Index into CustomThemes::All() for the currently active theme, applied to
// every tab (existing and new). A plain module-wide global rather than a
// frame member since the style-application functions below are free
// functions shared by every editor instance.
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
        {Language::CSharp, "C#"},
        {Language::Java, "Java"},
        {Language::JavaScript, "JavaScript"},
        {Language::TypeScript, "TypeScript"},
        {Language::Go, "Go"},
        {Language::Swift, "Swift"},
        {Language::ObjectiveC, "Objective-C"},
        {Language::ActionScript, "ActionScript"},

        {Language::Python, "Python"},
        {Language::Ruby, "Ruby"},
        {Language::Perl, "Perl"},
        {Language::Lua, "Lua"},
        {Language::Php, "PHP"},
        {Language::Bash, "Bash / Shell"},
        {Language::PowerShell, "PowerShell"},
        {Language::Batch, "Batch"},
        {Language::Tcl, "Tcl"},
        {Language::CoffeeScript, "CoffeeScript"},

        {Language::Html, "HTML"},
        {Language::Xml, "XML"},
        {Language::Css, "CSS"},
        {Language::Json, "JSON"},
        {Language::Yaml, "YAML"},
        {Language::Markdown, "Markdown"},
        {Language::Ini, "INI / Properties"},

        {Language::Rust, "Rust"},
        {Language::D, "D"},
        {Language::Pascal, "Pascal"},
        {Language::Fortran, "Fortran"},
        {Language::Ada, "Ada"},
        {Language::Assembly, "Assembly"},
        {Language::Verilog, "Verilog"},
        {Language::Vhdl, "VHDL"},

        {Language::R, "R"},
        {Language::Sql, "SQL"},

        {Language::Haskell, "Haskell"},
        {Language::Erlang, "Erlang"},

        {Language::Makefile, "Makefile"},
        {Language::CMake, "CMake"},
        {Language::Nsis, "NSIS"},
        {Language::InnoSetup, "Inno Setup"},

        {Language::LaTeX, "LaTeX"},
        {Language::Diff, "Diff / Patch"},
        {Language::VisualBasic, "Visual Basic"},
        {Language::VbScript, "VBScript"},
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
    if (filePath.IsEmpty()) return Language::PlainText;

    wxFileName fn(filePath);

    // Extension-less files that are still recognized by their exact name
    // (case-insensitive), e.g. "Makefile" or "CMakeLists.txt".
    wxString name = fn.GetFullName().Lower();
    if (name == "makefile" || name == "gnumakefile" || name == "makefile.am" || name == "makefile.in")
        return Language::Makefile;
    if (name == "cmakelists.txt")
        return Language::CMake;
    if (name == "dockerfile")
        return Language::Bash; // no dedicated Dockerfile lexer; shell-like syntax is a reasonable fit

    wxString ext = fn.GetExt().Lower();

    // C-family
    if (ext == "c" || ext == "cpp" || ext == "cc" || ext == "cxx" ||
        ext == "h" || ext == "hpp" || ext == "hh" || ext == "hxx" || ext == "inl" || ext == "ipp")
        return Language::Cpp;
    if (ext == "cs") return Language::CSharp;
    if (ext == "java") return Language::Java;
    if (ext == "js" || ext == "mjs" || ext == "cjs" || ext == "jsx") return Language::JavaScript;
    if (ext == "ts" || ext == "tsx") return Language::TypeScript;
    if (ext == "go") return Language::Go;
    if (ext == "swift") return Language::Swift;
    if (ext == "m" || ext == "mm") return Language::ObjectiveC;
    if (ext == "as") return Language::ActionScript;

    // Scripting
    if (ext == "py" || ext == "pyw") return Language::Python;
    if (ext == "rb" || ext == "erb") return Language::Ruby;
    if (ext == "pl" || ext == "pm") return Language::Perl;
    if (ext == "lua") return Language::Lua;
    if (ext == "php" || ext == "phtml" || ext == "php3" || ext == "php4" || ext == "php5") return Language::Php;
    if (ext == "sh" || ext == "bash" || ext == "zsh" || ext == "ksh") return Language::Bash;
    if (ext == "ps1" || ext == "psm1" || ext == "psd1") return Language::PowerShell;
    if (ext == "bat" || ext == "cmd") return Language::Batch;
    if (ext == "tcl" || ext == "tk") return Language::Tcl;
    if (ext == "coffee") return Language::CoffeeScript;

    // Web / structured data
    if (ext == "html" || ext == "htm" || ext == "xhtml" || ext == "vue" || ext == "svelte")
        return Language::Html;
    if (ext == "xml" || ext == "xsd" || ext == "xsl" || ext == "xslt" || ext == "svg" ||
        ext == "rss" || ext == "csproj" || ext == "vcxproj" || ext == "plist")
        return Language::Xml;
    if (ext == "css" || ext == "scss" || ext == "less") return Language::Css;
    if (ext == "json" || ext == "jsonc" || ext == "json5") return Language::Json;
    if (ext == "yaml" || ext == "yml") return Language::Yaml;
    if (ext == "md" || ext == "markdown") return Language::Markdown;
    if (ext == "ini" || ext == "cfg" || ext == "conf" || ext == "properties" || ext == "toml")
        return Language::Ini;

    // Systems / compiled
    if (ext == "rs") return Language::Rust;
    if (ext == "d") return Language::D;
    if (ext == "pas" || ext == "pp" || ext == "dpr") return Language::Pascal;
    if (ext == "f" || ext == "f77" || ext == "f90" || ext == "f95" || ext == "for") return Language::Fortran;
    if (ext == "ada" || ext == "adb" || ext == "ads") return Language::Ada;
    if (ext == "asm" || ext == "s" || ext == "nasm") return Language::Assembly;
    if (ext == "v" || ext == "vh") return Language::Verilog;
    if (ext == "vhd" || ext == "vhdl") return Language::Vhdl;

    // Data / math
    if (ext == "r" || ext == "rdata") return Language::R;
    if (ext == "sql") return Language::Sql;

    // Functional
    if (ext == "hs" || ext == "lhs") return Language::Haskell;
    if (ext == "erl" || ext == "hrl") return Language::Erlang;

    // Build / config / installer scripting
    if (ext == "cmake") return Language::CMake;
    if (ext == "nsi" || ext == "nsh") return Language::Nsis;
    if (ext == "iss") return Language::InnoSetup;

    // Documents / misc
    if (ext == "tex" || ext == "sty" || ext == "cls") return Language::LaTeX;
    if (ext == "diff" || ext == "patch") return Language::Diff;
    if (ext == "vb") return Language::VisualBasic;
    if (ext == "vbs") return Language::VbScript;

    return Language::PlainText;
}

// ----------------------------------------------------------------------
// Shared setup (theme colors, caret line, braces, folding) applied by
// every Apply*Styles function via SetCommonStyleDefaults.
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
    // drifts from — and stops matching — the saved FontSize preference.
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

// ----------------------------------------------------------------------
// C-family: C/C++, C#, Java, JavaScript, TypeScript, Go, Swift,
// Objective-C, ActionScript -- one lexer (wxSTC_LEX_CPP), one style set
// (wxSTC_C_*), different keyword lists per language.
// ----------------------------------------------------------------------

static void ApplyCFamilyStyles(wxStyledTextCtrl *stc, Language lang)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_CPP);
    SetCommonStyleDefaults(stc);

    wxString keywords;
    switch (lang)
    {
        case Language::CSharp:
            keywords =
                "abstract as base break case catch checked class const continue default delegate "
                "do else enum event explicit extern false finally fixed for foreach goto if implicit "
                "in interface internal is lock namespace new null object operator out override params "
                "private protected public readonly ref return sealed sizeof stackalloc static struct "
                "switch this throw true try typeof unchecked unsafe using virtual void volatile while "
                "async await var dynamic string bool byte sbyte char decimal double float int uint "
                "long ulong short ushort get set value partial yield";
            break;
        case Language::Java:
            keywords =
                "abstract assert boolean break byte case catch char class const continue default do "
                "double else enum extends final finally float for goto if implements import instanceof "
                "int interface long native new package private protected public return short static "
                "strictfp super switch synchronized this throw throws transient try void volatile while "
                "true false null var record sealed permits yield";
            break;
        case Language::JavaScript:
            keywords =
                "break case catch class const continue debugger default delete do else export extends "
                "finally for function if import in instanceof let new return super switch this throw "
                "try typeof var void while with yield async await static get set of "
                "true false null undefined NaN Infinity";
            break;
        case Language::TypeScript:
            keywords =
                "break case catch class const continue debugger default delete do else export extends "
                "finally for function if import in instanceof let new return super switch this throw "
                "try typeof var void while with yield async await static get set of "
                "interface type enum implements namespace declare public private protected readonly "
                "abstract as any boolean is keyof infer never string number symbol unknown "
                "true false null undefined";
            break;
        case Language::Go:
            keywords =
                "break case chan const continue default defer else fallthrough for func go goto if "
                "import interface map package range return select struct switch type var "
                "bool byte complex64 complex128 error float32 float64 int int8 int16 int32 int64 rune "
                "string uint uint8 uint16 uint32 uint64 uintptr true false nil iota make new len cap "
                "append copy delete panic recover print println";
            break;
        case Language::Swift:
            keywords =
                "associatedtype class deinit enum extension fileprivate func import init inout "
                "internal let open operator private protocol public rethrows static struct subscript "
                "typealias var break case continue default defer do else fallthrough for guard if in "
                "repeat return switch where while as Any catch false is nil rethrows super self Self "
                "throw throws true try #available #colorLiteral #column #else #elseif #endif #error "
                "#file #function #if #imageLiteral #line #selector #sourceLocation #warning "
                "associativity convenience dynamic didSet final get infix indirect lazy left mutating "
                "none nonmutating optional override postfix precedence prefix Protocol required right "
                "set some Type unowned weak willSet";
            break;
        case Language::ObjectiveC:
            keywords =
                "if else for while do switch case default break continue return goto typedef struct "
                "union enum sizeof static extern const volatile signed unsigned void char short int "
                "long float double id Class SEL IMP BOOL YES NO nil Nil self super "
                "@interface @implementation @end @protocol @property @synthesize @dynamic @class "
                "@selector @encode @synchronized @try @catch @finally @throw @autoreleasepool "
                "@import strong weak assign copy nonatomic atomic readonly readwrite nullable nonnull "
                "instancetype in out inout bycopy byref oneway";
            break;
        case Language::ActionScript:
            keywords =
                "break case catch class continue default delete do else extends false finally for "
                "function if implements import in instanceof interface new null package private public "
                "protected return super switch this throw true try typeof var void while with "
                "class dynamic final internal native override static";
            break;
        case Language::Cpp:
        default:
            keywords =
                "if else for while do return break continue switch case default "
                "class struct public private protected namespace using template "
                "typename const static virtual override new delete nullptr enum "
                "union sizeof this throw try catch friend inline operator explicit "
                "extern auto volatile unsigned signed short long void int float "
                "double char bool true false import export function var let";
            break;
    }
    stc->SetKeyWords(0, keywords);

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

// ----------------------------------------------------------------------
// Python
// ----------------------------------------------------------------------

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

// ----------------------------------------------------------------------
// HTML / XML, and PHP (HTML lexer with embedded-PHP styles layered on)
// ----------------------------------------------------------------------

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

// PHP files are lexed as HTML with PHP recognized inline (the same way
// Scintilla/Notepad++ handle them) -- <?php ... ?> blocks pick up their own
// style IDs (wxSTC_HPHP_*) layered on top of the base HTML styling.
static void ApplyPhpStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    ApplyMarkupStyles(stc, false);

    stc->StyleSetForeground(wxSTC_HPHP_DEFAULT, th.foreground);
    stc->StyleSetForeground(wxSTC_HPHP_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_HPHP_COMMENTLINE, th.comment);
    stc->StyleSetForeground(wxSTC_HPHP_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_HPHP_SIMPLESTRING, th.string);
    stc->StyleSetForeground(wxSTC_HPHP_HSTRING, th.string);
    stc->StyleSetForeground(wxSTC_HPHP_HSTRING_VARIABLE, th.attribute);
    stc->StyleSetForeground(wxSTC_HPHP_VARIABLE, th.attribute);
    stc->StyleSetForeground(wxSTC_HPHP_COMPLEX_VARIABLE, th.attribute);
    stc->StyleSetForeground(wxSTC_HPHP_WORD, th.keyword);
    stc->StyleSetBold(wxSTC_HPHP_WORD, true);
    stc->StyleSetForeground(wxSTC_HPHP_OPERATOR, th.operatorColor);
}

// ----------------------------------------------------------------------
// Markdown
// ----------------------------------------------------------------------

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

// ----------------------------------------------------------------------
// CSS
// ----------------------------------------------------------------------

static void ApplyCssStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_CSS);
    SetCommonStyleDefaults(stc);

    stc->StyleSetForeground(wxSTC_CSS_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_CSS_TAG, th.tag);
    stc->StyleSetForeground(wxSTC_CSS_CLASS, th.keyword2);
    stc->StyleSetForeground(wxSTC_CSS_ID, th.keyword2);
    stc->StyleSetForeground(wxSTC_CSS_PSEUDOCLASS, th.keyword2);
    stc->StyleSetForeground(wxSTC_CSS_PSEUDOELEMENT, th.keyword2);
    stc->StyleSetForeground(wxSTC_CSS_UNKNOWN_PSEUDOCLASS, th.keyword2);
    stc->StyleSetForeground(wxSTC_CSS_EXTENDED_PSEUDOCLASS, th.keyword2);
    stc->StyleSetForeground(wxSTC_CSS_EXTENDED_PSEUDOELEMENT, th.keyword2);
    stc->StyleSetForeground(wxSTC_CSS_IDENTIFIER, th.keyword);
    stc->StyleSetForeground(wxSTC_CSS_IDENTIFIER2, th.keyword);
    stc->StyleSetForeground(wxSTC_CSS_IDENTIFIER3, th.keyword);
    stc->StyleSetForeground(wxSTC_CSS_UNKNOWN_IDENTIFIER, th.keyword);
    stc->StyleSetForeground(wxSTC_CSS_EXTENDED_IDENTIFIER, th.keyword);
    stc->StyleSetForeground(wxSTC_CSS_VALUE, th.string);
    stc->StyleSetForeground(wxSTC_CSS_DOUBLESTRING, th.string);
    stc->StyleSetForeground(wxSTC_CSS_SINGLESTRING, th.string);
    stc->StyleSetForeground(wxSTC_CSS_VARIABLE, th.attribute);
    stc->StyleSetForeground(wxSTC_CSS_OPERATOR, th.operatorColor);
    stc->StyleSetForeground(wxSTC_CSS_IMPORTANT, th.preprocessor);
    stc->StyleSetBold(wxSTC_CSS_IMPORTANT, true);
    stc->StyleSetForeground(wxSTC_CSS_DIRECTIVE, th.preprocessor);
#ifdef wxSTC_CSS_MEDIA
    stc->StyleSetForeground(wxSTC_CSS_MEDIA, th.preprocessor);
#endif
}

// ----------------------------------------------------------------------
// JSON
// ----------------------------------------------------------------------

static void ApplyJsonStyles(wxStyledTextCtrl *stc)
{
#ifdef wxSTC_LEX_JSON
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_JSON);
    SetCommonStyleDefaults(stc);

    stc->StyleSetForeground(wxSTC_JSON_LINECOMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_JSON_BLOCKCOMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_JSON_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_JSON_STRING, th.string);
    stc->StyleSetForeground(wxSTC_JSON_STRINGEOL, th.string);
    stc->StyleSetForeground(wxSTC_JSON_PROPERTYNAME, th.attribute);
    stc->StyleSetForeground(wxSTC_JSON_ESCAPESEQUENCE, th.preprocessor);
    stc->StyleSetForeground(wxSTC_JSON_KEYWORD, th.keyword);
    stc->StyleSetBold(wxSTC_JSON_KEYWORD, true);
    stc->StyleSetForeground(wxSTC_JSON_LDKEYWORD, th.keyword2);
    stc->StyleSetForeground(wxSTC_JSON_OPERATOR, th.operatorColor);
    stc->StyleSetForeground(wxSTC_JSON_URI, th.string);
    stc->StyleSetForeground(wxSTC_JSON_COMPACTIRI, th.string);
    stc->StyleSetForeground(wxSTC_JSON_ERROR, wxColour(220, 60, 60));
#else
    // JSON lexer unavailable in this wxWidgets build (added in 3.1.0) --
    // fall back to plain text rather than fail to compile.
    ApplyPlainText(stc);
#endif
}

// ----------------------------------------------------------------------
// YAML
// ----------------------------------------------------------------------

static void ApplyYamlStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_YAML);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0, "true false null yes no on off");

    stc->StyleSetForeground(wxSTC_YAML_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_YAML_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_YAML_IDENTIFIER, th.attribute);
    stc->StyleSetForeground(wxSTC_YAML_KEYWORD, th.keyword);
    stc->StyleSetBold(wxSTC_YAML_KEYWORD, true);
    stc->StyleSetForeground(wxSTC_YAML_REFERENCE, th.keyword2);
    stc->StyleSetForeground(wxSTC_YAML_DOCUMENT, th.preprocessor);
    stc->StyleSetForeground(wxSTC_YAML_OPERATOR, th.operatorColor);
    stc->StyleSetForeground(wxSTC_YAML_TEXT, th.string);
    stc->StyleSetForeground(wxSTC_YAML_ERROR, wxColour(220, 60, 60));
}

// ----------------------------------------------------------------------
// INI / properties files
// ----------------------------------------------------------------------

static void ApplyIniStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_PROPERTIES);
    SetCommonStyleDefaults(stc);

    stc->StyleSetForeground(wxSTC_PROPS_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_PROPS_SECTION, th.tag);
    stc->StyleSetBold(wxSTC_PROPS_SECTION, true);
    stc->StyleSetForeground(wxSTC_PROPS_KEY, th.keyword);
    stc->StyleSetForeground(wxSTC_PROPS_ASSIGNMENT, th.operatorColor);
    stc->StyleSetForeground(wxSTC_PROPS_DEFVAL, th.string);
}

// ----------------------------------------------------------------------
// Bash / shell
// ----------------------------------------------------------------------

static void ApplyBashStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_BASH);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "if then else elif fi for while until do done case esac function select "
        "in return break continue exit export local readonly set unset shift "
        "echo printf read cd pwd source alias unalias trap eval exec test");

    stc->StyleSetForeground(wxSTC_SH_COMMENTLINE, th.comment);
    stc->StyleSetForeground(wxSTC_SH_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_SH_STRING, th.string);
    stc->StyleSetForeground(wxSTC_SH_CHARACTER, th.string);
    stc->StyleSetForeground(wxSTC_SH_BACKTICKS, th.string);
    stc->StyleSetForeground(wxSTC_SH_HERE_DELIM, th.preprocessor);
    stc->StyleSetForeground(wxSTC_SH_HERE_Q, th.string);
    stc->StyleSetForeground(wxSTC_SH_WORD, th.keyword);
    stc->StyleSetBold(wxSTC_SH_WORD, true);
    stc->StyleSetForeground(wxSTC_SH_SCALAR, th.attribute);
    stc->StyleSetForeground(wxSTC_SH_PARAM, th.attribute);
    stc->StyleSetForeground(wxSTC_SH_IDENTIFIER, th.foreground);
    stc->StyleSetForeground(wxSTC_SH_OPERATOR, th.operatorColor);
    stc->StyleSetForeground(wxSTC_SH_ERROR, wxColour(220, 60, 60));
}

// ----------------------------------------------------------------------
// PowerShell
// ----------------------------------------------------------------------

static void ApplyPowerShellStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_POWERSHELL);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "begin break catch class continue data define do dynamicparam else elseif end enum exit "
        "filter finally for foreach from function hidden if in inlinescript param process return "
        "static switch throw trap try until using var while workflow "
        "true false null $null $true $false");
    stc->SetKeyWords(1,
        "get-childitem set-location get-location get-content set-content get-process "
        "stop-process write-output write-host write-error write-warning "
        "new-object select-object where-object foreach-object");

    stc->StyleSetForeground(wxSTC_POWERSHELL_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_POWERSHELL_COMMENTSTREAM, th.comment);
    stc->StyleSetForeground(wxSTC_POWERSHELL_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_POWERSHELL_STRING, th.string);
    stc->StyleSetForeground(wxSTC_POWERSHELL_CHARACTER, th.string);
#ifdef wxSTC_POWERSHELL_HERE_STRING
    stc->StyleSetForeground(wxSTC_POWERSHELL_HERE_STRING, th.string);
#endif
#ifdef wxSTC_POWERSHELL_HERE_CHARACTER
    stc->StyleSetForeground(wxSTC_POWERSHELL_HERE_CHARACTER, th.string);
#endif
    stc->StyleSetForeground(wxSTC_POWERSHELL_VARIABLE, th.attribute);
    stc->StyleSetForeground(wxSTC_POWERSHELL_KEYWORD, th.keyword);
    stc->StyleSetBold(wxSTC_POWERSHELL_KEYWORD, true);
    stc->StyleSetForeground(wxSTC_POWERSHELL_CMDLET, th.keyword2);
    stc->StyleSetForeground(wxSTC_POWERSHELL_ALIAS, th.keyword2);
    stc->StyleSetForeground(wxSTC_POWERSHELL_FUNCTION, th.keyword2);
    stc->StyleSetForeground(wxSTC_POWERSHELL_OPERATOR, th.operatorColor);
}

// ----------------------------------------------------------------------
// Batch
// ----------------------------------------------------------------------

static void ApplyBatchStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_BATCH);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "if else for goto call exit setlocal endlocal shift break rem "
        "echo pause cls set pushd popd start title cd cls copy del dir "
        "md rd ren type ver vol");

    stc->StyleSetForeground(wxSTC_BAT_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_BAT_WORD, th.keyword);
    stc->StyleSetBold(wxSTC_BAT_WORD, true);
    stc->StyleSetForeground(wxSTC_BAT_COMMAND, th.keyword2);
    stc->StyleSetForeground(wxSTC_BAT_LABEL, th.tag);
    stc->StyleSetForeground(wxSTC_BAT_IDENTIFIER, th.attribute);
    stc->StyleSetForeground(wxSTC_BAT_OPERATOR, th.operatorColor);
}

// ----------------------------------------------------------------------
// Tcl
// ----------------------------------------------------------------------

static void ApplyTclStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_TCL);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "after append array break catch cd close concat continue eof error eval exec exit expr "
        "file flush for foreach format gets glob global history if incr info interp join lappend "
        "lindex linsert list llength lrange lreplace lsearch lsort namespace open pid proc puts "
        "pwd read regexp regsub rename return scan seek set source split string subst switch tell "
        "time trace unknown unset uplevel upvar variable vwait while");

    stc->StyleSetForeground(wxSTC_TCL_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_TCL_COMMENTLINE, th.comment);
    stc->StyleSetForeground(wxSTC_TCL_COMMENT_BOX, th.comment);
    stc->StyleSetForeground(wxSTC_TCL_BLOCK_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_TCL_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_TCL_IN_QUOTE, th.string);
    stc->StyleSetForeground(wxSTC_TCL_WORD_IN_QUOTE, th.string);
    stc->StyleSetForeground(wxSTC_TCL_WORD, th.keyword);
    stc->StyleSetBold(wxSTC_TCL_WORD, true);
    stc->StyleSetForeground(wxSTC_TCL_IDENTIFIER, th.foreground);
    stc->StyleSetForeground(wxSTC_TCL_SUBSTITUTION, th.attribute);
    stc->StyleSetForeground(wxSTC_TCL_MODIFIER, th.attribute);
    stc->StyleSetForeground(wxSTC_TCL_OPERATOR, th.operatorColor);
}

// ----------------------------------------------------------------------
// CoffeeScript
// ----------------------------------------------------------------------

static void ApplyCoffeeScriptStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_COFFEESCRIPT);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "and break by catch class continue delete do else extends false finally for if in "
        "instanceof is isnt loop new no not of off on or return switch then this throw true try "
        "typeof unless until when while yes super");

    stc->StyleSetForeground(wxSTC_COFFEESCRIPT_COMMENTLINE, th.comment);
    stc->StyleSetForeground(wxSTC_COFFEESCRIPT_COMMENTBLOCK, th.comment);
    stc->StyleSetForeground(wxSTC_COFFEESCRIPT_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_COFFEESCRIPT_STRING, th.string);
    stc->StyleSetForeground(wxSTC_COFFEESCRIPT_CHARACTER, th.string);
    stc->StyleSetForeground(wxSTC_COFFEESCRIPT_VERBATIM, th.string);
    stc->StyleSetForeground(wxSTC_COFFEESCRIPT_REGEX, th.string);
    stc->StyleSetForeground(wxSTC_COFFEESCRIPT_WORD, th.keyword);
    stc->StyleSetBold(wxSTC_COFFEESCRIPT_WORD, true);
    stc->StyleSetForeground(wxSTC_COFFEESCRIPT_WORD2, th.keyword2);
    stc->StyleSetForeground(wxSTC_COFFEESCRIPT_GLOBALCLASS, th.keyword2);
#ifdef wxSTC_COFFEESCRIPT_INSTANCEPROPERTY
    stc->StyleSetForeground(wxSTC_COFFEESCRIPT_INSTANCEPROPERTY, th.attribute);
#endif
    stc->StyleSetForeground(wxSTC_COFFEESCRIPT_OPERATOR, th.operatorColor);
}

// ----------------------------------------------------------------------
// Ruby
// ----------------------------------------------------------------------

static void ApplyRubyStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_RUBY);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "alias and begin break case class def defined? do else elsif end ensure false for if in "
        "module next nil not or redo rescue retry return self super then true undef unless until "
        "when while yield __FILE__ __LINE__");

    stc->StyleSetForeground(wxSTC_RB_COMMENTLINE, th.comment);
    stc->StyleSetForeground(wxSTC_RB_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_RB_STRING, th.string);
    stc->StyleSetForeground(wxSTC_RB_CHARACTER, th.string);
    stc->StyleSetForeground(wxSTC_RB_STRING_Q, th.string);
    stc->StyleSetForeground(wxSTC_RB_STRING_QQ, th.string);
    stc->StyleSetForeground(wxSTC_RB_STRING_QR, th.string);
    stc->StyleSetForeground(wxSTC_RB_STRING_QW, th.string);
    stc->StyleSetForeground(wxSTC_RB_STRING_QX, th.string);
    stc->StyleSetForeground(wxSTC_RB_BACKTICKS, th.string);
    stc->StyleSetForeground(wxSTC_RB_REGEX, th.string);
    stc->StyleSetForeground(wxSTC_RB_SYMBOL, th.attribute);
    stc->StyleSetForeground(wxSTC_RB_GLOBAL, th.attribute);
    stc->StyleSetForeground(wxSTC_RB_INSTANCE_VAR, th.attribute);
    stc->StyleSetForeground(wxSTC_RB_CLASS_VAR, th.attribute);
    stc->StyleSetForeground(wxSTC_RB_WORD, th.keyword);
    stc->StyleSetBold(wxSTC_RB_WORD, true);
    stc->StyleSetForeground(wxSTC_RB_CLASSNAME, th.keyword2);
    stc->StyleSetBold(wxSTC_RB_CLASSNAME, true);
    stc->StyleSetForeground(wxSTC_RB_DEFNAME, th.keyword2);
    stc->StyleSetForeground(wxSTC_RB_MODULE_NAME, th.keyword2);
    stc->StyleSetForeground(wxSTC_RB_OPERATOR, th.operatorColor);
    stc->StyleSetForeground(wxSTC_RB_ERROR, wxColour(220, 60, 60));
}

// ----------------------------------------------------------------------
// Perl
// ----------------------------------------------------------------------

static void ApplyPerlStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_PERL);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "my our local sub if elsif else unless while until for foreach do return last next redo "
        "package use no require and or not xor eq ne lt gt le ge cmp defined undef bless ref wantarray "
        "print printf sort map grep split join push pop shift unshift splice keys values each exists "
        "delete die warn eval");

    stc->StyleSetForeground(wxSTC_PL_COMMENTLINE, th.comment);
    stc->StyleSetForeground(wxSTC_PL_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_PL_STRING, th.string);
    stc->StyleSetForeground(wxSTC_PL_CHARACTER, th.string);
    stc->StyleSetForeground(wxSTC_PL_STRING_Q, th.string);
    stc->StyleSetForeground(wxSTC_PL_STRING_QQ, th.string);
    stc->StyleSetForeground(wxSTC_PL_STRING_QW, th.string);
    stc->StyleSetForeground(wxSTC_PL_STRING_QX, th.string);
    stc->StyleSetForeground(wxSTC_PL_BACKTICKS, th.string);
    stc->StyleSetForeground(wxSTC_PL_HERE_QQ, th.string);
    stc->StyleSetForeground(wxSTC_PL_HERE_QX, th.string);
    stc->StyleSetForeground(wxSTC_PL_HERE_Q, th.string);
    stc->StyleSetForeground(wxSTC_PL_REGEX, th.string);
    stc->StyleSetForeground(wxSTC_PL_REGSUBST, th.string);
    stc->StyleSetForeground(wxSTC_PL_SCALAR, th.attribute);
    stc->StyleSetForeground(wxSTC_PL_ARRAY, th.attribute);
    stc->StyleSetForeground(wxSTC_PL_HASH, th.attribute);
    stc->StyleSetForeground(wxSTC_PL_SYMBOLTABLE, th.attribute);
    stc->StyleSetForeground(wxSTC_PL_VARIABLE_INDEXER, th.attribute);
    stc->StyleSetForeground(wxSTC_PL_WORD, th.keyword);
    stc->StyleSetBold(wxSTC_PL_WORD, true);
    stc->StyleSetForeground(wxSTC_PL_PREPROCESSOR, th.preprocessor);
    stc->StyleSetForeground(wxSTC_PL_OPERATOR, th.operatorColor);
    stc->StyleSetForeground(wxSTC_PL_ERROR, wxColour(220, 60, 60));
}

// ----------------------------------------------------------------------
// Lua
// ----------------------------------------------------------------------

static void ApplyLuaStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_LUA);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "and break do else elseif end false for function goto if in local nil not or repeat return "
        "then true until while");
    stc->SetKeyWords(1,
        "assert collectgarbage dofile error _G getmetatable ipairs load loadfile next pairs pcall "
        "print rawequal rawget rawlen rawset select setmetatable tonumber tostring type _VERSION "
        "xpcall string table math io os coroutine");

    stc->StyleSetForeground(wxSTC_LUA_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_LUA_COMMENTLINE, th.comment);
    stc->StyleSetForeground(wxSTC_LUA_COMMENTDOC, th.comment);
    stc->StyleSetForeground(wxSTC_LUA_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_LUA_STRING, th.string);
    stc->StyleSetForeground(wxSTC_LUA_CHARACTER, th.string);
    stc->StyleSetForeground(wxSTC_LUA_LITERALSTRING, th.string);
    stc->StyleSetForeground(wxSTC_LUA_PREPROCESSOR, th.preprocessor);
    stc->StyleSetForeground(wxSTC_LUA_WORD, th.keyword);
    stc->StyleSetBold(wxSTC_LUA_WORD, true);
    stc->StyleSetForeground(wxSTC_LUA_WORD2, th.keyword2);
    stc->StyleSetForeground(wxSTC_LUA_LABEL, th.attribute);
    stc->StyleSetForeground(wxSTC_LUA_OPERATOR, th.operatorColor);
    stc->StyleSetForeground(wxSTC_LUA_STRINGEOL, wxColour(220, 60, 60));
}

// ----------------------------------------------------------------------
// Rust
// ----------------------------------------------------------------------

static void ApplyRustStyles(wxStyledTextCtrl *stc)
{
#ifdef wxSTC_LEX_RUST
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_RUST);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "as break const continue crate dyn else enum extern false fn for if impl in let loop match "
        "mod move mut pub ref return self Self static struct super trait true type unsafe use where "
        "while async await dyn union try");
    stc->SetKeyWords(1,
        "bool char str u8 u16 u32 u64 u128 usize i8 i16 i32 i64 i128 isize f32 f64 "
        "Vec String Option Some None Result Ok Err Box");

    stc->StyleSetForeground(wxSTC_RUST_COMMENTLINE, th.comment);
    stc->StyleSetForeground(wxSTC_RUST_COMMENTLINEDOC, th.comment);
    stc->StyleSetForeground(wxSTC_RUST_COMMENTBLOCK, th.comment);
    stc->StyleSetForeground(wxSTC_RUST_COMMENTBLOCKDOC, th.comment);
    stc->StyleSetForeground(wxSTC_RUST_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_RUST_STRING, th.string);
    stc->StyleSetForeground(wxSTC_RUST_STRINGR, th.string);
    stc->StyleSetForeground(wxSTC_RUST_CHARACTER, th.string);
    stc->StyleSetForeground(wxSTC_RUST_BYTESTRING, th.string);
    stc->StyleSetForeground(wxSTC_RUST_BYTESTRINGR, th.string);
    stc->StyleSetForeground(wxSTC_RUST_BYTECHARACTER, th.string);
    stc->StyleSetForeground(wxSTC_RUST_WORD, th.keyword);
    stc->StyleSetBold(wxSTC_RUST_WORD, true);
    stc->StyleSetForeground(wxSTC_RUST_WORD2, th.keyword2);
    stc->StyleSetForeground(wxSTC_RUST_MACRO, th.preprocessor);
    stc->StyleSetForeground(wxSTC_RUST_LIFETIME, th.attribute);
    stc->StyleSetForeground(wxSTC_RUST_OPERATOR, th.operatorColor);
    stc->StyleSetForeground(wxSTC_RUST_LEXERROR, wxColour(220, 60, 60));
#else
    // Rust lexer unavailable in this wxWidgets build (added in 3.1.0) --
    // fall back to plain text rather than fail to compile.
    ApplyPlainText(stc);
#endif
}

// ----------------------------------------------------------------------
// D
// ----------------------------------------------------------------------

static void ApplyDStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_D);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "abstract alias align asm assert auto body break case cast catch class const continue "
        "debug default delete deprecated do else enum export extern false final finally for "
        "foreach foreach_reverse function goto if immutable import in inout interface invariant "
        "is lazy module new nothrow null out override package pragma private protected public "
        "pure ref return scope shared static struct super switch synchronized template this throw "
        "true try typedef typeid typeof union unittest version void while with");
    stc->SetKeyWords(1,
        "bool byte cdouble cent cfloat char creal dchar double float idouble ifloat int ireal long "
        "real short ubyte ucent uint ulong ushort wchar size_t ptrdiff_t");

    stc->StyleSetForeground(wxSTC_D_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_D_COMMENTLINE, th.comment);
    stc->StyleSetForeground(wxSTC_D_COMMENTDOC, th.comment);
    stc->StyleSetForeground(wxSTC_D_COMMENTNESTED, th.comment);
    stc->StyleSetForeground(wxSTC_D_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_D_STRING, th.string);
    stc->StyleSetForeground(wxSTC_D_STRINGB, th.string);
    stc->StyleSetForeground(wxSTC_D_STRINGR, th.string);
    stc->StyleSetForeground(wxSTC_D_CHARACTER, th.string);
    stc->StyleSetForeground(wxSTC_D_WORD, th.keyword);
    stc->StyleSetBold(wxSTC_D_WORD, true);
    stc->StyleSetForeground(wxSTC_D_WORD2, th.keyword2);
    stc->StyleSetForeground(wxSTC_D_TYPEDEF, th.keyword2);
    stc->StyleSetForeground(wxSTC_D_OPERATOR, th.operatorColor);
    stc->StyleSetForeground(wxSTC_D_STRINGEOL, wxColour(220, 60, 60));
}

// ----------------------------------------------------------------------
// Pascal / Delphi
// ----------------------------------------------------------------------

static void ApplyPascalStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_PASCAL);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "and array asm begin case const constructor destructor div do downto else end file for "
        "function goto if implementation in inherited inline interface label mod nil not object "
        "of or packed procedure program record repeat set shl shr string then to type unit until "
        "uses var while with class private public protected published property");

    stc->StyleSetForeground(wxSTC_PAS_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_PAS_COMMENT2, th.comment);
    stc->StyleSetForeground(wxSTC_PAS_COMMENTLINE, th.comment);
    stc->StyleSetForeground(wxSTC_PAS_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_PAS_HEXNUMBER, th.number);
    stc->StyleSetForeground(wxSTC_PAS_STRING, th.string);
    stc->StyleSetForeground(wxSTC_PAS_CHARACTER, th.string);
    stc->StyleSetForeground(wxSTC_PAS_WORD, th.keyword);
    stc->StyleSetBold(wxSTC_PAS_WORD, true);
    stc->StyleSetForeground(wxSTC_PAS_PREPROCESSOR, th.preprocessor);
    stc->StyleSetForeground(wxSTC_PAS_PREPROCESSOR2, th.preprocessor);
    stc->StyleSetForeground(wxSTC_PAS_ASM, th.keyword2);
    stc->StyleSetForeground(wxSTC_PAS_OPERATOR, th.operatorColor);
    stc->StyleSetForeground(wxSTC_PAS_STRINGEOL, wxColour(220, 60, 60));
}

// ----------------------------------------------------------------------
// Fortran
// ----------------------------------------------------------------------

static void ApplyFortranStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_FORTRAN);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "program end subroutine function module use implicit none integer real double precision "
        "character logical complex dimension parameter common equivalence data allocate deallocate "
        "if then else elseif endif do while continue stop return call go to select case default "
        "type kind intent optional public private contains interface");
    stc->SetKeyWords(1,
        "abs sqrt sin cos tan exp log min max mod sum size len trim allocated present");

    stc->StyleSetForeground(wxSTC_F_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_F_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_F_STRING1, th.string);
    stc->StyleSetForeground(wxSTC_F_STRING2, th.string);
    stc->StyleSetForeground(wxSTC_F_WORD, th.keyword);
    stc->StyleSetBold(wxSTC_F_WORD, true);
    stc->StyleSetForeground(wxSTC_F_WORD2, th.keyword2);
    stc->StyleSetForeground(wxSTC_F_WORD3, th.preprocessor);
    stc->StyleSetForeground(wxSTC_F_LABEL, th.attribute);
    stc->StyleSetForeground(wxSTC_F_OPERATOR, th.operatorColor);
    stc->StyleSetForeground(wxSTC_F_OPERATOR2, th.operatorColor);
    stc->StyleSetForeground(wxSTC_F_STRINGEOL, wxColour(220, 60, 60));
}

// ----------------------------------------------------------------------
// Ada
// ----------------------------------------------------------------------

static void ApplyAdaStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_ADA);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "abort abs abstract accept access aliased all and array at begin body case constant "
        "declare delay delta digits do else elsif end entry exception exit for function generic "
        "goto if in interface is limited loop mod new not null of or others out overriding "
        "package pragma private procedure protected raise range record rem renames requeue "
        "return reverse select separate subtype synchronized tagged task terminate then type "
        "until use when while with xor");

    stc->StyleSetForeground(wxSTC_ADA_COMMENTLINE, th.comment);
    stc->StyleSetForeground(wxSTC_ADA_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_ADA_STRING, th.string);
    stc->StyleSetForeground(wxSTC_ADA_CHARACTER, th.string);
    stc->StyleSetForeground(wxSTC_ADA_WORD, th.keyword);
    stc->StyleSetBold(wxSTC_ADA_WORD, true);
    stc->StyleSetForeground(wxSTC_ADA_IDENTIFIER, th.foreground);
    stc->StyleSetForeground(wxSTC_ADA_LABEL, th.attribute);
    stc->StyleSetForeground(wxSTC_ADA_DELIMITER, th.operatorColor);
    stc->StyleSetForeground(wxSTC_ADA_ILLEGAL, wxColour(220, 60, 60));
    stc->StyleSetForeground(wxSTC_ADA_STRINGEOL, wxColour(220, 60, 60));
    stc->StyleSetForeground(wxSTC_ADA_CHARACTEREOL, wxColour(220, 60, 60));
}

// ----------------------------------------------------------------------
// Assembly
// ----------------------------------------------------------------------

static void ApplyAsmStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_ASM);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "mov add sub mul div inc dec push pop call ret jmp je jne jg jl jge jle jz jnz cmp test "
        "and or xor not shl shr lea nop int enter leave loop rep movs lods stos cmps scas");
    stc->SetKeyWords(1,
        "eax ebx ecx edx esi edi ebp esp ax bx cx dx si di bp sp al bl cl dl ah bh ch dh "
        "rax rbx rcx rdx rsi rdi rbp rsp r8 r9 r10 r11 r12 r13 r14 r15");
    stc->SetKeyWords(2,
        "section segment global extern db dw dd dq resb resw resd equ align org bits");

    stc->StyleSetForeground(wxSTC_ASM_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_ASM_COMMENTBLOCK, th.comment);
    stc->StyleSetForeground(wxSTC_ASM_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_ASM_STRING, th.string);
    stc->StyleSetForeground(wxSTC_ASM_CHARACTER, th.string);
    stc->StyleSetForeground(wxSTC_ASM_CPUINSTRUCTION, th.keyword);
    stc->StyleSetBold(wxSTC_ASM_CPUINSTRUCTION, true);
    stc->StyleSetForeground(wxSTC_ASM_REGISTER, th.keyword2);
    stc->StyleSetForeground(wxSTC_ASM_MATHINSTRUCTION, th.keyword2);
    stc->StyleSetForeground(wxSTC_ASM_DIRECTIVE, th.preprocessor);
    stc->StyleSetForeground(wxSTC_ASM_DIRECTIVEOPERAND, th.preprocessor);
    stc->StyleSetForeground(wxSTC_ASM_EXTINSTRUCTION, th.preprocessor);
    stc->StyleSetForeground(wxSTC_ASM_IDENTIFIER, th.attribute);
    stc->StyleSetForeground(wxSTC_ASM_OPERATOR, th.operatorColor);
    stc->StyleSetForeground(wxSTC_ASM_STRINGEOL, wxColour(220, 60, 60));
}

// ----------------------------------------------------------------------
// Verilog / VHDL
// ----------------------------------------------------------------------

static void ApplyVerilogStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_VERILOG);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "always and assign begin buf bufif0 bufif1 case casex casez cmos deassign default defparam "
        "disable edge else end endcase endfunction endmodule endprimitive endspecify endtable "
        "endtask event for force forever fork function highz0 highz1 if ifnone initial inout input "
        "integer join large medium module nand negedge nmos nor not notif0 notif1 or output "
        "parameter pmos posedge primitive pull0 pull1 pulldown pullup rcmos real realtime reg "
        "release repeat rnmos rpmos rtran rtranif0 rtranif1 scalared small specify specparam "
        "strength strong0 strong1 supply0 supply1 table task time tran tranif0 tranif1 tri tri0 "
        "tri1 triand trior trireg vectored wait wand weak0 weak1 while wire wor xnor xor");

    stc->StyleSetForeground(wxSTC_V_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_V_COMMENTLINE, th.comment);
    stc->StyleSetForeground(wxSTC_V_COMMENTLINEBANG, th.comment);
    stc->StyleSetForeground(wxSTC_V_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_V_STRING, th.string);
    stc->StyleSetForeground(wxSTC_V_WORD, th.keyword);
    stc->StyleSetBold(wxSTC_V_WORD, true);
    stc->StyleSetForeground(wxSTC_V_WORD2, th.keyword2);
    stc->StyleSetForeground(wxSTC_V_WORD3, th.preprocessor);
    stc->StyleSetForeground(wxSTC_V_PREPROCESSOR, th.preprocessor);
#ifdef wxSTC_V_INPUT
    stc->StyleSetForeground(wxSTC_V_INPUT, th.attribute);
    stc->StyleSetForeground(wxSTC_V_OUTPUT, th.attribute);
    stc->StyleSetForeground(wxSTC_V_INOUT, th.attribute);
    stc->StyleSetForeground(wxSTC_V_PORT_CONNECT, th.attribute);
#endif
    stc->StyleSetForeground(wxSTC_V_OPERATOR, th.operatorColor);
    stc->StyleSetForeground(wxSTC_V_STRINGEOL, wxColour(220, 60, 60));
}

static void ApplyVhdlStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_VHDL);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "access after alias all architecture array assert attribute begin block body buffer bus "
        "case component configuration constant disconnect downto else elsif end entity exit file "
        "for function generate generic group guarded if impure in inertial inout is label library "
        "linkage literal loop map new next null of on open others out package port postponed "
        "procedure process pure range record register reject report return select severity signal "
        "shared subtype then to transport type unaffected units until use variable wait when while "
        "with");

    stc->StyleSetForeground(wxSTC_VHDL_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_VHDL_COMMENTLINEBANG, th.comment);
#ifdef wxSTC_VHDL_BLOCK_COMMENT
    stc->StyleSetForeground(wxSTC_VHDL_BLOCK_COMMENT, th.comment);
#endif
    stc->StyleSetForeground(wxSTC_VHDL_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_VHDL_STRING, th.string);
    stc->StyleSetForeground(wxSTC_VHDL_KEYWORD, th.keyword);
    stc->StyleSetBold(wxSTC_VHDL_KEYWORD, true);
    stc->StyleSetForeground(wxSTC_VHDL_STDOPERATOR, th.keyword2);
    stc->StyleSetForeground(wxSTC_VHDL_STDFUNCTION, th.keyword2);
    stc->StyleSetForeground(wxSTC_VHDL_STDPACKAGE, th.keyword2);
    stc->StyleSetForeground(wxSTC_VHDL_STDTYPE, th.keyword2);
    stc->StyleSetForeground(wxSTC_VHDL_ATTRIBUTE, th.attribute);
    stc->StyleSetForeground(wxSTC_VHDL_USERWORD, th.attribute);
    stc->StyleSetForeground(wxSTC_VHDL_OPERATOR, th.operatorColor);
    stc->StyleSetForeground(wxSTC_VHDL_STRINGEOL, wxColour(220, 60, 60));
}

// ----------------------------------------------------------------------
// R
// ----------------------------------------------------------------------

static void ApplyRStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_R);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "if else repeat while function for in next break TRUE FALSE NULL NA Inf NaN");
    stc->SetKeyWords(1,
        "c q library require attach detach source setwd getwd print cat paste paste0 sprintf "
        "length nrow ncol dim names class summary head tail apply sapply lapply vapply mapply "
        "data.frame list vector matrix array factor");

    stc->StyleSetForeground(wxSTC_R_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_R_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_R_STRING, th.string);
    stc->StyleSetForeground(wxSTC_R_STRING2, th.string);
    stc->StyleSetForeground(wxSTC_R_KWORD, th.keyword);
    stc->StyleSetBold(wxSTC_R_KWORD, true);
    stc->StyleSetForeground(wxSTC_R_BASEKWORD, th.keyword2);
    stc->StyleSetForeground(wxSTC_R_OTHERKWORD, th.keyword2);
    stc->StyleSetForeground(wxSTC_R_IDENTIFIER, th.foreground);
    stc->StyleSetForeground(wxSTC_R_INFIX, th.operatorColor);
    stc->StyleSetForeground(wxSTC_R_OPERATOR, th.operatorColor);
    stc->StyleSetForeground(wxSTC_R_INFIXEOL, wxColour(220, 60, 60));
}

// ----------------------------------------------------------------------
// SQL
// ----------------------------------------------------------------------

static void ApplySqlStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_SQL);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "select insert update delete from where join inner outer left right full on group by "
        "having order limit offset union all distinct as into values set create table alter drop "
        "index view trigger procedure function begin end declare cursor if then else elseif while "
        "loop return case when exists in like between and or not null is primary key foreign "
        "references constraint default check unique");
    stc->SetKeyWords(1,
        "int integer smallint bigint decimal numeric float real double char varchar text date "
        "datetime timestamp boolean blob");

    stc->StyleSetForeground(wxSTC_SQL_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_SQL_COMMENTLINE, th.comment);
    stc->StyleSetForeground(wxSTC_SQL_COMMENTDOC, th.comment);
    stc->StyleSetForeground(wxSTC_SQL_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_SQL_STRING, th.string);
    stc->StyleSetForeground(wxSTC_SQL_CHARACTER, th.string);
    stc->StyleSetForeground(wxSTC_SQL_WORD, th.keyword);
    stc->StyleSetBold(wxSTC_SQL_WORD, true);
    stc->StyleSetForeground(wxSTC_SQL_WORD2, th.keyword2);
    stc->StyleSetForeground(wxSTC_SQL_IDENTIFIER, th.foreground);
    stc->StyleSetForeground(wxSTC_SQL_QUOTEDIDENTIFIER, th.attribute);
    stc->StyleSetForeground(wxSTC_SQL_OPERATOR, th.operatorColor);
    stc->StyleSetForeground(wxSTC_SQL_SQLPLUS, th.preprocessor);
    stc->StyleSetForeground(wxSTC_SQL_SQLPLUS_PROMPT, th.preprocessor);
}

// ----------------------------------------------------------------------
// Haskell
// ----------------------------------------------------------------------

static void ApplyHaskellStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_HASKELL);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "case class data default deriving do else foreign if import in infix infixl infixr "
        "instance let module newtype of then type where");

    stc->StyleSetForeground(wxSTC_HA_COMMENTLINE, th.comment);
    stc->StyleSetForeground(wxSTC_HA_COMMENTBLOCK, th.comment);
    stc->StyleSetForeground(wxSTC_HA_COMMENTBLOCK2, th.comment);
    stc->StyleSetForeground(wxSTC_HA_COMMENTBLOCK3, th.comment);
    stc->StyleSetForeground(wxSTC_HA_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_HA_STRING, th.string);
    stc->StyleSetForeground(wxSTC_HA_CHARACTER, th.string);
    stc->StyleSetForeground(wxSTC_HA_KEYWORD, th.keyword);
    stc->StyleSetBold(wxSTC_HA_KEYWORD, true);
    stc->StyleSetForeground(wxSTC_HA_CLASS, th.keyword2);
    stc->StyleSetForeground(wxSTC_HA_CAPITAL, th.keyword2);
    stc->StyleSetForeground(wxSTC_HA_MODULE, th.keyword2);
    stc->StyleSetForeground(wxSTC_HA_IMPORT, th.preprocessor);
#ifdef wxSTC_HA_PRAGMA
    stc->StyleSetForeground(wxSTC_HA_PRAGMA, th.preprocessor);
#endif
    stc->StyleSetForeground(wxSTC_HA_OPERATOR, th.operatorColor);
#ifdef wxSTC_HA_RESERVED_OPERATOR
    stc->StyleSetForeground(wxSTC_HA_RESERVED_OPERATOR, th.operatorColor);
#endif
#ifdef wxSTC_HA_STRINGEOL
    stc->StyleSetForeground(wxSTC_HA_STRINGEOL, wxColour(220, 60, 60));
#endif
}

// ----------------------------------------------------------------------
// Erlang
// ----------------------------------------------------------------------

static void ApplyErlangStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_ERLANG);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "after and andalso band begin bnot bor bsl bsr bxor case catch cond div end fun if let not "
        "of or orelse query receive rem try when xor");

    stc->StyleSetForeground(wxSTC_ERLANG_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_ERLANG_COMMENT_FUNCTION, th.comment);
    stc->StyleSetForeground(wxSTC_ERLANG_COMMENT_MODULE, th.comment);
    stc->StyleSetForeground(wxSTC_ERLANG_COMMENT_DOC, th.comment);
    stc->StyleSetForeground(wxSTC_ERLANG_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_ERLANG_STRING, th.string);
    stc->StyleSetForeground(wxSTC_ERLANG_CHARACTER, th.string);
    stc->StyleSetForeground(wxSTC_ERLANG_KEYWORD, th.keyword);
    stc->StyleSetBold(wxSTC_ERLANG_KEYWORD, true);
    stc->StyleSetForeground(wxSTC_ERLANG_BIFS, th.keyword2);
    stc->StyleSetForeground(wxSTC_ERLANG_FUNCTION_NAME, th.keyword2);
    stc->StyleSetForeground(wxSTC_ERLANG_MODULES, th.keyword2);
    stc->StyleSetForeground(wxSTC_ERLANG_MODULES_ATT, th.preprocessor);
    stc->StyleSetForeground(wxSTC_ERLANG_ATOM, th.attribute);
    stc->StyleSetForeground(wxSTC_ERLANG_VARIABLE, th.attribute);
    stc->StyleSetForeground(wxSTC_ERLANG_RECORD, th.attribute);
    stc->StyleSetForeground(wxSTC_ERLANG_MACRO, th.preprocessor);
    stc->StyleSetForeground(wxSTC_ERLANG_PREPROC, th.preprocessor);
    stc->StyleSetForeground(wxSTC_ERLANG_OPERATOR, th.operatorColor);
    stc->StyleSetForeground(wxSTC_ERLANG_UNKNOWN, wxColour(220, 60, 60));
}

// ----------------------------------------------------------------------
// Makefile
// ----------------------------------------------------------------------

static void ApplyMakefileStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_MAKEFILE);
    SetCommonStyleDefaults(stc);

    stc->StyleSetForeground(wxSTC_MAKE_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_MAKE_PREPROCESSOR, th.preprocessor);
    stc->StyleSetForeground(wxSTC_MAKE_TARGET, th.tag);
    stc->StyleSetBold(wxSTC_MAKE_TARGET, true);
    stc->StyleSetForeground(wxSTC_MAKE_IDENTIFIER, th.attribute);
    stc->StyleSetForeground(wxSTC_MAKE_OPERATOR, th.operatorColor);
    stc->StyleSetForeground(wxSTC_MAKE_IDEOL, wxColour(220, 60, 60));
}

// ----------------------------------------------------------------------
// CMake
// ----------------------------------------------------------------------

static void ApplyCMakeStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_CMAKE);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "add_executable add_library add_subdirectory add_custom_command add_custom_target "
        "add_definitions add_dependencies add_test cmake_minimum_required project set unset "
        "option include include_directories link_directories link_libraries target_link_libraries "
        "target_include_directories target_compile_definitions target_compile_options "
        "find_package find_library find_path find_program if else elseif endif foreach endforeach "
        "while endwhile function endfunction macro endmacro return message install file");

    stc->StyleSetForeground(wxSTC_CMAKE_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_CMAKE_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_CMAKE_STRINGDQ, th.string);
    stc->StyleSetForeground(wxSTC_CMAKE_STRINGLQ, th.string);
    stc->StyleSetForeground(wxSTC_CMAKE_STRINGRQ, th.string);
    stc->StyleSetForeground(wxSTC_CMAKE_STRINGVAR, th.attribute);
    stc->StyleSetForeground(wxSTC_CMAKE_VARIABLE, th.attribute);
    stc->StyleSetForeground(wxSTC_CMAKE_COMMANDS, th.keyword);
    stc->StyleSetBold(wxSTC_CMAKE_COMMANDS, true);
    stc->StyleSetForeground(wxSTC_CMAKE_PARAMETERS, th.keyword2);
    stc->StyleSetForeground(wxSTC_CMAKE_USERDEFINED, th.keyword2);
    stc->StyleSetForeground(wxSTC_CMAKE_IFDEFINEDEF, th.preprocessor);
    stc->StyleSetForeground(wxSTC_CMAKE_MACRODEF, th.preprocessor);
    stc->StyleSetForeground(wxSTC_CMAKE_FOREACHDEF, th.preprocessor);
    stc->StyleSetForeground(wxSTC_CMAKE_WHILEDEF, th.preprocessor);
}

// ----------------------------------------------------------------------
// NSIS
// ----------------------------------------------------------------------

static void ApplyNsisStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_NSIS);
    SetCommonStyleDefaults(stc);

    stc->StyleSetForeground(wxSTC_NSIS_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_NSIS_COMMENTBOX, th.comment);
    stc->StyleSetForeground(wxSTC_NSIS_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_NSIS_STRINGDQ, th.string);
    stc->StyleSetForeground(wxSTC_NSIS_STRINGLQ, th.string);
    stc->StyleSetForeground(wxSTC_NSIS_STRINGRQ, th.string);
    stc->StyleSetForeground(wxSTC_NSIS_STRINGVAR, th.attribute);
    stc->StyleSetForeground(wxSTC_NSIS_VARIABLE, th.attribute);
    stc->StyleSetForeground(wxSTC_NSIS_FUNCTION, th.keyword);
    stc->StyleSetBold(wxSTC_NSIS_FUNCTION, true);
    stc->StyleSetForeground(wxSTC_NSIS_FUNCTIONDEF, th.keyword2);
    stc->StyleSetForeground(wxSTC_NSIS_SECTIONDEF, th.keyword2);
    stc->StyleSetForeground(wxSTC_NSIS_SUBSECTIONDEF, th.keyword2);
    stc->StyleSetForeground(wxSTC_NSIS_SECTIONGROUP, th.keyword2);
    stc->StyleSetForeground(wxSTC_NSIS_PAGEEX, th.keyword2);
    stc->StyleSetForeground(wxSTC_NSIS_IFDEFINEDEF, th.preprocessor);
    stc->StyleSetForeground(wxSTC_NSIS_MACRODEF, th.preprocessor);
    stc->StyleSetForeground(wxSTC_NSIS_LABEL, th.tag);
    stc->StyleSetForeground(wxSTC_NSIS_USERDEFINED, th.attribute);
}

// ----------------------------------------------------------------------
// Inno Setup
// ----------------------------------------------------------------------

static void ApplyInnoSetupStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_INNOSETUP);
    SetCommonStyleDefaults(stc);

    stc->StyleSetForeground(wxSTC_INNO_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_INNO_COMMENT_PASCAL, th.comment);
    stc->StyleSetForeground(wxSTC_INNO_STRING_DOUBLE, th.string);
    stc->StyleSetForeground(wxSTC_INNO_STRING_SINGLE, th.string);
    stc->StyleSetForeground(wxSTC_INNO_SECTION, th.tag);
    stc->StyleSetBold(wxSTC_INNO_SECTION, true);
    stc->StyleSetForeground(wxSTC_INNO_KEYWORD, th.keyword);
    stc->StyleSetBold(wxSTC_INNO_KEYWORD, true);
    stc->StyleSetForeground(wxSTC_INNO_KEYWORD_PASCAL, th.keyword2);
    stc->StyleSetForeground(wxSTC_INNO_KEYWORD_USER, th.keyword2);
    stc->StyleSetForeground(wxSTC_INNO_PARAMETER, th.attribute);
    stc->StyleSetForeground(wxSTC_INNO_IDENTIFIER, th.attribute);
    stc->StyleSetForeground(wxSTC_INNO_PREPROC, th.preprocessor);
    stc->StyleSetForeground(wxSTC_INNO_INLINE_EXPANSION, th.preprocessor);
}

// ----------------------------------------------------------------------
// LaTeX (shares style constants with Scintilla's TeX lexer)
// ----------------------------------------------------------------------

static void ApplyLatexStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_LATEX);
    SetCommonStyleDefaults(stc);

    stc->StyleSetForeground(wxSTC_TEX_TEXT, th.foreground);
    stc->StyleSetForeground(wxSTC_TEX_COMMAND, th.keyword);
    stc->StyleSetBold(wxSTC_TEX_COMMAND, true);
    stc->StyleSetForeground(wxSTC_TEX_GROUP, th.attribute);
    stc->StyleSetForeground(wxSTC_TEX_SYMBOL, th.operatorColor);
    stc->StyleSetForeground(wxSTC_TEX_SPECIAL, th.preprocessor);
}

// ----------------------------------------------------------------------
// Diff / patch
// ----------------------------------------------------------------------

static void ApplyDiffStyles(wxStyledTextCtrl *stc)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(wxSTC_LEX_DIFF);
    SetCommonStyleDefaults(stc);

    stc->StyleSetForeground(wxSTC_DIFF_COMMENT, th.comment);
    stc->StyleSetForeground(wxSTC_DIFF_COMMAND, th.preprocessor);
    stc->StyleSetForeground(wxSTC_DIFF_HEADER, th.keyword2);
    stc->StyleSetBold(wxSTC_DIFF_HEADER, true);
    stc->StyleSetForeground(wxSTC_DIFF_POSITION, th.tag);
    stc->StyleSetForeground(wxSTC_DIFF_ADDED, wxColour(100, 180, 100));
    stc->StyleSetForeground(wxSTC_DIFF_DELETED, wxColour(200, 90, 90));
    stc->StyleSetForeground(wxSTC_DIFF_CHANGED, th.keyword);
}

// ----------------------------------------------------------------------
// Visual Basic / VBScript (share Scintilla's "Basic-family" style set)
// ----------------------------------------------------------------------

static void ApplyBasicStyles(wxStyledTextCtrl *stc, bool isVbScript)
{
    const EditorTheme &th = CurrentTheme();
    stc->SetLexer(isVbScript ? wxSTC_LEX_VBSCRIPT : wxSTC_LEX_VB);
    SetCommonStyleDefaults(stc);

    stc->SetKeyWords(0,
        "and as boolean byref byte byval call case class const continue dim do double each else "
        "elseif end enum error exit false for function get global goto if implements in integer is "
        "let lib like long loop me mod module new next not nothing object on optional or private "
        "property public raiseevent redim rem resume return select set shared single static step "
        "stop string sub then to true type typeof until wend while with withevents");

    stc->StyleSetForeground(wxSTC_B_COMMENT, th.comment);
#ifdef wxSTC_B_COMMENTBLOCK
    stc->StyleSetForeground(wxSTC_B_COMMENTBLOCK, th.comment);
#endif
#ifdef wxSTC_B_DOCLINE
    stc->StyleSetForeground(wxSTC_B_DOCLINE, th.comment);
#endif
#ifdef wxSTC_B_DOCBLOCK
    stc->StyleSetForeground(wxSTC_B_DOCBLOCK, th.comment);
#endif
    stc->StyleSetForeground(wxSTC_B_NUMBER, th.number);
    stc->StyleSetForeground(wxSTC_B_BINNUMBER, th.number);
    stc->StyleSetForeground(wxSTC_B_HEXNUMBER, th.number);
    stc->StyleSetForeground(wxSTC_B_STRING, th.string);
    stc->StyleSetForeground(wxSTC_B_CONSTANT, th.attribute);
    stc->StyleSetForeground(wxSTC_B_KEYWORD, th.keyword);
    stc->StyleSetBold(wxSTC_B_KEYWORD, true);
    stc->StyleSetForeground(wxSTC_B_KEYWORD2, th.keyword2);
    stc->StyleSetForeground(wxSTC_B_KEYWORD3, th.keyword2);
    stc->StyleSetForeground(wxSTC_B_KEYWORD4, th.keyword2);
    stc->StyleSetForeground(wxSTC_B_PREPROCESSOR, th.preprocessor);
    stc->StyleSetForeground(wxSTC_B_LABEL, th.tag);
    stc->StyleSetForeground(wxSTC_B_ASM, th.keyword2);
    stc->StyleSetForeground(wxSTC_B_OPERATOR, th.operatorColor);
    stc->StyleSetForeground(wxSTC_B_STRINGEOL, wxColour(220, 60, 60));
    stc->StyleSetForeground(wxSTC_B_ERROR, wxColour(220, 60, 60));
}

// ----------------------------------------------------------------------
// Dispatcher
// ----------------------------------------------------------------------

void ApplyHighlighting(wxStyledTextCtrl *stc, Language lang)
{
    switch (lang)
    {
        case Language::Cpp:
        case Language::CSharp:
        case Language::Java:
        case Language::JavaScript:
        case Language::TypeScript:
        case Language::Go:
        case Language::Swift:
        case Language::ObjectiveC:
        case Language::ActionScript:
            ApplyCFamilyStyles(stc, lang);
            break;
        case Language::Python:      ApplyPythonStyles(stc); break;
        case Language::Html:        ApplyMarkupStyles(stc, false); break;
        case Language::Xml:         ApplyMarkupStyles(stc, true); break;
        case Language::Php:         ApplyPhpStyles(stc); break;
        case Language::Markdown:    ApplyMarkdownStyles(stc); break;
        case Language::Css:         ApplyCssStyles(stc); break;
        case Language::Json:        ApplyJsonStyles(stc); break;
        case Language::Yaml:        ApplyYamlStyles(stc); break;
        case Language::Ini:         ApplyIniStyles(stc); break;
        case Language::Bash:        ApplyBashStyles(stc); break;
        case Language::PowerShell:  ApplyPowerShellStyles(stc); break;
        case Language::Batch:       ApplyBatchStyles(stc); break;
        case Language::Tcl:         ApplyTclStyles(stc); break;
        case Language::CoffeeScript: ApplyCoffeeScriptStyles(stc); break;
        case Language::Ruby:        ApplyRubyStyles(stc); break;
        case Language::Perl:        ApplyPerlStyles(stc); break;
        case Language::Lua:         ApplyLuaStyles(stc); break;
        case Language::Rust:        ApplyRustStyles(stc); break;
        case Language::D:           ApplyDStyles(stc); break;
        case Language::Pascal:      ApplyPascalStyles(stc); break;
        case Language::Fortran:     ApplyFortranStyles(stc); break;
        case Language::Ada:         ApplyAdaStyles(stc); break;
        case Language::Assembly:    ApplyAsmStyles(stc); break;
        case Language::Verilog:     ApplyVerilogStyles(stc); break;
        case Language::Vhdl:        ApplyVhdlStyles(stc); break;
        case Language::R:           ApplyRStyles(stc); break;
        case Language::Sql:         ApplySqlStyles(stc); break;
        case Language::Haskell:     ApplyHaskellStyles(stc); break;
        case Language::Erlang:      ApplyErlangStyles(stc); break;
        case Language::Makefile:    ApplyMakefileStyles(stc); break;
        case Language::CMake:       ApplyCMakeStyles(stc); break;
        case Language::Nsis:        ApplyNsisStyles(stc); break;
        case Language::InnoSetup:   ApplyInnoSetupStyles(stc); break;
        case Language::LaTeX:       ApplyLatexStyles(stc); break;
        case Language::Diff:        ApplyDiffStyles(stc); break;
        case Language::VisualBasic: ApplyBasicStyles(stc, false); break;
        case Language::VbScript:    ApplyBasicStyles(stc, true); break;
        case Language::Auto: // shouldn't happen -- caller should have resolved it
        case Language::PlainText:
        default:
            ApplyPlainText(stc);
            break;
    }

    stc->Colourise(0, -1); // force a full re-lex now that styles/keywords changed
}

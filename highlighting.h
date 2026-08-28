#pragma once

#include <wx/stc/stc.h>
#include <wx/string.h>
#include <vector>

// ============================================================================
// SYNTAX HIGHLIGHTING (Scintilla built-in lexers + theme colors)
//
// A tab's highlighting is driven by a Language value. Normally that value is
// derived from the file's extension (DetectLanguageFromExtension), but a tab
// can also carry an explicit user override -- e.g. picked from the Language
// menu -- which is stored by the caller (see ClearTextFrame::TabData) and
// takes priority over the extension whenever it isn't Language::Auto.
// ============================================================================

enum class Language
{
    Auto = 0, // not a real style -- means "use the file extension"
    PlainText,
    Cpp,
    Python,
    Html,
    Xml,
    Markdown,
};

struct LanguageInfo
{
    Language id;
    wxString label; // display name, used in the Language menu and status bar
};

// All selectable languages in menu order, Auto first. Use this to build the
// Language menu and to resolve a menu item id back to a Language.
const std::vector<LanguageInfo> &AllLanguages();

// Display name for `lang`, e.g. "C / C++" -- used for the status bar and
// menu labels. Language::Auto returns "Auto-Detect".
wxString LanguageDisplayName(Language lang);

// Maps a file's extension to a Language. Returns Language::PlainText for
// unknown or missing extensions. Never returns Language::Auto.
Language DetectLanguageFromExtension(const wxString &filePath);

// Applies lexer, keywords, and theme colors for `lang` to `stc`. `lang` must
// be a concrete language, not Language::Auto -- callers in "auto" mode
// should resolve via DetectLanguageFromExtension() first.
void ApplyHighlighting(wxStyledTextCtrl *stc, Language lang);

// ----------------------------------------------------------------------
// Theme + font size: process-wide display settings that every
// ApplyHighlighting() call picks up, alongside the palette list in themes.h.
// ----------------------------------------------------------------------
extern const int kMinFontSize;
extern const int kMaxFontSize;
extern const int kDefaultFontSize;

int GetThemeIndex();
void SetThemeIndex(int index); // caller is responsible for range-checking

int GetFontSize();
void SetFontSize(int size); // caller is responsible for range-checking

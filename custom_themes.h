#pragma once

#include "themes.h"
#include <vector>

// Extends themes.h's compiled-in AllThemes() with user-defined themes read
// from the config file (see app_config.h), so a custom color scheme works
// the same way as a built-in one everywhere -- the Theme menu, the actual
// highlighting (see highlighting.h's CurrentTheme), and persistence all
// index into the combined list this module returns.
//
// A custom theme is added by hand-editing the config file's [CustomThemes]
// section: one "N/Name" key plus one "N/<Field>" hex-color key per
// EditorTheme field (Background, Foreground, Caret, SelectionBg, MarginBg,
// MarginFg, Comment, Number, String, Preprocessor, Keyword, Keyword2,
// OperatorColor, Tag, Attribute, MarkupCode), e.g.:
//
//   [CustomThemes]
//   Count=1
//   0/Name=Solarized Dark
//   0/Background=#002b36
//   0/Foreground=#839496
//   ...
//
// Any field left out falls back to a reasonable default rather than
// failing the whole theme. Use View > Theme > Reload Custom Themes to pick
// up edits without restarting.
namespace CustomThemes
{
    // Built-in themes (themes.h) followed by custom ones from the config
    // file. Cached in memory after the first call; Reload() re-reads it.
    const std::vector<EditorTheme> &All();

    // True if `index` refers to a custom (config-file-defined) theme
    // rather than one of the compiled-in ones.
    bool IsCustom(size_t index);

    // Re-reads custom themes from the config file, discarding anything
    // cached from a previous load.
    void Reload();
}

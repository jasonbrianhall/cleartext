#pragma once

#include "themes.h"
#include <vector>

// Extends themes.h's compiled-in AllThemes() with user-defined themes,
// created and managed via View > Theme > New/Edit/Delete Theme (see
// theme_editor.h) and persisted to the config file (see app_config.h), so a
// custom color scheme works the same way as a built-in one everywhere -- the
// Theme menu, the actual highlighting (see highlighting.h's CurrentTheme),
// and persistence all index into the combined list this module returns.
namespace CustomThemes
{
    // Built-in themes (themes.h) followed by custom ones from the config
    // file. Cached in memory after the first call.
    const std::vector<EditorTheme> &All();

    // True if `index` refers to a custom (user-created) theme rather than
    // one of the compiled-in ones -- e.g. to decide whether Edit/Delete
    // should be enabled for the currently selected theme.
    bool IsCustom(size_t index);

    // Appends `theme` as a new custom theme, persists the change, and
    // returns its index in All().
    size_t Add(const EditorTheme &theme);

    // Replaces the custom theme at `index` with `theme` and persists the
    // change. Returns false (no-op) if `index` isn't a custom theme.
    bool Update(size_t index, const EditorTheme &theme);

    // Removes the custom theme at `index` and persists the change. Returns
    // false (no-op) if `index` isn't a custom theme. Indices at or after
    // `index` shift down by one afterward.
    bool Remove(size_t index);

    // Re-reads custom themes from the config file, discarding anything
    // cached from a previous load or added in memory since. Not needed in
    // the normal Add/Update/Remove flow, which already keeps the in-memory
    // list and the config file in sync -- useful mainly if the file was
    // edited by hand or by another process.
    void Reload();
}

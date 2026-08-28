#pragma once

#include <wx/dialog.h>
#include <vector>
#include "themes.h"

class wxColourPickerCtrl;
class wxColourPickerEvent;
class wxTextCtrl;
class wxStyledTextCtrl;

// Modal dialog for creating or editing a custom color theme: one color
// picker per EditorTheme field (opens the OS's native color picker, which
// includes a hue wheel on most platforms/themes) plus a live preview pane
// showing sample code styled with the colors as they're picked.
//
// Used for both "New Theme..." (seeded from the currently active theme's
// colors, with the name field cleared) and "Edit Theme..." (seeded from the
// exact custom theme being edited) -- see ClearTextFrame::OnNewTheme /
// OnEditTheme, which handle persisting the result via custom_themes.h.
class ThemeEditorDialog : public wxDialog
{
public:
    ThemeEditorDialog(wxWindow *parent, const wxString &title, const EditorTheme &initial);

    // Valid after ShowModal() returns wxID_OK.
    EditorTheme GetTheme() const;

private:
    // One picker per EditorTheme field, addressed via pointer-to-member so
    // adding a field to EditorTheme later only means adding one entry to
    // the field table in the .cpp, not touching the row-building logic.
    struct ColorRow
    {
        wxColourPickerCtrl *picker;
        wxColour EditorTheme::*field;
    };

    wxTextCtrl *m_nameField;
    wxStyledTextCtrl *m_preview;
    std::vector<ColorRow> m_rows;

    EditorTheme CollectTheme() const;
    void UpdatePreview();
    void OnColorChanged(wxColourPickerEvent &event);
    void OnOk(wxCommandEvent &event);
};

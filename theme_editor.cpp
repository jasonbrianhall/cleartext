#include "theme_editor.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/clrpicker.h>
#include <wx/textctrl.h>
#include <wx/scrolwin.h>
#include <wx/stc/stc.h>
#include <wx/msgdlg.h>

namespace
{
    struct FieldDef
    {
        wxColour EditorTheme::*field;
        const char *label;
    };

    // Every EditorTheme field, in the order rows are drawn. Add a new
    // theme field here and it appears in the dialog automatically.
    const FieldDef kFields[] = {
        {&EditorTheme::background, "Background"},
        {&EditorTheme::foreground, "Foreground"},
        {&EditorTheme::caret, "Caret"},
        {&EditorTheme::selectionBg, "Selection Background"},
        {&EditorTheme::marginBg, "Margin Background"},
        {&EditorTheme::marginFg, "Margin Foreground"},
        {&EditorTheme::comment, "Comment"},
        {&EditorTheme::number, "Number"},
        {&EditorTheme::string, "String"},
        {&EditorTheme::preprocessor, "Preprocessor"},
        {&EditorTheme::keyword, "Keyword"},
        {&EditorTheme::keyword2, "Keyword (secondary)"},
        {&EditorTheme::operatorColor, "Operator"},
        {&EditorTheme::tag, "Tag"},
        {&EditorTheme::attribute, "Attribute"},
        {&EditorTheme::markupCode, "Markup Code"},
    };
}

ThemeEditorDialog::ThemeEditorDialog(wxWindow *parent, const wxString &title, const EditorTheme &initial)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(680, 540),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    wxBoxSizer *root = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer *nameRow = new wxBoxSizer(wxHORIZONTAL);
    nameRow->Add(new wxStaticText(this, wxID_ANY, "Theme Name:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    m_nameField = new wxTextCtrl(this, wxID_ANY, initial.name);
    nameRow->Add(m_nameField, 1);
    root->Add(nameRow, 0, wxEXPAND | wxALL, 10);

    wxBoxSizer *mainRow = new wxBoxSizer(wxHORIZONTAL);

    // Scrollable list of color pickers on the left -- 16 rows is too tall
    // for most screens at a comfortable dialog size otherwise.
    wxScrolledWindow *scroller = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxSize(340, -1));
    scroller->SetScrollRate(0, 12);
    wxFlexGridSizer *grid = new wxFlexGridSizer(2, 6, 10);
    grid->AddGrowableCol(1);

    for (const FieldDef &def : kFields)
    {
        grid->Add(new wxStaticText(scroller, wxID_ANY, def.label), 0, wxALIGN_CENTER_VERTICAL);
        wxColourPickerCtrl *picker = new wxColourPickerCtrl(scroller, wxID_ANY, initial.*(def.field));
        grid->Add(picker, 0);
        picker->Bind(wxEVT_COLOURPICKER_CHANGED, &ThemeEditorDialog::OnColorChanged, this);
        m_rows.push_back({picker, def.field});
    }
    scroller->SetSizer(grid);
    mainRow->Add(scroller, 1, wxEXPAND | wxRIGHT, 10);

    // Live preview on the right: sample C-like code, restyled on every
    // color change so the effect of each field is immediately visible.
    m_preview = new wxStyledTextCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(300, -1));
    m_preview->SetText(
        "// Sample code\n"
        "#include <iostream>\n"
        "\n"
        "int main() {\n"
        "    int count = 42;\n"
        "    std::string name = \"ClearText\";\n"
        "    // A comment\n"
        "    if (count > 0) {\n"
        "        return count;\n"
        "    }\n"
        "}\n"
    );
    m_preview->SetReadOnly(true);
    m_preview->SetMarginWidth(1, 0);
    mainRow->Add(m_preview, 1, wxEXPAND);

    root->Add(mainRow, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);

    root->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);

    SetSizer(root);
    Bind(wxEVT_BUTTON, &ThemeEditorDialog::OnOk, this, wxID_OK);

    UpdatePreview();
}

EditorTheme ThemeEditorDialog::CollectTheme() const
{
    EditorTheme th;
    th.name = m_nameField->GetValue();
    for (const ColorRow &row : m_rows)
        th.*(row.field) = row.picker->GetColour();
    return th;
}

EditorTheme ThemeEditorDialog::GetTheme() const
{
    return CollectTheme();
}

void ThemeEditorDialog::UpdatePreview()
{
    EditorTheme th = CollectTheme();

    m_preview->SetLexer(wxSTC_LEX_CPP);
    wxFont font(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    m_preview->StyleSetFont(wxSTC_STYLE_DEFAULT, font);
    m_preview->StyleSetBackground(wxSTC_STYLE_DEFAULT, th.background);
    m_preview->StyleSetForeground(wxSTC_STYLE_DEFAULT, th.foreground);
    m_preview->StyleClearAll();

    m_preview->SetCaretLineVisible(true);
    m_preview->SetCaretLineBackground(th.marginBg);
    m_preview->SetSelBackground(true, th.selectionBg);

    m_preview->SetKeyWords(0, "int if return");
    m_preview->SetKeyWords(1, "std string");
    m_preview->StyleSetForeground(wxSTC_C_COMMENT, th.comment);
    m_preview->StyleSetForeground(wxSTC_C_COMMENTLINE, th.comment);
    m_preview->StyleSetForeground(wxSTC_C_NUMBER, th.number);
    m_preview->StyleSetForeground(wxSTC_C_STRING, th.string);
    m_preview->StyleSetForeground(wxSTC_C_PREPROCESSOR, th.preprocessor);
    m_preview->StyleSetForeground(wxSTC_C_WORD, th.keyword);
    m_preview->StyleSetBold(wxSTC_C_WORD, true);
    m_preview->StyleSetForeground(wxSTC_C_WORD2, th.keyword2);
    m_preview->StyleSetForeground(wxSTC_C_OPERATOR, th.operatorColor);

    m_preview->Colourise(0, -1);
}

void ThemeEditorDialog::OnColorChanged(wxColourPickerEvent &event)
{
    UpdatePreview();
}

void ThemeEditorDialog::OnOk(wxCommandEvent &event)
{
    if (m_nameField->GetValue().Trim(true).Trim(false).IsEmpty())
    {
        wxMessageBox("Please enter a name for this theme.", "Theme Name Required",
            wxOK | wxICON_WARNING, this);
        return; // keep the dialog open
    }
    EndModal(wxID_OK);
}

#include "emoji_picker.h"
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/scrolwin.h>
#include <wx/notebook.h>
#include <wx/button.h>
#include <vector>

namespace
{
    struct EmojiEntry { const char *glyph; const char *name; };
    struct EmojiCategory { const char *label; std::vector<EmojiEntry> entries; };

    // Glyphs are UTF-8 string literals (\U... universal character names);
    // the project is built with a UTF-8 source/execution charset, same
    // assumption copy_html.cpp's EscapeHtml etc. already rely on.
    const std::vector<EmojiCategory> &AllCategories()
    {
        static const std::vector<EmojiCategory> categories = {
            {"Smileys", {
                {"\U0001F600", "grinning face"},
                {"\U0001F604", "grinning face smiling eyes"},
                {"\U0001F923", "rolling on the floor laughing"},
                {"\U0001F602", "face with tears of joy"},
                {"\U0001F642", "slightly smiling face"},
                {"\U0001F643", "upside down face"},
                {"\U0001F609", "winking face"},
                {"\U0001F60A", "smiling face smiling eyes"},
                {"\U0001F970", "smiling face with hearts"},
                {"\U0001F60D", "heart eyes"},
                {"\U0001F929", "star struck"},
                {"\U0001F618", "face blowing a kiss"},
                {"\U0001F61B", "face with tongue"},
                {"\U0001F92A", "zany face"},
                {"\U0001F917", "hugging face"},
                {"\U0001F92D", "face with hand over mouth"},
                {"\U0001F914", "thinking face"},
                {"\U0001F644", "face with rolling eyes"},
                {"\U0001F60F", "smirking face"},
                {"\U0001F612", "unamused face"},
                {"\U0001F614", "pensive face"},
                {"\U0001F622", "crying face"},
                {"\U0001F62D", "loudly crying face"},
                {"\U0001F631", "screaming in fear"},
                {"\U0001F620", "angry face"},
                {"\U0001F971", "yawning face"},
                {"\U0001F634", "sleeping face"},
                {"\U0001F912", "face with thermometer"},
                {"\U0001F92E", "face vomiting"},
                {"\U0001F975", "hot face"},
            }},
            {"Gestures", {
                {"\U0001F44D", "thumbs up"},
                {"\U0001F44E", "thumbs down"},
                {"\U0001F44F", "clapping hands"},
                {"\U0001F64C", "raising hands"},
                {"\U0001F91D", "handshake"},
                {"\U0001F64F", "folded hands"},
                {"\U0000270C", "victory hand"},
                {"\U0001F91E", "crossed fingers"},
                {"\U0001F918", "sign of the horns"},
                {"\U0001F919", "call me hand"},
                {"\U0001F44C", "OK hand"},
                {"\U0000270B", "raised hand"},
                {"\U0001F44B", "waving hand"},
                {"\U0001F4AA", "flexed biceps"},
                {"\U0001F449", "pointing right"},
                {"\U0001F448", "pointing left"},
                {"\U0001F446", "pointing up"},
                {"\U0001F447", "pointing down"},
            }},
            {"Hearts & Symbols", {
                {"\U00002764", "red heart"},
                {"\U0001F9E1", "orange heart"},
                {"\U0001F49B", "yellow heart"},
                {"\U0001F49A", "green heart"},
                {"\U0001F499", "blue heart"},
                {"\U0001F49C", "purple heart"},
                {"\U0001F494", "broken heart"},
                {"\U0001F495", "two hearts"},
                {"\U00002B50", "star"},
                {"\U0001F31F", "glowing star"},
                {"\U00002728", "sparkles"},
                {"\U0001F525", "fire"},
                {"\U0001F4AF", "hundred points"},
                {"\U00002705", "check mark"},
                {"\U0000274C", "cross mark"},
                {"\U00002753", "question mark"},
                {"\U00002757", "exclamation mark"},
                {"\U0001F389", "party popper"},
                {"\U0001F38A", "confetti ball"},
            }},
            {"Animals & Nature", {
                {"\U0001F436", "dog face"},
                {"\U0001F431", "cat face"},
                {"\U0001F42D", "mouse face"},
                {"\U0001F43B", "bear face"},
                {"\U0001F98A", "fox face"},
                {"\U0001F42F", "tiger face"},
                {"\U0001F981", "lion face"},
                {"\U0001F42E", "cow face"},
                {"\U0001F437", "pig face"},
                {"\U0001F438", "frog face"},
                {"\U0001F435", "monkey face"},
                {"\U0001F427", "penguin"},
                {"\U0001F41D", "honeybee"},
                {"\U0001F98B", "butterfly"},
                {"\U0001F433", "spouting whale"},
                {"\U0001F41F", "fish"},
                {"\U0001F332", "evergreen tree"},
                {"\U0001F33B", "sunflower"},
                {"\U0001F338", "cherry blossom"},
                {"\U0001F340", "four leaf clover"},
                {"\U0001F31E", "sun with face"},
                {"\U0001F319", "crescent moon"},
            }},
            {"Food", {
                {"\U0001F34E", "red apple"},
                {"\U0001F34C", "banana"},
                {"\U0001F347", "grapes"},
                {"\U0001F353", "strawberry"},
                {"\U0001F349", "watermelon"},
                {"\U0001F34D", "pineapple"},
                {"\U0001F345", "tomato"},
                {"\U0001F355", "pizza"},
                {"\U0001F354", "hamburger"},
                {"\U0001F35F", "french fries"},
                {"\U0001F32D", "hot dog"},
                {"\U0001F32E", "taco"},
                {"\U0001F363", "sushi"},
                {"\U0001F35C", "ramen"},
                {"\U0001F366", "soft ice cream"},
                {"\U0001F370", "shortcake"},
                {"\U0001F36A", "cookie"},
                {"\U00002615", "hot beverage"},
                {"\U0001F37A", "beer mug"},
                {"\U0001F377", "wine glass"},
            }},
            {"Objects", {
                {"\U0001F4BB", "laptop"},
                {"\U0001F4F1", "mobile phone"},
                {"\U0001F4A1", "light bulb"},
                {"\U0001F4DA", "books"},
                {"\U0001F4DD", "memo"},
                {"\U0001F4CE", "paperclip"},
                {"\U0001F517", "link"},
                {"\U0001F512", "locked"},
                {"\U0001F513", "unlocked"},
                {"\U0001F511", "key"},
                {"\U0001F527", "wrench"},
                {"\U00002699", "gear"},
                {"\U0001F4E6", "package"},
                {"\U0001F4C1", "file folder"},
                {"\U0001F4CA", "bar chart"},
                {"\U0001F4C5", "calendar"},
                {"\U000023F0", "alarm clock"},
                {"\U0001F4B0", "money bag"},
                {"\U0001F3AF", "direct hit"},
                {"\U0001F680", "rocket"},
                {"\U0001F41B", "bug"},
            }},
        };
        return categories;
    }

    class EmojiPickerDialog : public wxDialog
    {
    public:
        explicit EmojiPickerDialog(wxWindow *parent)
            : wxDialog(parent, wxID_ANY, "Insert Emoji", wxDefaultPosition, wxSize(420, 380),
                       wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
        {
            wxBoxSizer *root = new wxBoxSizer(wxVERTICAL);

            m_search = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                       wxTE_PROCESS_ENTER);
            m_search->SetHint("Search emoji...");
            root->Add(m_search, 0, wxEXPAND | wxALL, 8);

            m_notebook = new wxNotebook(this, wxID_ANY);
            for (const EmojiCategory &cat : AllCategories())
            {
                wxScrolledWindow *page = new wxScrolledWindow(m_notebook, wxID_ANY);
                page->SetScrollRate(0, 12);
                wxGridSizer *grid = new wxGridSizer(8, 4, 4);
                for (const EmojiEntry &e : cat.entries)
                    grid->Add(MakeButton(page, e), 0, wxEXPAND);
                page->SetSizer(grid);
                page->FitInside();
                m_notebook->AddPage(page, cat.label);
            }
            root->Add(m_notebook, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

            // Flat search-results page, shown in place of the notebook
            // while the search field is non-empty; rebuilt on every
            // keystroke from scratch since the match set is cheap to redo.
            m_resultsPage = new wxScrolledWindow(this, wxID_ANY);
            m_resultsPage->SetScrollRate(0, 12);
            m_resultsSizer = new wxGridSizer(8, 4, 4);
            m_resultsPage->SetSizer(m_resultsSizer);
            m_resultsPage->Hide();
            root->Add(m_resultsPage, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

            SetSizer(root);

            m_search->Bind(wxEVT_TEXT, &EmojiPickerDialog::OnSearchText, this);
            m_search->SetFocus();
        }

        wxString GetChosenEmoji() const { return m_chosen; }

    private:
        wxTextCtrl *m_search;
        wxNotebook *m_notebook;
        wxScrolledWindow *m_resultsPage;
        wxGridSizer *m_resultsSizer;
        wxString m_chosen;

        wxButton *MakeButton(wxWindow *parent, const EmojiEntry &e)
        {
            wxString glyph = wxString::FromUTF8(e.glyph);
            wxButton *btn = new wxButton(parent, wxID_ANY, glyph, wxDefaultPosition, wxSize(40, 36));
            btn->SetToolTip(e.name);
            btn->Bind(wxEVT_BUTTON, [this, glyph](wxCommandEvent&) {
                m_chosen = glyph;
                EndModal(wxID_OK);
            });
            return btn;
        }

        void OnSearchText(wxCommandEvent &event)
        {
            wxString query = m_search->GetValue().Lower().Trim(true).Trim(false);

            if (query.IsEmpty())
            {
                m_resultsPage->Hide();
                m_notebook->Show();
                Layout();
                return;
            }

            m_resultsSizer->Clear(true); // destroys the previous match buttons too
            for (const EmojiCategory &cat : AllCategories())
                for (const EmojiEntry &e : cat.entries)
                    if (wxString(e.name).Lower().Find(query) != wxNOT_FOUND)
                        m_resultsSizer->Add(MakeButton(m_resultsPage, e), 0, wxEXPAND);

            m_notebook->Hide();
            m_resultsPage->Show();
            m_resultsPage->FitInside();
            Layout();
        }
    };
}

wxString ShowEmojiPicker(wxWindow *parent)
{
    EmojiPickerDialog dlg(parent);
    if (dlg.ShowModal() != wxID_OK) return wxString();
    return dlg.GetChosenEmoji();
}

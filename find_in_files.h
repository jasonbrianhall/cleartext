#pragma once

#include <wx/dialog.h>
#include <wx/listctrl.h>
#include <wx/textctrl.h>
#include <wx/radiobut.h>
#include <wx/checkbox.h>
#include <wx/stattext.h>
#include <vector>

class ClearTextFrame;

// "Find in Files" dialog: searches either every open tab's in-memory text
// or every text file under a chosen folder, and lists one match per line
// the user can double-click to jump to (opening the file in a new tab if
// it isn't already open). Persistent per ClearTextFrame -- closing it via
// the window's X just hides it, so search text/results survive reopening.
class FindInFilesDialog : public wxDialog
{
public:
    explicit FindInFilesDialog(ClearTextFrame *frame);

    void FocusSearchField();

private:
    // One search hit: `tabIndex` is >= 0 when the hit came from an
    // already-open tab (which may be an unsaved "Untitled" tab with no
    // `path`); it's -1 for a hit found by scanning a folder, where `path`
    // is always a real file to open.
    struct Match
    {
        wxString path;
        int tabIndex;
        int line;
    };

    ClearTextFrame *m_frame;
    wxTextCtrl *m_searchText;
    wxRadioButton *m_scopeTabs;
    wxRadioButton *m_scopeFolder;
    wxTextCtrl *m_folderPath;
    wxCheckBox *m_matchCase;
    wxCheckBox *m_wholeWord;
    wxListCtrl *m_results;
    wxStaticText *m_statusText;
    std::vector<Match> m_matches;

    void OnBrowse(wxCommandEvent &event);
    void OnSearch(wxCommandEvent &event);
    void OnResultActivated(wxListEvent &event);
    void OnClose(wxCloseEvent &event);

    void SearchOpenTabs(const wxString &needle, bool matchCase, bool wholeWord);
    void SearchFolder(const wxString &folder, const wxString &needle, bool matchCase, bool wholeWord);
    void SearchTextBuffer(const wxString &label, const wxString &path, int tabIndex,
                           const wxString &text, const wxString &needle, bool matchCase, bool wholeWord);
    void AddResult(const wxString &label, const wxString &path, int tabIndex, int line,
                   const wxString &preview);
};

#include "find_in_files.h"
#include "editor_frame.h"
#include "encoding.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/checkbox.h>
#include <wx/radiobut.h>
#include <wx/button.h>
#include <wx/dirdlg.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/file.h>

namespace
{
    enum
    {
        ID_Search = wxID_HIGHEST + 2000,
        ID_Browse,
        ID_ScopeTabs,
        ID_ScopeFolder,
    };

    bool IsWordChar(wxUniChar c)
    {
        return wxIsalnum(c) || c == '_';
    }

    // Directories that are almost never worth scanning and can be huge
    // (VCS metadata, dependency trees, build output).
    bool IsSkippedDir(const wxString &name)
    {
        return name == ".git" || name == ".svn" || name == ".hg" ||
               name == "node_modules" || name == "build" || name == "__pycache__";
    }

    // Cheap binary-file heuristic: a NUL byte in the first chunk almost
    // never appears in real text, so treat it as "don't scan this file"
    // rather than trying (and likely failing) to search it as text.
    bool LooksBinary(const wxString &path)
    {
        wxFile file(path);
        if (!file.IsOpened()) return true;
        char buf[8192];
        ssize_t n = file.Read(buf, sizeof(buf));
        if (n <= 0) return false;
        for (ssize_t i = 0; i < n; i++)
            if (buf[i] == '\0') return true;
        return false;
    }

    const wxULongLong kMaxScanFileSize = 2 * 1024 * 1024; // 2 MB

    void CollectTextFiles(const wxString &dir, wxArrayString &out, int depth = 0)
    {
        if (depth > 12) return; // guard against pathological/symlink-loop recursion

        wxDir d(dir);
        if (!d.IsOpened()) return;

        wxString name;
        bool cont = d.GetFirst(&name, wxEmptyString, wxDIR_DIRS);
        while (cont)
        {
            if (name != "." && name != ".." && !IsSkippedDir(name))
                CollectTextFiles(dir + wxFileName::GetPathSeparator() + name, out, depth + 1);
            cont = d.GetNext(&name);
        }

        cont = d.GetFirst(&name, wxEmptyString, wxDIR_FILES);
        while (cont)
        {
            wxString path = dir + wxFileName::GetPathSeparator() + name;
            wxULongLong size = wxFileName::GetSize(path);
            if (size != wxInvalidSize && size <= kMaxScanFileSize && !LooksBinary(path))
                out.Add(path);
            cont = d.GetNext(&name);
        }
    }
}

FindInFilesDialog::FindInFilesDialog(ClearTextFrame *frame)
    : wxDialog(frame, wxID_ANY, "Find in Files", wxDefaultPosition, wxSize(560, 420),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_frame(frame)
{
    wxBoxSizer *root = new wxBoxSizer(wxVERTICAL);

    wxFlexGridSizer *searchRow = new wxFlexGridSizer(1, 2, 4, 8);
    searchRow->AddGrowableCol(1);
    searchRow->Add(new wxStaticText(this, wxID_ANY, "Find:"), 0, wxALIGN_CENTER_VERTICAL);
    m_searchText = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    searchRow->Add(m_searchText, 1, wxEXPAND);
    root->Add(searchRow, 0, wxEXPAND | wxALL, 8);

    wxBoxSizer *scopeRow = new wxBoxSizer(wxHORIZONTAL);
    m_scopeTabs = new wxRadioButton(this, ID_ScopeTabs, "Open Tabs", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    m_scopeTabs->SetValue(true);
    m_scopeFolder = new wxRadioButton(this, ID_ScopeFolder, "Folder:");
    m_folderPath = new wxTextCtrl(this, wxID_ANY);
    m_folderPath->Enable(false);
    wxButton *browseButton = new wxButton(this, ID_Browse, "Browse...");
    scopeRow->Add(m_scopeTabs, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
    scopeRow->Add(m_scopeFolder, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    scopeRow->Add(m_folderPath, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    scopeRow->Add(browseButton, 0);
    root->Add(scopeRow, 0, wxEXPAND | wxLEFT | wxRIGHT, 8);

    wxBoxSizer *optionsRow = new wxBoxSizer(wxHORIZONTAL);
    m_matchCase = new wxCheckBox(this, wxID_ANY, "Match Case");
    m_wholeWord = new wxCheckBox(this, wxID_ANY, "Whole Word");
    wxButton *searchButton = new wxButton(this, ID_Search, "Search");
    optionsRow->Add(m_matchCase, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
    optionsRow->Add(m_wholeWord, 0, wxALIGN_CENTER_VERTICAL);
    optionsRow->AddStretchSpacer();
    optionsRow->Add(searchButton, 0);
    root->Add(optionsRow, 0, wxEXPAND | wxALL, 8);

    m_results = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxLC_REPORT | wxLC_SINGLE_SEL);
    m_results->InsertColumn(0, "File", wxLIST_FORMAT_LEFT, 180);
    m_results->InsertColumn(1, "Line", wxLIST_FORMAT_RIGHT, 60);
    m_results->InsertColumn(2, "Text", wxLIST_FORMAT_LEFT, 280);
    root->Add(m_results, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    m_statusText = new wxStaticText(this, wxID_ANY, "");
    root->Add(m_statusText, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    SetSizer(root);

    Bind(wxEVT_BUTTON, &FindInFilesDialog::OnBrowse, this, ID_Browse);
    Bind(wxEVT_BUTTON, &FindInFilesDialog::OnSearch, this, ID_Search);
    Bind(wxEVT_TEXT_ENTER, &FindInFilesDialog::OnSearch, this, m_searchText->GetId());
    m_results->Bind(wxEVT_LIST_ITEM_ACTIVATED, &FindInFilesDialog::OnResultActivated, this);
    Bind(wxEVT_CLOSE_WINDOW, &FindInFilesDialog::OnClose, this);
}

void FindInFilesDialog::FocusSearchField()
{
    m_searchText->SetFocus();
    m_searchText->SelectAll();
}

void FindInFilesDialog::OnClose(wxCloseEvent &event)
{
    // Kept alive (just hidden) so the search text and results are still
    // there next time the frame reopens this dialog; the frame owns
    // destroying it when the whole window closes.
    Hide();
}

void FindInFilesDialog::OnBrowse(wxCommandEvent &event)
{
    wxDirDialog dlg(this, "Choose a folder to search", m_folderPath->GetValue());
    if (dlg.ShowModal() == wxID_CANCEL) return;

    m_folderPath->SetValue(dlg.GetPath());
    m_scopeFolder->SetValue(true);
}

void FindInFilesDialog::OnSearch(wxCommandEvent &event)
{
    wxString needle = m_searchText->GetValue();
    m_results->DeleteAllItems();
    m_matches.clear();
    m_statusText->SetLabel("");

    if (needle.IsEmpty()) return;

    bool matchCase = m_matchCase->GetValue();
    bool wholeWord = m_wholeWord->GetValue();

    if (m_scopeTabs->GetValue())
    {
        SearchOpenTabs(needle, matchCase, wholeWord);
    }
    else
    {
        wxString folder = m_folderPath->GetValue();
        if (folder.IsEmpty() || !wxDirExists(folder))
        {
            m_statusText->SetLabel("Choose a valid folder to search.");
            return;
        }
        wxBeginBusyCursor();
        SearchFolder(folder, needle, matchCase, wholeWord);
        wxEndBusyCursor();
    }

    m_statusText->SetLabel(wxString::Format(
        "%zu match%s found.", m_matches.size(), m_matches.size() == 1 ? "" : "es"));
}

void FindInFilesDialog::SearchOpenTabs(const wxString &needle, bool matchCase, bool wholeWord)
{
    int count = m_frame->GetTabCount();
    for (int i = 0; i < count; i++)
    {
        wxString label = m_frame->GetTabLabel(i);
        wxString path = m_frame->GetTabFilePath(i);
        wxString text = m_frame->GetTabText(i);
        SearchTextBuffer(label, path, i, text, needle, matchCase, wholeWord);
    }
}

void FindInFilesDialog::SearchFolder(const wxString &folder, const wxString &needle,
                                      bool matchCase, bool wholeWord)
{
    wxArrayString files;
    CollectTextFiles(folder, files);

    for (const wxString &path : files)
    {
        wxString text;
        if (!TextEncoding::ReadFile(path, text)) continue;
        SearchTextBuffer(wxFileName(path).GetFullName(), path, -1, text, needle, matchCase, wholeWord);
    }
}

void FindInFilesDialog::SearchTextBuffer(const wxString &label, const wxString &path, int tabIndex,
                                          const wxString &text, const wxString &needle,
                                          bool matchCase, bool wholeWord)
{
    wxArrayString lines = wxSplit(text, '\n');
    wxString needleCmp = matchCase ? needle : needle.Lower();

    for (size_t i = 0; i < lines.size(); i++)
    {
        const wxString &line = lines[i];
        wxString lineCmp = matchCase ? line : line.Lower();

        int searchFrom = 0;
        while (searchFrom <= (int)lineCmp.length())
        {
            int found = lineCmp.Mid(searchFrom).Find(needleCmp);
            if (found == wxNOT_FOUND) break;
            int matchPos = searchFrom + found;

            bool ok = true;
            if (wholeWord)
            {
                if (matchPos > 0 && IsWordChar(line[matchPos - 1])) ok = false;
                int endPos = matchPos + (int)needle.length();
                if (endPos < (int)line.length() && IsWordChar(line[endPos])) ok = false;
            }

            if (ok)
            {
                wxString preview = line;
                preview.Trim(true).Trim(false);
                AddResult(label, path, tabIndex, (int)i + 1, preview);
                break; // one row per matching line keeps the results list readable
            }

            searchFrom = matchPos + wxMax(1, (int)needle.length());
        }
    }
}

void FindInFilesDialog::AddResult(const wxString &label, const wxString &path, int tabIndex,
                                   int line, const wxString &preview)
{
    long row = (long)m_matches.size();
    m_results->InsertItem(row, label);
    m_results->SetItem(row, 1, wxString::Format("%d", line));
    m_results->SetItem(row, 2, preview);
    m_matches.push_back({path, tabIndex, line});
}

void FindInFilesDialog::OnResultActivated(wxListEvent &event)
{
    long idx = event.GetIndex();
    if (idx < 0 || idx >= (long)m_matches.size()) return;

    const Match &m = m_matches[idx];
    if (m.tabIndex >= 0)
        m_frame->GoToTabAndLine(m.tabIndex, m.line);
    else
        m_frame->OpenFilePathAndGoToLine(m.path, m.line);
}

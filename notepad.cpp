#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/filename.h>
#include <wx/stc/stc.h>
#include <wx/print.h>
#include <wx/aboutdlg.h>

// Simple printout that paginates plain text across pages using the
// device context's own text-measurement, so it scales to any paper size.
class TextPrintout : public wxPrintout
{
public:
    TextPrintout(const wxString &text, const wxString &title)
        : wxPrintout(title), m_text(text)
    {
        m_lines = wxSplit(m_text, '\n');
    }

    bool OnPrintPage(int page) override
    {
        wxDC *dc = GetDC();
        if (!dc) return false;

        dc->SetFont(wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        int lineHeight = dc->GetCharHeight() + 2;

        int firstLine = (page - 1) * m_linesPerPage;
        int lastLine = wxMin(firstLine + m_linesPerPage, (int)m_lines.size());

        int y = 0;
        for (int i = firstLine; i < lastLine; i++)
        {
            dc->DrawText(m_lines[i], 0, y);
            y += lineHeight;
        }
        return true;
    }

    void GetPageInfo(int *minPage, int *maxPage, int *pageFrom, int *pageTo) override
    {
        wxDC *dc = GetDC();
        if (!dc)
        {
            *minPage = *maxPage = *pageFrom = *pageTo = 1;
            return;
        }

        int w = 0, h = 0;
        dc->GetSize(&w, &h);
        dc->SetFont(wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        int lineHeight = dc->GetCharHeight() + 2;
        if (lineHeight <= 0) lineHeight = 1;
        m_linesPerPage = wxMax(1, h / lineHeight);

        int pages = wxMax(1, (int)((m_lines.size() + m_linesPerPage - 1) / m_linesPerPage));
        *minPage = 1;
        *maxPage = pages;
        *pageFrom = 1;
        *pageTo = pages;
    }

    bool HasPage(int page) override
    {
        return page <= wxMax(1, (int)((m_lines.size() + m_linesPerPage - 1) / m_linesPerPage));
    }

private:
    wxString m_text;
    wxArrayString m_lines;
    int m_linesPerPage = 1;
};

enum
{
    ID_NewTab = wxID_HIGHEST + 1,
    ID_CloseTab
};

class NotepadFrame : public wxFrame
{
public:
    NotepadFrame() : wxFrame(nullptr, wxID_ANY, "Notepad", wxDefaultPosition, wxSize(800, 600))
    {
        wxMenuBar *menuBar = new wxMenuBar();

        wxMenu *fileMenu = new wxMenu();
        fileMenu->Append(ID_NewTab, "New Tab\tCtrl+T");
        fileMenu->Append(wxID_OPEN, "Open...\tCtrl+O");
        fileMenu->Append(wxID_SAVE, "Save\tCtrl+S");
        fileMenu->Append(wxID_SAVEAS, "Save As...\tCtrl+Shift+S");
        fileMenu->AppendSeparator();
        fileMenu->Append(wxID_PRINT, "Print...\tCtrl+P");
        fileMenu->AppendSeparator();
        fileMenu->Append(ID_CloseTab, "Close Tab\tCtrl+W");
        fileMenu->AppendSeparator();
        fileMenu->Append(wxID_EXIT, "Exit\tAlt+F4");
        menuBar->Append(fileMenu, "&File");

        wxMenu *editMenu = new wxMenu();
        editMenu->Append(wxID_UNDO, "Undo\tCtrl+Z");
        editMenu->Append(wxID_REDO, "Redo\tCtrl+Y");
        editMenu->AppendSeparator();
        editMenu->Append(wxID_CUT, "Cut\tCtrl+X");
        editMenu->Append(wxID_COPY, "Copy\tCtrl+C");
        editMenu->Append(wxID_PASTE, "Paste\tCtrl+V");
        editMenu->AppendSeparator();
        editMenu->Append(wxID_SELECTALL, "Select All\tCtrl+A");
        menuBar->Append(editMenu, "&Edit");

        wxMenu *helpMenu = new wxMenu();
        helpMenu->Append(wxID_ABOUT, "About Notepad...");
        menuBar->Append(helpMenu, "&Help");

        SetMenuBar(menuBar);
        CreateStatusBar();

        m_notebook = new wxNotebook(this, wxID_ANY);
        AddTab("Untitled");

        Bind(wxEVT_MENU, &NotepadFrame::OnNewTab, this, ID_NewTab);
        Bind(wxEVT_MENU, &NotepadFrame::OnCloseTab, this, ID_CloseTab);
        Bind(wxEVT_MENU, &NotepadFrame::OnOpen, this, wxID_OPEN);
        Bind(wxEVT_MENU, &NotepadFrame::OnSave, this, wxID_SAVE);
        Bind(wxEVT_MENU, &NotepadFrame::OnSaveAs, this, wxID_SAVEAS);
        Bind(wxEVT_MENU, &NotepadFrame::OnExit, this, wxID_EXIT);
        Bind(wxEVT_MENU, &NotepadFrame::OnUndo, this, wxID_UNDO);
        Bind(wxEVT_MENU, &NotepadFrame::OnRedo, this, wxID_REDO);
        Bind(wxEVT_MENU, &NotepadFrame::OnCut, this, wxID_CUT);
        Bind(wxEVT_MENU, &NotepadFrame::OnCopy, this, wxID_COPY);
        Bind(wxEVT_MENU, &NotepadFrame::OnPaste, this, wxID_PASTE);
        Bind(wxEVT_MENU, &NotepadFrame::OnSelectAll, this, wxID_SELECTALL);
        Bind(wxEVT_MENU, &NotepadFrame::OnPrint, this, wxID_PRINT);
        Bind(wxEVT_MENU, &NotepadFrame::OnAbout, this, wxID_ABOUT);
        Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &NotepadFrame::OnPageChanged, this);
        Bind(wxEVT_CLOSE_WINDOW, &NotepadFrame::OnCloseWindow, this);
    }

private:
    struct TabData
    {
        wxString filePath;
        bool modified = false;
    };

    wxNotebook *m_notebook;
    std::vector<TabData> m_tabData;

    wxStyledTextCtrl* CurrentText()
    {
        int sel = m_notebook->GetSelection();
        if (sel == wxNOT_FOUND) return nullptr;
        return (wxStyledTextCtrl*)m_notebook->GetPage(sel);
    }

    wxStyledTextCtrl* PageText(int index)
    {
        return (wxStyledTextCtrl*)m_notebook->GetPage(index);
    }

    void SetupEditor(wxStyledTextCtrl *stc)
    {
        stc->SetLexer(wxSTC_LEX_NULL);
        stc->StyleSetFont(wxSTC_STYLE_DEFAULT,
            wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

        // Line number margin
        stc->SetMarginType(0, wxSTC_MARGIN_NUMBER);
        stc->SetMarginWidth(0, stc->TextWidth(wxSTC_STYLE_LINENUMBER, "_99999"));
        stc->SetMarginWidth(1, 0); // hide folding/symbol margin

        stc->SetTabWidth(4);
        stc->SetUseTabs(false);
        stc->SetWrapMode(wxSTC_WRAP_NONE);
    }

    // Grows the line-number margin as the document gains more digits
    void UpdateMarginWidth(wxStyledTextCtrl *stc)
    {
        int lines = stc->GetLineCount();
        wxString widest = wxString::Format("_%d", lines);
        stc->SetMarginWidth(0, stc->TextWidth(wxSTC_STYLE_LINENUMBER, widest));
    }

    void AddTab(const wxString &title, const wxString &content = "", const wxString &filePath = "")
    {
        // Push tab metadata BEFORE AddPage, since AddPage synchronously fires
        // a page-changed event whose handler reads m_tabData by index.
        m_tabData.push_back({filePath, false});

        wxStyledTextCtrl *stc = new wxStyledTextCtrl(m_notebook, wxID_ANY);
        SetupEditor(stc);
        if (!content.IsEmpty())
            stc->SetText(content);
        stc->EmptyUndoBuffer();

        stc->Bind(wxEVT_STC_MODIFIED, &NotepadFrame::OnTextModified, this);
        stc->Bind(wxEVT_STC_CHANGE, &NotepadFrame::OnLineCountChange, this);

        m_notebook->AddPage(stc, title, true);
        UpdateMarginWidth(stc);
        UpdateTitle();
    }

    void UpdateTabLabel(int index)
    {
        wxString label = m_tabData[index].filePath.IsEmpty()
            ? "Untitled" : wxFileName(m_tabData[index].filePath).GetFullName();
        if (m_tabData[index].modified) label += " *";
        m_notebook->SetPageText(index, label);
    }

    void UpdateTitle()
    {
        int sel = m_notebook->GetSelection();
        if (sel == wxNOT_FOUND || sel >= (int)m_tabData.size()) { SetTitle("Notepad"); return; }
        wxString name = m_tabData[sel].filePath.IsEmpty()
            ? "Untitled" : wxFileName(m_tabData[sel].filePath).GetFullName();
        SetTitle(name + " - Notepad");
    }

    int IndexOf(wxStyledTextCtrl *stc)
    {
        for (size_t i = 0; i < m_notebook->GetPageCount(); i++)
            if (m_notebook->GetPage(i) == stc) return (int)i;
        return wxNOT_FOUND;
    }

    void OnTextModified(wxStyledTextEvent &event)
    {
        wxStyledTextCtrl *stc = (wxStyledTextCtrl*)event.GetEventObject();
        int index = IndexOf(stc);
        if (index != wxNOT_FOUND && !m_tabData[index].modified)
        {
            m_tabData[index].modified = true;
            UpdateTabLabel(index);
        }
        event.Skip();
    }

    void OnLineCountChange(wxStyledTextEvent &event)
    {
        wxStyledTextCtrl *stc = (wxStyledTextCtrl*)event.GetEventObject();
        UpdateMarginWidth(stc);
        event.Skip();
    }

    void OnPageChanged(wxBookCtrlEvent &event)
    {
        UpdateTitle();
        event.Skip();
    }

    void OnNewTab(wxCommandEvent &event)
    {
        AddTab("Untitled");
    }

    void OnCloseTab(wxCommandEvent &event)
    {
        CloseTab(m_notebook->GetSelection());
    }

    bool CloseTab(int index, bool addNewIfEmpty = true)
    {
        if (index == wxNOT_FOUND) return true;

        if (m_tabData[index].modified)
        {
            int result = wxMessageBox(
                "Save changes before closing this tab?",
                "Unsaved Changes", wxYES_NO | wxCANCEL | wxICON_QUESTION, this);
            if (result == wxCANCEL) return false;
            if (result == wxYES)
            {
                m_notebook->SetSelection(index);
                if (!SaveTab(index)) return false;
            }
        }

        m_notebook->DeletePage(index);
        m_tabData.erase(m_tabData.begin() + index);

        if (addNewIfEmpty && m_notebook->GetPageCount() == 0)
            AddTab("Untitled");

        UpdateTitle();
        return true;
    }

    bool SaveTab(int index)
    {
        wxStyledTextCtrl *stc = PageText(index);

        if (m_tabData[index].filePath.IsEmpty())
        {
            wxFileDialog dlg(this, "Save As", "", "", "Text files (*.txt)|*.txt|All files (*.*)|*.*",
                wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
            if (dlg.ShowModal() == wxID_CANCEL) return false;
            m_tabData[index].filePath = dlg.GetPath();
        }

        wxFile file(m_tabData[index].filePath, wxFile::write);
        if (!file.IsOpened() || !file.Write(stc->GetText()))
        {
            wxMessageBox("Failed to save file.", "Error", wxOK | wxICON_ERROR, this);
            return false;
        }

        m_tabData[index].modified = false;
        UpdateTabLabel(index);
        UpdateTitle();
        return true;
    }

    void OnOpen(wxCommandEvent &event)
    {
        wxFileDialog dlg(this, "Open File", "", "", "Text files (*.txt)|*.txt|All files (*.*)|*.*",
            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() == wxID_CANCEL) return;

        wxString path = dlg.GetPath();
        wxString content;
        wxFile file(path);
        if (file.IsOpened())
            file.ReadAll(&content);

        AddTab(wxFileName(path).GetFullName(), content, path);
    }

    void OnSave(wxCommandEvent &event)
    {
        SaveTab(m_notebook->GetSelection());
    }

    void OnSaveAs(wxCommandEvent &event)
    {
        int sel = m_notebook->GetSelection();
        if (sel == wxNOT_FOUND) return;
        m_tabData[sel].filePath = "";
        SaveTab(sel);
    }

    void OnExit(wxCommandEvent &event)
    {
        Close();
    }

    void OnCloseWindow(wxCloseEvent &event)
    {
        while (m_notebook->GetPageCount() > 0)
        {
            if (!CloseTab(0, false)) // don't re-add "Untitled" while shutting down
            {
                event.Veto();
                return;
            }
        }
        Destroy();
    }

    void OnUndo(wxCommandEvent &event) { if (auto *t = CurrentText()) t->Undo(); }
    void OnRedo(wxCommandEvent &event) { if (auto *t = CurrentText()) t->Redo(); }
    void OnCut(wxCommandEvent &event) { if (auto *t = CurrentText()) t->Cut(); }
    void OnCopy(wxCommandEvent &event) { if (auto *t = CurrentText()) t->Copy(); }
    void OnPaste(wxCommandEvent &event) { if (auto *t = CurrentText()) t->Paste(); }
    void OnSelectAll(wxCommandEvent &event) { if (auto *t = CurrentText()) t->SelectAll(); }

    void OnPrint(wxCommandEvent &event)
    {
        wxStyledTextCtrl *stc = CurrentText();
        if (!stc) return;

        int sel = m_notebook->GetSelection();
        wxString title = m_tabData[sel].filePath.IsEmpty()
            ? "Untitled" : wxFileName(m_tabData[sel].filePath).GetFullName();

        wxPrintDialogData printDialogData;
        wxPrinter printer(&printDialogData);
        TextPrintout printout(stc->GetText(), title);

        if (!printer.Print(this, &printout, true) &&
            printer.GetLastError() == wxPRINTER_ERROR)
        {
            wxMessageBox("There was an error printing.\n"
                "Check your printer setup.", "Printing Error", wxOK | wxICON_ERROR, this);
        }
    }

    void OnAbout(wxCommandEvent &event)
    {
        wxAboutDialogInfo info;
        info.SetName("Notepad");
        info.SetVersion("1.0");
        info.SetDescription("A simple multi-tab text editor built with wxWidgets.");
        info.SetCopyright("(C) 2026");
        info.SetLicense(
            "MIT License\n\n"
            "Permission is hereby granted, free of charge, to any person obtaining a copy "
            "of this software and associated documentation files (the \"Software\"), to deal "
            "in the Software without restriction, including without limitation the rights "
            "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell "
            "copies of the Software, and to permit persons to whom the Software is "
            "furnished to do so, subject to the following conditions:\n\n"
            "The above copyright notice and this permission notice shall be included in all "
            "copies or substantial portions of the Software.\n\n"
            "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR "
            "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, "
            "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE "
            "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER "
            "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, "
            "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE "
            "SOFTWARE."
        );
        wxAboutBox(info, this);
    }
};

class NotepadApp : public wxApp
{
public:
    bool OnInit() override
    {
        NotepadFrame *frame = new NotepadFrame();
        frame->Show();
        return true;
    }
};

wxIMPLEMENT_APP(NotepadApp);

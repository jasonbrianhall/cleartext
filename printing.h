#pragma once

#include <wx/print.h>
#include <wx/string.h>

// Simple printout that paginates plain text across pages using the device
// context's own text-measurement, so it scales to any paper size.
class TextPrintout : public wxPrintout
{
public:
    TextPrintout(const wxString &text, const wxString &title);

    void OnPreparePrinting() override;
    bool OnPrintPage(int page) override;
    void GetPageInfo(int *minPage, int *maxPage, int *pageFrom, int *pageTo) override;
    bool HasPage(int page) override;

private:
    wxString m_text;
    wxArrayString m_lines;
    int m_linesPerPage = 1;
    int m_fontPointSize = 14;
    float m_scale = 1.0f;
};

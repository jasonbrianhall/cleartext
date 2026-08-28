#include "printing.h"

TextPrintout::TextPrintout(const wxString &text, const wxString &title)
    : wxPrintout(title), m_text(text)
{
    m_lines = wxSplit(m_text, '\n');
}

void TextPrintout::OnPreparePrinting()
{
    wxDC *dc = GetDC();
    if (!dc) return;

    // Printer DCs run at a much higher DPI than the screen, so without
    // scaling, point sizes come out tiny relative to the page. Scale so
    // that our font size maps to its true physical size on paper.
    wxSize ppiPrinter, ppiScreen;
    GetPPIPrinter(&ppiPrinter.x, &ppiPrinter.y);
    GetPPIScreen(&ppiScreen.x, &ppiScreen.y);
    if (ppiScreen.x == 0) ppiScreen = wxSize(96, 96);
    float scale = (float)ppiPrinter.x / (float)ppiScreen.x;

    int pageW, pageH;
    GetPageSizePixels(&pageW, &pageH);
    wxSize dcSize = dc->GetSize();
    float pageScale = pageW > 0 ? (float)dcSize.x / (float)pageW : 1.0f;

    m_scale = scale * pageScale;
    dc->SetUserScale(m_scale, m_scale);
}

bool TextPrintout::OnPrintPage(int page)
{
    wxDC *dc = GetDC();
    if (!dc) return false;

    dc->SetFont(wxFont(m_fontPointSize, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    int lineHeight = dc->GetCharHeight() + 4;

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

void TextPrintout::GetPageInfo(int *minPage, int *maxPage, int *pageFrom, int *pageTo)
{
    wxDC *dc = GetDC();
    if (!dc)
    {
        *minPage = *maxPage = *pageFrom = *pageTo = 1;
        return;
    }

    // Usable page height in the DC's *scaled* (logical) units.
    int pageW, pageH;
    GetPageSizePixels(&pageW, &pageH);
    int usableH = m_scale > 0 ? (int)(pageH / m_scale) : pageH;

    dc->SetFont(wxFont(m_fontPointSize, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    int lineHeight = dc->GetCharHeight() + 4;
    if (lineHeight <= 0) lineHeight = 1;
    m_linesPerPage = wxMax(1, usableH / lineHeight);

    int pages = wxMax(1, (int)((m_lines.size() + m_linesPerPage - 1) / m_linesPerPage));
    *minPage = 1;
    *maxPage = pages;
    *pageFrom = 1;
    *pageTo = pages;
}

bool TextPrintout::HasPage(int page)
{
    return page <= wxMax(1, (int)((m_lines.size() + m_linesPerPage - 1) / m_linesPerPage));
}

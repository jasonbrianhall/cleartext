#include "copy_html.h"
#include <wx/stc/stc.h>

namespace
{
    wxString ColourToHex(const wxColour &c)
    {
        return wxString::Format("#%02x%02x%02x", c.Red(), c.Green(), c.Blue());
    }

    wxString EscapeHtml(const wxString &text)
    {
        wxString out;
        out.Alloc(text.length());
        for (size_t i = 0; i < text.length(); i++)
        {
            wxUniChar ch = text[i];
            if (ch == '&') out << "&amp;";
            else if (ch == '<') out << "&lt;";
            else if (ch == '>') out << "&gt;";
            else out << ch;
        }
        return out;
    }
}

wxString BuildHtmlFromStc(wxStyledTextCtrl *stc)
{
    bool hasSelection = stc->GetSelectionStart() != stc->GetSelectionEnd();
    int start = hasSelection ? stc->GetSelectionStart() : 0;
    int end = hasSelection ? stc->GetSelectionEnd() : stc->GetTextLength();

    // Read back what's actually on screen right now -- the current theme's
    // default colors/font -- rather than reaching into highlighting.cpp, so
    // this always matches exactly regardless of which theme is active.
    wxColour bg = stc->StyleGetBackground(wxSTC_STYLE_DEFAULT);
    wxColour fg = stc->StyleGetForeground(wxSTC_STYLE_DEFAULT);
    wxFont font = stc->StyleGetFont(wxSTC_STYLE_DEFAULT);
    int fontSize = font.GetPointSize();
    wxString fontFace = font.GetFaceName();
    wxString fontFamily = fontFace.IsEmpty()
        ? "Consolas, 'Courier New', monospace"
        : "'" + fontFace + "', Consolas, 'Courier New', monospace";

    wxString pre;
    pre << "<pre style=\"background-color:" << ColourToHex(bg)
        << ";color:" << ColourToHex(fg)
        << ";font-family:" << fontFamily
        << ";font-size:" << fontSize << "pt"
        << ";tab-size:4;-moz-tab-size:4"
        << ";white-space:pre-wrap;word-wrap:break-word"
        << ";padding:8px;margin:0;\">";

    int pos = start;
    while (pos < end)
    {
        int style = stc->GetStyleAt(pos);
        wxColour runFg = stc->StyleGetForeground(style);
        bool runBold = stc->StyleGetBold(style);
        bool runItalic = stc->StyleGetItalic(style);

        // Merge by actual resolved appearance, not raw style id: many
        // lexers assign distinct style numbers to token kinds (e.g.
        // identifiers vs. operators) that happen to render identically in
        // a given theme, and splitting those into separate spans would
        // just bloat the markup for no visual difference.
        int runStart = pos;
        pos = stc->PositionAfter(pos);
        while (pos < end)
        {
            int nextStyle = stc->GetStyleAt(pos);
            if (stc->StyleGetForeground(nextStyle) != runFg ||
                stc->StyleGetBold(nextStyle) != runBold ||
                stc->StyleGetItalic(nextStyle) != runItalic)
                break;
            pos = stc->PositionAfter(pos);
        }

        pre << "<span style=\"color:" << ColourToHex(runFg);
        if (runBold) pre << ";font-weight:bold";
        if (runItalic) pre << ";font-style:italic";
        pre << "\">" << EscapeHtml(stc->GetTextRange(runStart, pos)) << "</span>";
    }

    pre << "</pre>";

    wxString doc;
    doc << "<!DOCTYPE html>\n"
        << "<html lang=\"en\">\n"
        << "<head>\n"
        << "<meta charset=\"utf-8\">\n"
        << "<title>ClearText Export</title>\n"
        << "</head>\n"
        << "<body style=\"margin:0;background-color:" << ColourToHex(bg) << ";\">\n"
        << pre << "\n"
        << "</body>\n"
        << "</html>\n";
    return doc;
}

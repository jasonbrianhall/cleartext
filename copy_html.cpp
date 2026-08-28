#include "copy_html.h"
#include <wx/stc/stc.h>
#include <vector>

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

    bool IsUrlWordCharByte(char c)
    {
        return wxIsalnum((unsigned char)c) || c == '_';
    }

    bool IsUrlTerminatorByte(char c)
    {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
               c == '<' || c == '>' || c == '"' || c == '\'';
    }

    // Trailing punctuation that's almost always sentence/code punctuation
    // rather than part of the URL -- e.g. the ')' closing "(see
    // https://example.com)" or the '.' ending a sentence.
    bool IsUrlTrailingPunctByte(char c)
    {
        return c == '.' || c == ',' || c == ';' || c == ':' || c == '!' ||
               c == '?' || c == ')' || c == ']' || c == '}';
    }

    struct UrlRange { int start; int end; };

    // Finds every http(s):// URL in [rangeStart, rangeEnd), scanning at the
    // Scintilla document-position level rather than within already-split
    // style runs -- the lexer often assigns several different styles across
    // a single URL (scheme, "://", host, ...), so detecting URLs before any
    // style-based splitting is what lets one URL become one link even when
    // it's rendered in multiple colors.
    std::vector<UrlRange> FindUrls(wxStyledTextCtrl *stc, int rangeStart, int rangeEnd)
    {
        std::vector<UrlRange> urls;
        int pos = rangeStart;
        while (pos < rangeEnd)
        {
            wxString head = stc->GetTextRange(pos, wxMin(pos + 8, rangeEnd));
            bool isHttps = head.StartsWith("https://");
            bool isHttp = !isHttps && head.StartsWith("http://");
            if (isHttp || isHttps)
            {
                bool boundaryOk = true;
                if (pos > rangeStart)
                {
                    int prev = stc->PositionBefore(pos);
                    if (IsUrlWordCharByte((char)stc->GetCharAt(prev)))
                        boundaryOk = false;
                }
                if (boundaryOk)
                {
                    int urlEnd = pos + (isHttps ? 8 : 7);
                    while (urlEnd < rangeEnd && !IsUrlTerminatorByte((char)stc->GetCharAt(urlEnd)))
                        urlEnd = stc->PositionAfter(urlEnd);
                    while (urlEnd > pos)
                    {
                        int prevPos = stc->PositionBefore(urlEnd);
                        if (!IsUrlTrailingPunctByte((char)stc->GetCharAt(prevPos))) break;
                        urlEnd = prevPos;
                    }
                    urls.push_back({pos, urlEnd});
                    pos = urlEnd;
                    continue;
                }
            }
            pos = stc->PositionAfter(pos);
        }
        return urls;
    }

    // Emits [segStart, segEnd) as one or more <span>s, merged by resolved
    // appearance exactly as before -- used both for plain text and for the
    // text inside a link, so a URL that crosses style boundaries still gets
    // colored per-token inside its single enclosing <a>.
    void EmitStyledSpans(wxStyledTextCtrl *stc, wxString &pre, int segStart, int segEnd)
    {
        int pos = segStart;
        while (pos < segEnd)
        {
            int style = stc->GetStyleAt(pos);
            wxColour runFg = stc->StyleGetForeground(style);
            bool runBold = stc->StyleGetBold(style);
            bool runItalic = stc->StyleGetItalic(style);

            int runStart = pos;
            pos = stc->PositionAfter(pos);
            while (pos < segEnd)
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

    std::vector<UrlRange> urls = FindUrls(stc, start, end);

    int pos = start;
    size_t urlIdx = 0;
    while (pos < end)
    {
        if (urlIdx < urls.size() && urls[urlIdx].start == pos)
        {
            int urlEnd = urls[urlIdx].end;
            wxString href = EscapeHtml(stc->GetTextRange(pos, urlEnd));
            pre << "<a href=\"" << href << "\" style=\"text-decoration:underline;\">";
            EmitStyledSpans(stc, pre, pos, urlEnd);
            pre << "</a>";
            pos = urlEnd;
            urlIdx++;
        }
        else
        {
            int segEnd = (urlIdx < urls.size()) ? urls[urlIdx].start : end;
            EmitStyledSpans(stc, pre, pos, segEnd);
            pos = segEnd;
        }
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

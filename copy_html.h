#pragma once

#include <wx/string.h>

class wxStyledTextCtrl;

// Builds a standalone HTML document reproducing the current selection (or,
// if there's no selection, the whole document) exactly as it's displayed --
// same theme colors, background, bold/italic -- by reading back the styles
// Scintilla already resolved via ApplyHighlighting (see highlighting.h)
// rather than recomputing anything itself. Used for Edit > Copy as HTML.
wxString BuildHtmlFromStc(wxStyledTextCtrl *stc);

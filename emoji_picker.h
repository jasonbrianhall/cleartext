#pragma once

#include <wx/string.h>

class wxWindow;

// Modal picker for inserting an emoji: a searchable, categorized grid of
// common emoji. Returns the chosen emoji glyph, or an empty string if the
// dialog was cancelled. Used by ClearTextFrame's editor context menu (see
// editor_frame.cpp's OnEditorContextMenu) for right-click "Insert Emoji...".
wxString ShowEmojiPicker(wxWindow *parent);

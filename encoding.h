#pragma once

#include <wx/string.h>

// Robust text file I/O: detects a byte-order mark (UTF-8/UTF-16LE/UTF-16BE)
// on read and falls back sensibly when there isn't one, instead of decoding
// with whatever the platform's default locale conversion happens to be
// (which is what wxFile::ReadAll's implicit conversion does, and which can
// silently mangle non-ASCII text depending on the OS/locale).
namespace TextEncoding
{
    // Reads `path`'s entire contents as text. Returns false only on an I/O
    // failure (file missing/unreadable) -- decoding itself always succeeds,
    // falling back to Latin-1 if the bytes aren't valid UTF-8, so no file
    // comes back empty or triggers an error purely from its encoding.
    bool ReadFile(const wxString &path, wxString &outContent);

    // Writes `content` to `path` as UTF-8 without a byte-order mark --
    // ClearText's on-disk save format, regardless of how the file was
    // originally encoded when it was opened.
    bool WriteFile(const wxString &path, const wxString &content);
}

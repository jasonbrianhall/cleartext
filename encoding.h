#pragma once

#include <wx/string.h>
#include <vector>

// Robust text file I/O: detects a byte-order mark (UTF-8/UTF-16LE/UTF-16BE)
// on read and falls back sensibly when there isn't one, instead of decoding
// with whatever the platform's default locale conversion happens to be
// (which is what wxFile::ReadAll's implicit conversion does, and which can
// silently mangle non-ASCII text depending on the OS/locale). The guess can
// be wrong -- e.g. a Latin-1 file with no accented characters in the first
// few KB looks identical to ASCII/UTF-8 -- so callers can also request a
// specific encoding explicitly (see ReadFileAs), surfaced as the View >
// Encoding menu in editor_frame.cpp.
namespace TextEncoding
{
    enum class Encoding
    {
        Auto = 0, // not a real encoding -- means "guess via BOM/UTF-8 validity" (see ReadFile)
        Utf8,
        Utf16LE,
        Utf16BE,
        Latin1,
    };

    // Every selectable value in menu order, Auto first -- used to build the
    // View > Encoding menu and to resolve a menu item id back to an
    // Encoding, the same way highlighting.h's AllLanguages() works for the
    // Language menu.
    const std::vector<Encoding> &AllEncodings();

    // Display name for `enc`, e.g. "UTF-8" -- used for the Encoding menu.
    // Encoding::Auto returns "Auto-Detect".
    wxString EncodingDisplayName(Encoding enc);

    // Reads `path`'s entire contents as text. Returns false only on an I/O
    // failure (file missing/unreadable) -- decoding itself always succeeds,
    // falling back to Latin-1 if the bytes aren't valid UTF-8, so no file
    // comes back empty or triggers an error purely from its encoding.
    // `outDetected`, if non-null, receives which concrete Encoding
    // (never Auto) was actually used to decode the bytes.
    bool ReadFile(const wxString &path, wxString &outContent, Encoding *outDetected = nullptr);

    // Reads `path`'s entire contents as text using exactly `encoding`
    // (which must be a concrete encoding, not Auto), ignoring auto-
    // detection entirely -- used when the user has explicitly picked a
    // tab's encoding from View > Encoding because ReadFile's guess came
    // out wrong. A byte-order mark matching `encoding` is stripped; any
    // other leading bytes are decoded as ordinary content.
    bool ReadFileAs(const wxString &path, Encoding encoding, wxString &outContent);

    // Writes `content` to `path` encoded as `encoding` -- UTF-16LE/BE are
    // written with a leading byte-order mark (there's no other way to
    // signal which one it is); UTF-8 and Latin-1 are written without one.
    // Defaults to UTF-8, ClearText's original on-disk format, when the
    // caller doesn't care.
    bool WriteFile(const wxString &path, const wxString &content, Encoding encoding = Encoding::Utf8);
}

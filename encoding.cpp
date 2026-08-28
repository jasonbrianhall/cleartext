#include "encoding.h"
#include <wx/file.h>
#include <wx/strconv.h>
#include <vector>

namespace TextEncoding
{

bool ReadFile(const wxString &path, wxString &outContent)
{
    wxFile file(path);
    if (!file.IsOpened()) return false;

    wxFileOffset len = file.Length();
    if (len <= 0)
    {
        outContent.Clear();
        return true;
    }

    std::vector<char> buf((size_t)len);
    ssize_t read = file.Read(buf.data(), (size_t)len);
    if (read < 0) return false;
    size_t n = (size_t)read;

    const unsigned char *bytes = (const unsigned char*)buf.data();

    // UTF-8 BOM: EF BB BF
    if (n >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
    {
        outContent = wxString(buf.data() + 3, wxConvUTF8, n - 3);
        return true;
    }
    // UTF-16LE BOM: FF FE (but not the 4-byte UTF-32LE BOM FF FE 00 00)
    if (n >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE &&
        !(n >= 4 && bytes[2] == 0x00 && bytes[3] == 0x00))
    {
        outContent = wxString(buf.data() + 2, wxMBConvUTF16LE(), n - 2);
        return true;
    }
    // UTF-16BE BOM: FE FF
    if (n >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF)
    {
        outContent = wxString(buf.data() + 2, wxMBConvUTF16BE(), n - 2);
        return true;
    }

    // No BOM: try strict UTF-8 first (the common case for source files and
    // most modern text), falling back to Latin-1 -- which never fails,
    // since every byte maps to a character -- so legacy-encoded files still
    // load instead of coming back empty or visibly corrupted.
    if (wxConvUTF8.ToWChar(nullptr, 0, buf.data(), n) != wxCONV_FAILED)
        outContent = wxString(buf.data(), wxConvUTF8, n);
    else
        outContent = wxString(buf.data(), wxConvISO8859_1, n);

    return true;
}

bool WriteFile(const wxString &path, const wxString &content)
{
    wxFile file(path, wxFile::write);
    if (!file.IsOpened()) return false;
    return file.Write(content, wxConvUTF8);
}

} // namespace TextEncoding

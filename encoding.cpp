#include "encoding.h"
#include <wx/file.h>
#include <wx/strconv.h>
#include <vector>

namespace TextEncoding
{

const std::vector<Encoding> &AllEncodings()
{
    static const std::vector<Encoding> encodings = {
        Encoding::Auto,
        Encoding::Utf8,
        Encoding::Utf16LE,
        Encoding::Utf16BE,
        Encoding::Latin1,
    };
    return encodings;
}

wxString EncodingDisplayName(Encoding enc)
{
    switch (enc)
    {
        case Encoding::Utf8:    return "UTF-8";
        case Encoding::Utf16LE: return "UTF-16 LE";
        case Encoding::Utf16BE: return "UTF-16 BE";
        case Encoding::Latin1:  return "Western (Latin-1)";
        case Encoding::Auto:
        default:                return "Auto-Detect";
    }
}

namespace
{
    bool ReadRaw(const wxString &path, std::vector<char> &outBuf)
    {
        wxFile file(path);
        if (!file.IsOpened()) return false;

        wxFileOffset len = file.Length();
        if (len <= 0)
        {
            outBuf.clear();
            return true;
        }

        outBuf.resize((size_t)len);
        ssize_t read = file.Read(outBuf.data(), (size_t)len);
        if (read < 0) return false;
        outBuf.resize((size_t)read);
        return true;
    }

    // Decodes `buf`[0..n) as `encoding`, stripping a matching byte-order
    // mark if present at the very start. `encoding` must be concrete (not
    // Auto). Never fails -- Latin-1 accepts any byte sequence, and wx's
    // UTF-8/UTF-16 conversions substitute rather than throw on invalid
    // input, so a wrong-on-purpose choice still shows *something* rather
    // than erroring out (that's the point: the user picks a different one
    // and sees whether it looks right).
    wxString DecodeAs(const char *buf, size_t n, Encoding encoding)
    {
        const unsigned char *bytes = (const unsigned char*)buf;
        switch (encoding)
        {
            case Encoding::Utf16LE:
                if (n >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE)
                    return wxString(buf + 2, wxMBConvUTF16LE(), n - 2);
                return wxString(buf, wxMBConvUTF16LE(), n);

            case Encoding::Utf16BE:
                if (n >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF)
                    return wxString(buf + 2, wxMBConvUTF16BE(), n - 2);
                return wxString(buf, wxMBConvUTF16BE(), n);

            case Encoding::Latin1:
                return wxString(buf, wxConvISO8859_1, n);

            case Encoding::Utf8:
            case Encoding::Auto: // shouldn't happen; treat like UTF-8
            default:
                if (n >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
                    return wxString(buf + 3, wxConvUTF8, n - 3);
                return wxString(buf, wxConvUTF8, n);
        }
    }
}

bool ReadFile(const wxString &path, wxString &outContent, Encoding *outDetected)
{
    std::vector<char> buf;
    if (!ReadRaw(path, buf)) return false;

    size_t n = buf.size();
    if (n == 0)
    {
        outContent.Clear();
        if (outDetected) *outDetected = Encoding::Utf8;
        return true;
    }

    const unsigned char *bytes = (const unsigned char*)buf.data();

    // UTF-8 BOM: EF BB BF
    if (n >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
    {
        outContent = wxString(buf.data() + 3, wxConvUTF8, n - 3);
        if (outDetected) *outDetected = Encoding::Utf8;
        return true;
    }
    // UTF-16LE BOM: FF FE (but not the 4-byte UTF-32LE BOM FF FE 00 00)
    if (n >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE &&
        !(n >= 4 && bytes[2] == 0x00 && bytes[3] == 0x00))
    {
        outContent = wxString(buf.data() + 2, wxMBConvUTF16LE(), n - 2);
        if (outDetected) *outDetected = Encoding::Utf16LE;
        return true;
    }
    // UTF-16BE BOM: FE FF
    if (n >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF)
    {
        outContent = wxString(buf.data() + 2, wxMBConvUTF16BE(), n - 2);
        if (outDetected) *outDetected = Encoding::Utf16BE;
        return true;
    }

    // No BOM: try strict UTF-8 first (the common case for source files and
    // most modern text), falling back to Latin-1 -- which never fails,
    // since every byte maps to a character -- so legacy-encoded files still
    // load instead of coming back empty or visibly corrupted. This is also
    // exactly the guess that can come out wrong (e.g. a Latin-1 file with
    // no non-ASCII bytes in it is indistinguishable from UTF-8), which is
    // what View > Encoding's explicit override is for.
    if (wxConvUTF8.ToWChar(nullptr, 0, buf.data(), n) != wxCONV_FAILED)
    {
        outContent = wxString(buf.data(), wxConvUTF8, n);
        if (outDetected) *outDetected = Encoding::Utf8;
    }
    else
    {
        outContent = wxString(buf.data(), wxConvISO8859_1, n);
        if (outDetected) *outDetected = Encoding::Latin1;
    }

    return true;
}

bool ReadFileAs(const wxString &path, Encoding encoding, wxString &outContent)
{
    std::vector<char> buf;
    if (!ReadRaw(path, buf)) return false;

    if (buf.empty())
    {
        outContent.Clear();
        return true;
    }

    outContent = DecodeAs(buf.data(), buf.size(), encoding);
    return true;
}

namespace
{
    // Writes `bom` followed by `content` re-encoded via `conv` (a 16-bit
    // conversion). Used for the two UTF-16 variants, which are otherwise
    // indistinguishable on disk without one.
    bool WriteWithBom(wxFile &file, const unsigned char (&bom)[2], const wxString &content, wxMBConv &conv)
    {
        if (file.Write(bom, 2) != 2) return false;
        if (content.IsEmpty()) return true;

        const wxScopedCharBuffer buf = content.mb_str(conv);
        size_t len = buf.length();
        if (len == 0) return true;
        return file.Write(buf.data(), len) == len;
    }
}

bool WriteFile(const wxString &path, const wxString &content, Encoding encoding)
{
    wxFile file(path, wxFile::write);
    if (!file.IsOpened()) return false;

    switch (encoding)
    {
        case Encoding::Utf16LE:
        {
            static const unsigned char bom[2] = {0xFF, 0xFE};
            wxMBConvUTF16LE conv;
            return WriteWithBom(file, bom, content, conv);
        }
        case Encoding::Utf16BE:
        {
            static const unsigned char bom[2] = {0xFE, 0xFF};
            wxMBConvUTF16BE conv;
            return WriteWithBom(file, bom, content, conv);
        }
        case Encoding::Latin1:
            return file.Write(content, wxConvISO8859_1);

        case Encoding::Utf8:
        case Encoding::Auto: // shouldn't happen; treat like UTF-8
        default:
            return file.Write(content, wxConvUTF8);
    }
}

} // namespace TextEncoding

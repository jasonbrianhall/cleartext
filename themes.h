#pragma once

#include <wx/colour.h>
#include <wx/string.h>
#include <vector>

// ============================================================================
// THEMES
// ============================================================================

// One palette drives both the editor chrome (background/foreground/caret/
// margins) and every lexer's syntax colors, so adding a theme is just one
// more entry in AllThemes() rather than touching each Apply*Styles function.
struct EditorTheme
{
    wxString name;
    wxColour background;
    wxColour foreground;
    wxColour caret;
    wxColour selectionBg;
    wxColour marginBg;
    wxColour marginFg;
    wxColour comment;
    wxColour number;
    wxColour string;
    wxColour preprocessor;  // also: python decorator
    wxColour keyword;       // bold
    wxColour keyword2;      // classnames/defnames/second keyword set
    wxColour operatorColor;
    wxColour tag;            // markup tag / markdown header
    wxColour attribute;      // markup attribute / markdown link
    wxColour markupCode;     // markdown inline/block code
};

namespace detail
{
    inline unsigned char Lerp8(unsigned char a, unsigned char b, double t)
    {
        return (unsigned char)(a + (b - a) * t);
    }

    inline wxColour Blend(const wxColour &a, const wxColour &b, double t)
    {
        return wxColour(Lerp8(a.Red(), b.Red(), t),
                         Lerp8(a.Green(), b.Green(), t),
                         Lerp8(a.Blue(), b.Blue(), t));
    }

    // Builds a full theme from just a background/foreground pair plus two
    // accent colors, deriving margins/comments/selection by blending toward
    // the background — this is what makes it cheap to add many themes that
    // still look coherent instead of hand-tuning 16 fields each.
    inline EditorTheme MakeTheme(const wxString &name, wxColour bg, wxColour fg,
                                  wxColour accent, wxColour accent2, wxColour warm)
    {
        EditorTheme t;
        t.name = name;
        t.background = bg;
        t.foreground = fg;
        t.caret = fg;
        t.selectionBg = Blend(bg, accent, 0.35);
        t.marginBg = Blend(bg, fg, 0.06);
        t.marginFg = Blend(fg, bg, 0.5);
        t.comment = Blend(fg, bg, 0.45);
        t.number = accent2;
        t.string = warm;
        t.preprocessor = accent2;
        t.keyword = accent;
        t.keyword2 = accent2;
        t.operatorColor = Blend(fg, bg, 0.25);
        t.tag = accent;
        t.attribute = accent2;
        t.markupCode = warm;
        return t;
    }
}

// Full built-in theme catalog, in menu order. Index 0 is applied at startup.
inline const std::vector<EditorTheme> &AllThemes()
{
    using detail::MakeTheme;
    static const std::vector<EditorTheme> themes = {
        MakeTheme("Default", wxColour(255, 255, 255), wxColour(20, 20, 20),
                  wxColour(111, 66, 193), wxColour(14, 124, 134), wxColour(163, 21, 21)),
        MakeTheme("Ocean", wxColour(240, 248, 255), wxColour(20, 30, 40),
                  wxColour(0, 119, 182), wxColour(0, 180, 216), wxColour(180, 60, 20)),
        MakeTheme("Sunset", wxColour(255, 247, 240), wxColour(40, 25, 20),
                  wxColour(232, 93, 4), wxColour(208, 0, 0), wxColour(150, 50, 10)),
        MakeTheme("Forest", wxColour(240, 247, 240), wxColour(20, 32, 20),
                  wxColour(45, 106, 79), wxColour(116, 198, 157), wxColour(120, 70, 20)),
        MakeTheme("Rose", wxColour(255, 240, 245), wxColour(40, 20, 26),
                  wxColour(201, 24, 74), wxColour(255, 117, 143), wxColour(140, 30, 60)),
        MakeTheme("Autumn", wxColour(253, 246, 236), wxColour(40, 28, 15),
                  wxColour(187, 77, 0), wxColour(140, 80, 0), wxColour(120, 40, 10)),
        MakeTheme("Mint", wxColour(240, 253, 250), wxColour(15, 35, 30),
                  wxColour(6, 146, 62), wxColour(45, 212, 191), wxColour(160, 90, 20)),
        MakeTheme("Grape", wxColour(247, 240, 253), wxColour(30, 20, 40),
                  wxColour(106, 76, 147), wxColour(157, 78, 221), wxColour(130, 30, 90)),
        MakeTheme("Steel", wxColour(243, 246, 249), wxColour(25, 30, 35),
                  wxColour(64, 103, 158), wxColour(92, 103, 125), wxColour(140, 50, 30)),
        MakeTheme("Coffee", wxColour(250, 244, 237), wxColour(40, 30, 20),
                  wxColour(111, 69, 24), wxColour(169, 116, 79), wxColour(120, 40, 20)),
        MakeTheme("Candy", wxColour(255, 240, 250), wxColour(40, 20, 35),
                  wxColour(255, 45, 149), wxColour(123, 47, 247), wxColour(180, 20, 90)),
        MakeTheme("Sky", wxColour(240, 249, 255), wxColour(15, 30, 40),
                  wxColour(0, 150, 199), wxColour(72, 202, 228), wxColour(150, 60, 20)),
        MakeTheme("Lime", wxColour(247, 255, 235), wxColour(28, 35, 10),
                  wxColour(88, 131, 0), wxColour(120, 170, 0), wxColour(140, 60, 10)),
        MakeTheme("Graphite", wxColour(45, 45, 48), wxColour(220, 220, 220),
                  wxColour(120, 170, 220), wxColour(160, 160, 165), wxColour(214, 157, 133)),
        MakeTheme("Midnight", wxColour(13, 17, 38), wxColour(205, 214, 235),
                  wxColour(110, 155, 240), wxColour(150, 120, 230), wxColour(230, 165, 120)),

        // Well-known editor palettes, given their real published colors
        // rather than run through the accent-blend generator above.
        EditorTheme{
            "Dracula",
            wxColour(40, 42, 54), wxColour(248, 248, 242),
            wxColour(248, 248, 242), wxColour(68, 71, 90),
            wxColour(33, 34, 44), wxColour(150, 152, 176),
            wxColour(98, 114, 164), wxColour(189, 147, 249), wxColour(241, 250, 140),
            wxColour(255, 184, 108), wxColour(255, 121, 198), wxColour(139, 233, 253),
            wxColour(248, 248, 242), wxColour(255, 121, 198), wxColour(80, 250, 123),
            wxColour(241, 250, 140)
        },
        EditorTheme{
            "Nord",
            wxColour(46, 52, 64), wxColour(216, 222, 233),
            wxColour(216, 222, 233), wxColour(67, 76, 94),
            wxColour(59, 66, 82), wxColour(122, 133, 153),
            wxColour(76, 86, 106), wxColour(180, 142, 173), wxColour(163, 190, 140),
            wxColour(208, 135, 112), wxColour(129, 161, 193), wxColour(136, 192, 208),
            wxColour(216, 222, 233), wxColour(129, 161, 193), wxColour(235, 203, 139),
            wxColour(163, 190, 140)
        },
        EditorTheme{
            "Monokai",
            wxColour(39, 40, 34), wxColour(248, 248, 242),
            wxColour(248, 248, 242), wxColour(73, 72, 62),
            wxColour(50, 51, 44), wxColour(144, 144, 138),
            wxColour(117, 113, 94), wxColour(174, 129, 255), wxColour(230, 219, 116),
            wxColour(253, 151, 31), wxColour(249, 38, 114), wxColour(166, 226, 46),
            wxColour(248, 248, 242), wxColour(249, 38, 114), wxColour(166, 226, 46),
            wxColour(230, 219, 116)
        },
        EditorTheme{
            "Cyberpunk",
            wxColour(10, 10, 16), wxColour(224, 224, 255),
            wxColour(0, 255, 200), wxColour(60, 60, 90),
            wxColour(18, 18, 26), wxColour(120, 120, 160),
            wxColour(100, 100, 140), wxColour(255, 255, 0), wxColour(0, 255, 102),
            wxColour(0, 255, 255), wxColour(255, 0, 153), wxColour(0, 255, 255),
            wxColour(224, 224, 255), wxColour(255, 0, 153), wxColour(0, 255, 255),
            wxColour(0, 255, 102)
        },
        EditorTheme{
            "High Contrast",
            wxColour(0, 0, 0), wxColour(255, 255, 255),
            wxColour(255, 255, 255), wxColour(38, 79, 120),
            wxColour(0, 0, 0), wxColour(255, 255, 255),
            wxColour(160, 160, 160), wxColour(0, 255, 255), wxColour(255, 140, 0),
            wxColour(0, 255, 255), wxColour(255, 255, 0), wxColour(0, 255, 255),
            wxColour(255, 255, 255), wxColour(255, 255, 0), wxColour(0, 255, 255),
            wxColour(255, 140, 0)
        },
        MakeTheme("Slate Blue", wxColour(30, 38, 54), wxColour(198, 208, 224),
                  wxColour(106, 142, 222), wxColour(140, 170, 220), wxColour(224, 155, 110)),
    };
    return themes;
}

# ClearText

A lightweight, multi-tab text editor built with wxWidgets and Scintilla, for Linux and Windows.

## Features

- **Tabs** — open multiple files at once, each tracked independently (title, modified state, saved path)
- **Syntax highlighting** — automatic, based on file extension:
  | Extension | Language |
  |---|---|
  | `.c .cpp .cc .cxx .h .hpp .hxx .java .js .cs` | C-family |
  | `.py` | Python |
  | `.html .htm` | HTML |
  | `.xml` | XML |
  | anything else | plain text |
- **Line numbers** — gutter auto-sizes to the document's line count and current zoom level
- **Word wrap** — on by default, toggle in View menu
- **Zoom** — Ctrl+scroll, or View → Zoom In/Out/Reset
- **Find & Replace** — non-modal dialog, wrap-around search (toggle in Edit menu), Replace All
- **Print** — scales to the printer's actual page size/DPI
- **Command-line files** — `cleartext file1.py file2.cpp` opens each in its own tab; a path that doesn't exist yet opens a blank tab pointed at that filename, ready to save
- **Single instance** — running `cleartext <file>` again while it's already open hands the file off to the running window as a new tab instead of launching a second copy
- **Help → About** — version info and the MIT license text

## Building

Requires wxWidgets (with the `stc`, `net`, and `adv` components) and a C++17 compiler.

```
make               # native Linux build      -> build/linux/cleartext
make debug         # ASan/UBSan debug build   -> build/linux_debug/cleartext_debug
make windows       # mingw-w64 cross-compile  -> build/windows/cleartext.exe
make clean         # remove build/
make help          # list all targets
```

`make windows` requires `x86_64-w64-mingw32-g++` and a mingw64 wxWidgets build (e.g. `sudo dnf install mingw64-wxWidgets` on Fedora). It automatically runs `collect_dlls.sh` afterward (if present in the project root) to copy the required runtime DLLs next to the `.exe`.

If your mingw wxWidgets install uses a different version or path than what's in the Makefile, adjust `WX_CFLAGS_WIN` / `WX_LIBS_WIN` at the top.

### Debug builds

`make debug` builds with AddressSanitizer and UndefinedBehaviorSanitizer enabled — just run `build/linux_debug/cleartext_debug` directly and it will print a symbolized stack trace to stderr if it crashes, no debugger needed.

## Usage

```
cleartext                      # opens with a single blank "Untitled" tab
cleartext notes.txt            # opens notes.txt in a tab
cleartext a.cpp b.py c.html    # opens each in its own tab
```

### Keyboard shortcuts

| Action | Shortcut |
|---|---|
| New tab | Ctrl+T |
| Open | Ctrl+O |
| Save | Ctrl+S |
| Save As | Ctrl+Shift+S |
| Print | Ctrl+P |
| Close tab | Ctrl+W |
| Find | Ctrl+F |
| Find Next | F3 |
| Replace | Ctrl+H |
| Select All | Ctrl+A |
| Zoom In / Out / Reset | Ctrl+= / Ctrl+- / Ctrl+0 |

## Project layout

```
cleartext.cpp    single-file source
Makefile         build (Linux, Windows cross-compile, debug)
```

## License

MIT — see the license text in Help → About within the application.

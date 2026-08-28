# Native Linux build with:    make        (output: build/linux/cleartext)
# Windows cross-compile with: make windows (output: build/windows/cleartext.exe)
# Debug (ASan/UBSan) build with: make debug (output: build/linux_debug/cleartext_debug)

CXX_LINUX = g++
CXX_WIN   = x86_64-w64-mingw32-g++

TARGET     = cleartext
SRC        = cleartext.cpp highlighting.cpp
HEADERS    = themes.h highlighting.h

BUILD_DIR_LINUX       = build/linux
BUILD_DIR_LINUX_DEBUG = build/linux_debug
BUILD_DIR_WIN         = build/windows

EXECUTABLE_LINUX       = $(BUILD_DIR_LINUX)/$(TARGET)
EXECUTABLE_LINUX_DEBUG = $(BUILD_DIR_LINUX_DEBUG)/$(TARGET)_debug
EXECUTABLE_WIN         = $(BUILD_DIR_WIN)/$(TARGET).exe

CXXFLAGS_COMMON = -Wall -Wextra -std=c++17

# ============================================================================
# LINUX (native wx-config)
# ============================================================================
WX_CFLAGS_LINUX := $(shell wx-config --cxxflags std,stc,net,adv)
WX_LIBS_LINUX   := $(shell wx-config --libs std,stc,net,adv)

CXXFLAGS_LINUX = $(CXXFLAGS_COMMON) -O2 $(WX_CFLAGS_LINUX)
LDFLAGS_LINUX  = $(WX_LIBS_LINUX)

CXXFLAGS_LINUX_DEBUG = $(CXXFLAGS_COMMON) -DDEBUG -g -O0 -fno-omit-frame-pointer \
                       -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=address,undefined \
                       $(WX_CFLAGS_LINUX)
LDFLAGS_LINUX_DEBUG  = $(WX_LIBS_LINUX) -fsanitize=address -fsanitize=undefined -rdynamic

# ============================================================================
# WINDOWS (mingw-w64 cross-compile)
# Adjust the wx version/suffix below to match your installed mingw64 wxWidgets
# package if it differs (e.g. Fedora's mingw64-wxWidgets ships 3.0).
# ============================================================================
WX_CFLAGS_WIN := -I/usr/x86_64-w64-mingw32/sys-root/mingw/include/wx-3.0 \
                  -I/usr/x86_64-w64-mingw32/sys-root/mingw/lib/wx/include/x86_64-w64-mingw32-msw-unicode-3.0

WX_LIBS_WIN   := -L/usr/x86_64-w64-mingw32/sys-root/mingw/lib \
                  -lwx_mswu_stc-3.0-x86_64-w64-mingw32 \
                  -lwx_mswu_adv-3.0-x86_64-w64-mingw32 \
                  -lwx_mswu_core-3.0-x86_64-w64-mingw32 \
                  -lwx_baseu_net-3.0-x86_64-w64-mingw32 \
                  -lwx_baseu-3.0-x86_64-w64-mingw32 \
                  -lcomctl32 -lrpcrt4 -loleaut32 -lole32 -luuid -lwinspool \
                  -lwinmm -lshell32 -lshlwapi -lcomdlg32 -ladvapi32 \
                  -lversion -lwsock32 -lgdi32

WINDRES = x86_64-w64-mingw32-windres
ICON_RES := $(if $(wildcard icon.ico),$(BUILD_DIR_WIN)/cleartext_res.o,)

CXXFLAGS_WIN = $(CXXFLAGS_COMMON) -DWIN32 -D_WIN32 -D_WIN32_WINNT=0x0A00 \
               $(WX_CFLAGS_WIN) -O2

LDFLAGS_WIN  = $(WX_LIBS_WIN) -static-libgcc -static-libstdc++ -mwindows

.PHONY: all linux windows debug clean help

all: linux

linux: $(EXECUTABLE_LINUX)

$(EXECUTABLE_LINUX): $(SRC) $(HEADERS)
	@mkdir -p $(BUILD_DIR_LINUX)
	$(CXX_LINUX) $(CXXFLAGS_LINUX) $(SRC) -o $(EXECUTABLE_LINUX) $(LDFLAGS_LINUX)
	@echo "Built $(EXECUTABLE_LINUX)"

# Cross-compiles (embedding icon.ico as the exe/window icon if present),
# then automatically collects the mingw runtime/wx DLLs alongside the .exe
# via collect_dlls.sh, same pattern as Felix Terminal's frontend-windows-dlls
# target.
windows: $(EXECUTABLE_WIN)
	@if [ -f collect_dlls.sh ]; then \
		echo "Collecting Windows DLLs for ClearText..."; \
		./collect_dlls.sh $(EXECUTABLE_WIN) /usr/x86_64-w64-mingw32/sys-root/mingw/bin $(BUILD_DIR_WIN); \
		echo "✓ DLLs collected to $(BUILD_DIR_WIN)"; \
	else \
		echo "collect_dlls.sh not found — copy manually or write one, e.g.:"; \
		echo "  libwx_mswu_stc-3.0-x86_64-w64-mingw32.dll, libwx_mswu_adv-3.0-x86_64-w64-mingw32.dll,"; \
		echo "  libwx_mswu_core-3.0-x86_64-w64-mingw32.dll, libwx_baseu_net-3.0-x86_64-w64-mingw32.dll,"; \
		echo "  libwx_baseu-3.0-x86_64-w64-mingw32.dll, libwinpthread-1.dll"; \
	fi

$(EXECUTABLE_WIN): $(SRC) $(HEADERS) $(ICON_RES)
	@mkdir -p $(BUILD_DIR_WIN)
	$(CXX_WIN) $(CXXFLAGS_WIN) $(SRC) $(ICON_RES) -o $(EXECUTABLE_WIN) $(LDFLAGS_WIN)
	@echo "Built $(EXECUTABLE_WIN)"

$(BUILD_DIR_WIN)/cleartext_res.o: cleartext.rc icon.ico
	@mkdir -p $(BUILD_DIR_WIN)
	$(WINDRES) cleartext.rc -O coff -o $@

debug: $(EXECUTABLE_LINUX_DEBUG)

$(EXECUTABLE_LINUX_DEBUG): $(SRC) $(HEADERS)
	@mkdir -p $(BUILD_DIR_LINUX_DEBUG)
	$(CXX_LINUX) $(CXXFLAGS_LINUX_DEBUG) $(SRC) -o $(EXECUTABLE_LINUX_DEBUG) $(LDFLAGS_LINUX_DEBUG)
	@echo "Built $(EXECUTABLE_LINUX_DEBUG) — just run it directly; ASan/UBSan print a stack trace on crash"

clean:
	rm -rf build

help:
	@echo "make               - native Linux build (build/linux/cleartext)"
	@echo "make windows       - mingw-w64 cross-compile + auto DLL collection (build/windows/)"
	@echo "make debug         - ASan/UBSan debug build (build/linux_debug/cleartext_debug)"
	@echo "make clean         - remove the build/ directory"
	@echo ""
	@echo "Windows cross-compile requires:"
	@echo "  x86_64-w64-mingw32-g++ (mingw-w64)"
	@echo "  A mingw64 wxWidgets build (Fedora: sudo dnf install mingw64-wxWidgets)"
	@echo "  Adjust WX_CFLAGS_WIN/WX_LIBS_WIN above if your wx version/paths differ."
	@echo ""
	@echo "If icon.ico exists in the project root at build time, it's embedded"
	@echo "into cleartext.exe (via cleartext.rc + windres) as the exe/window icon."

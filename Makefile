CXX     = g++
TARGET  = notepad
SRC     = notepad.cpp

WX_CFLAGS := $(shell wx-config --cxxflags std,stc)
WX_LIBS   := $(shell wx-config --libs std,stc)

CXXFLAGS_COMMON  = -Wall -Wextra -std=c++17
CXXFLAGS_RELEASE = $(CXXFLAGS_COMMON) -O2 $(WX_CFLAGS)
CXXFLAGS_DEBUG   = $(CXXFLAGS_COMMON) -DDEBUG -g -O0 -fno-omit-frame-pointer \
                   -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=address,undefined \
                   $(WX_CFLAGS)
LDFLAGS_DEBUG    = -fsanitize=address -fsanitize=undefined -rdynamic

.PHONY: all debug clean help

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS_RELEASE) $(SRC) -o $(TARGET) $(WX_LIBS)
	@echo "Built ./$(TARGET)"

debug: $(TARGET)_debug

$(TARGET)_debug: $(SRC)
	$(CXX) $(CXXFLAGS_DEBUG) $(SRC) -o $(TARGET)_debug $(WX_LIBS) $(LDFLAGS_DEBUG)
	@echo "Built ./$(TARGET)_debug — just run ./$(TARGET)_debug; ASan/UBSan print a stack trace on crash"

clean:
	rm -f $(TARGET) $(TARGET)_debug

help:
	@echo "make        - release build (./notepad)"
	@echo "make debug  - ASan/UBSan debug build, prints stack traces on crash (./notepad_debug)"
	@echo "make clean  - remove built binaries"

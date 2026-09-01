cmake_minimum_required(VERSION 3.20)
project(cleartext CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static CRT so the exe doesn't need the VC++ redistributable either
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(WIN32)
    # Expects vcpkg static triplet, e.g.:
    #   vcpkg install wxwidgets:x64-windows-static
    # Configure with:
    #   cmake -B build -S . -A x64 -DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
    find_package(wxWidgets CONFIG REQUIRED COMPONENTS core base stc adv aui net)
else()
    # Native Linux: use CMake's bundled FindwxWidgets module, which shells
    # out to wx-config (same wx-config the Makefile uses).
    find_package(wxWidgets REQUIRED COMPONENTS std stc net adv aui)
    include(${wxWidgets_USE_FILE})
endif()

set(SRC
    main.cpp editor_frame.cpp ipc.cpp app_config.cpp printing.cpp highlighting.cpp
    custom_themes.cpp encoding.cpp find_in_files.cpp theme_editor.cpp copy_html.cpp
)

set(HEADERS
    themes.h highlighting.h editor_frame.h ipc.h app_config.h printing.h
    custom_themes.h encoding.h find_in_files.h theme_editor.h copy_html.h
)

# Embed icon if present
set(RC_FILE "")
if(EXISTS "${CMAKE_SOURCE_DIR}/cleartext.rc" AND EXISTS "${CMAKE_SOURCE_DIR}/icon.ico")
    set(RC_FILE "${CMAKE_SOURCE_DIR}/cleartext.rc")
endif()

add_executable(cleartext WIN32 ${SRC} ${HEADERS} ${RC_FILE})

if(WIN32)
    target_compile_definitions(cleartext PRIVATE WIN32 _WIN32 _WIN32_WINNT=0x0A00)
endif()
target_compile_definitions(cleartext PRIVATE wxUSE_UNICODE=1)
target_link_libraries(cleartext PRIVATE ${wxWidgets_LIBRARIES})
target_include_directories(cleartext PRIVATE ${wxWidgets_INCLUDE_DIRS})

if(MSVC)
    target_compile_options(cleartext PRIVATE /W4)
endif()

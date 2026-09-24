cmake_minimum_required(VERSION 3.20)

project(tetromino
    VERSION 1.0.1
    DESCRIPTION "Falling-block puzzle game for the Linux terminal: termios + ANSI escape sequences, no TUI framework"
    HOMEPAGE_URL "https://github.com/varuns2903/tetromino"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Single-config generators (Makefiles, Ninja) default to an empty build type,
# which means "no optimisation, no debug info". Pick Release unless the user
# asked for something else.
get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT is_multi_config AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

option(TETROMINO_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(TETROMINO_BUILD_TESTS "Build the unit tests" ON)
option(TETROMINO_SANITIZE "Build with AddressSanitizer + UBSan" OFF)
option(TETROMINO_STATIC "Link the executable fully statically (for portable release binaries)" OFF)

include(GNUInstallDirs)

# Static release binaries are built as static-PIE, so they keep address-space
# layout randomisation; everything must then be compiled position-independent.
if(TETROMINO_STATIC)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# ---------------------------------------------------------------------------
# Shared compile options, applied to every target through an interface lib.
# ---------------------------------------------------------------------------
add_library(tetromino_options INTERFACE)
target_compile_options(tetromino_options INTERFACE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
    -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused
    -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion
    -Wimplicit-fallthrough
    $<$<BOOL:${TETROMINO_WARNINGS_AS_ERRORS}>:-Werror>
    $<$<CONFIG:Debug>:-O0 -g3>
)
if(TETROMINO_SANITIZE)
    target_compile_options(tetromino_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(tetromino_options INTERFACE -fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------------------
# tetromino_engine: pure game logic. No terminal, no I/O. This is what the tests
# link against, which enforces the "engine doesn't know about the terminal"
# rule at the build level: tui/ headers are simply not on its include path.
# ---------------------------------------------------------------------------
add_library(tetromino_engine STATIC
    src/core/Game.cpp
    src/core/GameConfig.cpp
    src/core/Presets.cpp
    src/core/GameState.cpp
    src/game/Board.cpp
    src/game/Piece.cpp
    src/game/Collision.cpp
    src/game/PieceGenerator.cpp
    src/game/Scoring.cpp
    src/game/Rotation.cpp
)
target_include_directories(tetromino_engine PUBLIC include)
target_link_libraries(tetromino_engine PRIVATE tetromino_options)

# ---------------------------------------------------------------------------
# tetromino_tui: terminal I/O, input decoding, screen buffer, rendering.
# Depends on the engine only to read GameState.
# ---------------------------------------------------------------------------
add_library(tetromino_tui STATIC
    src/tui/Ansi.cpp
    src/tui/Terminal.cpp
    src/tui/Input.cpp
    src/tui/Color.cpp
    src/tui/Theme.cpp
    src/tui/ScreenBuffer.cpp
    src/tui/PixelCanvas.cpp
    src/tui/Panel.cpp
    src/tui/Layout.cpp
    src/tui/Renderer.cpp
    src/util/Logger.cpp
)
target_include_directories(tetromino_tui PUBLIC include)
target_link_libraries(tetromino_tui PUBLIC tetromino_engine PRIVATE tetromino_options)

# ---------------------------------------------------------------------------
# The executable: argument parsing + the application loop that wires
# Terminal -> Input -> Game -> Renderer together.
# ---------------------------------------------------------------------------
add_executable(tetromino
    src/main.cpp
    src/app/Application.cpp
    src/app/Options.cpp
)
target_link_libraries(tetromino PRIVATE tetromino_tui tetromino_engine tetromino_options)
target_compile_definitions(tetromino PRIVATE TETROMINO_VERSION="${PROJECT_VERSION}")

if(TETROMINO_STATIC)
    # static-PIE + full RELRO: one self-contained file, still hardened.
    target_link_options(tetromino PRIVATE -static-pie -Wl,-z,relro,-z,now)
endif()

# ---------------------------------------------------------------------------
# Install: `cmake --install build` (prefix /usr/local by default).
# ---------------------------------------------------------------------------
install(TARGETS tetromino RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES docs/tetromino.6 DESTINATION ${CMAKE_INSTALL_MANDIR}/man6)
install(FILES README.md LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})

# ---------------------------------------------------------------------------
# Packages: `cpack --config build/CPackConfig.cmake -G "DEB;RPM;TGZ"`.
# The release workflow builds these for every tagged version.
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "tetromino")
set(CPACK_PACKAGE_VENDOR "Varun S")
set(CPACK_PACKAGE_CONTACT "Varun S <04varuns@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Falling-block puzzle game for the terminal")
set(CPACK_PACKAGE_DESCRIPTION "Tetromino is a falling-block puzzle game that runs in a Linux terminal. \
It has a setup menu with four board sizes and four difficulty levels, wall-kick \
rotation, hold, a next-piece preview, a ghost piece and 24-bit colour, and it is \
written from scratch in C++20 with termios and ANSI escape sequences.")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "tetromino")
set(CPACK_STRIP_FILES ON)
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)  # empty for the static release build

set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")
set(CPACK_RPM_PACKAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_RPM_PACKAGE_AUTOREQ ON)
# Directories owned by the distribution's base packages, not by us.
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION /usr/share/man /usr/share/man/man6)

include(CPack)

if(TETROMINO_BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt)
    enable_testing()
    add_subdirectory(tests)
endif()

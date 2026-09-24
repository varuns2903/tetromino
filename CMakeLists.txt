cmake_minimum_required(VERSION 3.20)

project(tetromino
    VERSION 1.0.0
    DESCRIPTION "Falling-block puzzle game for the Linux terminal: termios + ANSI escape sequences, no TUI framework"
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

install(TARGETS tetromino RUNTIME DESTINATION bin)

if(TETROMINO_BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt)
    enable_testing()
    add_subdirectory(tests)
endif()

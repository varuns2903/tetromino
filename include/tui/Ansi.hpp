#pragma once

// ANSI / VT escape sequences, in one place.
//
// A terminal emulator is a state machine that consumes a byte stream. Most
// bytes are printed at the cursor position; bytes starting with ESC (0x1b)
// are commands. The most common family is CSI ("Control Sequence Introducer",
// ESC '['), followed by numeric parameters and a final letter that names the
// command:
//
//   ESC [ 12 ; 40 H     move cursor to row 12, column 40 (1-based)
//   ESC [ 2 J           erase the whole screen
//   ESC [ 1 ; 31 m      SGR ("Select Graphic Rendition"): bold + red foreground
//   ESC [ ? 25 l        DEC private mode 25 (cursor visible) -> reset (hide)
//   ESC [ ? 1049 h      DEC private mode 1049 -> switch to the alternate screen
//
// The alternate screen is a second screen buffer with no scrollback. Full
// screen programs (vim, less, htop, this game) switch to it on startup and
// back on exit, which is why your shell history reappears untouched when they
// quit.
//
// No other file in the project should contain raw escape sequences.

#include <string>
#include <string_view>

#include "tui/Color.hpp"

namespace tetromino::tui::ansi {

// --- Fixed sequences ------------------------------------------------------

inline constexpr std::string_view kEnterAltScreen = "\x1b[?1049h";
inline constexpr std::string_view kLeaveAltScreen = "\x1b[?1049l";
inline constexpr std::string_view kHideCursor = "\x1b[?25l";
inline constexpr std::string_view kShowCursor = "\x1b[?25h";
inline constexpr std::string_view kClearScreen = "\x1b[2J";
inline constexpr std::string_view kCursorHome = "\x1b[H";
inline constexpr std::string_view kResetAttributes = "\x1b[0m";

// DECAWM (auto-wrap). With auto-wrap on, printing into the last column of the
// last row can scroll the whole screen by one line. We position every cell
// explicitly, so we turn wrapping off while running.
inline constexpr std::string_view kDisableAutoWrap = "\x1b[?7l";
inline constexpr std::string_view kEnableAutoWrap = "\x1b[?7h";

// Synchronized output (DEC mode 2026). The terminal buffers everything between
// begin and end and paints it atomically, which eliminates tearing when a
// frame is larger than one write(). Terminals that don't know the mode simply
// ignore it.
inline constexpr std::string_view kBeginSynchronizedUpdate = "\x1b[?2026h";
inline constexpr std::string_view kEndSynchronizedUpdate = "\x1b[?2026l";

// Focus reporting (DEC mode 1004): terminal sends ESC [ I / ESC [ O when the
// window gains / loses focus. We use it to auto-pause.
inline constexpr std::string_view kEnableFocusEvents = "\x1b[?1004h";
inline constexpr std::string_view kDisableFocusEvents = "\x1b[?1004l";

// xterm window-title stack (XTWINOPS 22/23): save the user's title before we
// change it, restore it on exit. Terminals without a title stack ignore these.
inline constexpr std::string_view kPushTitle = "\x1b[22;0t";
inline constexpr std::string_view kPopTitle = "\x1b[23;0t";

// Queries whose answers arrive on stdin. OSC 11 asks for the background
// colour (not every terminal answers); DA1 ("what are you?") is answered by
// every VT-compatible terminal, so its reply marks the end of the answers.
inline constexpr std::string_view kQueryBackground = "\x1b]11;?\x1b\\";
inline constexpr std::string_view kQueryDeviceAttributes = "\x1b[c";

// Everything needed to put the terminal back the way a shell expects it,
// as a single constant so it can be written from a signal handler (no
// allocation, one write()).
inline constexpr std::string_view kRestoreAll =
    "\x1b[0m"       // reset colours / attributes
    "\x1b[?2026l"   // end any half-finished synchronized update
    "\x1b[?1004l"   // stop focus reports
    "\x1b[?7h"      // auto-wrap back on
    "\x1b[?25h"     // show cursor
    "\x1b[?1049l";  // back to the main screen

// --- Parameterised sequences ----------------------------------------------
// These append to an existing string instead of returning a new one, so the
// renderer can build a whole frame in one reusable buffer without allocating.

// Move the cursor. Takes 0-based coordinates (the terminal uses 1-based).
void appendMoveTo(std::string& out, int row, int column);

// Emit a full SGR sequence for `style`, downsampled for `mode`.
// Always starts with a reset (0) so the result doesn't depend on the previous
// attributes, which keeps the diff renderer simple.
void appendStyle(std::string& out, const Style& style, ColorMode mode);

// Set the window title (OSC 0).
void appendTitle(std::string& out, std::string_view title);

// Encode a Unicode code point as UTF-8.
void appendUtf8(std::string& out, char32_t codepoint);

}  // namespace tetromino::tui::ansi

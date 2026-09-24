#pragma once

// RAII owner of the controlling terminal.
//
// Constructing a Terminal switches the tty into "game mode" (raw input,
// alternate screen, hidden cursor); destroying it puts everything back.
// All termios / ioctl / signal code in the project lives behind this class.
//
// Only one Terminal may exist at a time: terminal modes and signal
// dispositions are process-wide resources.

#include <chrono>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

#include "tui/Color.hpp"

namespace tetromino::tui {

struct TerminalSize {
    int columns = 0;
    int rows = 0;

    friend constexpr bool operator==(TerminalSize, TerminalSize) = default;
};

class Terminal {
public:
    // Saves the current tty state, installs signal handlers and enters raw
    // mode + alternate screen. Throws std::runtime_error if stdin/stdout is
    // not a terminal or the tty can't be configured.
    Terminal();
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;
    Terminal(Terminal&&) = delete;
    Terminal& operator=(Terminal&&) = delete;

    // --- Mode control (all idempotent) ---
    void enterRawMode();
    void enterAlternateScreen();
    void leaveAlternateScreen();
    void hideCursor();
    void showCursor();
    // Set the window title. The previous title is saved and put back by
    // restore() on terminals that support the xterm title stack.
    void setTitle(std::string_view title);

    // Undo everything: original termios, main screen, visible cursor.
    // Called by the destructor; safe to call more than once.
    void restore();

    // Stop the process like Ctrl-Z would in a normal program (we disable the
    // tty's own Ctrl-Z handling in raw mode). The terminal is restored before
    // stopping and re-entered on `fg`; consumeResize() reports true afterwards so
    // the caller repaints.
    void suspend();

    // --- Geometry ---
    // Current window size via ioctl(TIOCGWINSZ); falls back to $COLUMNS/$LINES
    // and finally 80x24 if the terminal won't say.
    [[nodiscard]] TerminalSize size() const;

    // --- I/O ---
    // Write all of `bytes` to stdout. Returns false if the terminal went away
    // (e.g. the window was closed), in which case the caller should quit.
    bool write(std::string_view bytes);

    // Block until stdin is readable, a signal arrives, or `timeout` elapses.
    // Returns true if input is available. This is how the game loop sleeps
    // without busy-waiting.
    bool waitForInput(std::chrono::milliseconds timeout);

    // Ask the terminal for its background colour. Waits at most `timeout`,
    // usually far less: the terminal's reply to a second, universally
    // supported query ends the wait early. nullopt if it doesn't say.
    [[nodiscard]] std::optional<Rgb> queryBackground(std::chrono::milliseconds timeout);

    // Read whatever bytes are available right now, without blocking.
    // Returns the number of bytes read (0 if none).
    std::size_t readAvailable(std::span<char> buffer);

    // --- Asynchronous events (set by signal handlers) ---
    // True once after each SIGWINCH (and after resuming from suspend).
    [[nodiscard]] bool consumeResize();
    // True if SIGINT / SIGTERM / SIGHUP was received.
    [[nodiscard]] bool quitRequested() const;

private:
    bool rawMode_ = false;
    bool altScreen_ = false;
    bool cursorHidden_ = false;
    bool titlePushed_ = false;
};

}  // namespace tetromino::tui

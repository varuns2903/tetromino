#pragma once

// Keyboard input: raw bytes -> key events.
//
// In raw mode the terminal hands us bytes, not keys. Printable keys are one
// byte ('a' = 0x61), but special keys arrive as escape sequences:
//
//   Up    ESC [ A   (or ESC O A in "application cursor" mode)
//   Down  ESC [ B
//   Right ESC [ C
//   Left  ESC [ D
//
// The Escape key itself is a lone ESC byte, which is the classic ambiguity
// every TUI has to resolve: is this ESC the start of an arrow key whose
// remaining bytes haven't arrived yet, or was Escape pressed? We resolve it
// with a short timeout: if nothing follows within a few milliseconds, it's
// the Escape key. (This is the same trick as vim's 'ttimeoutlen'.)
//
// InputDecoder is pure (bytes in, events out) so it can be unit tested;
// Input glues it to the Terminal.

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/Types.hpp"

namespace tetromino::tui {

class Terminal;

enum class Key : std::uint8_t {
    Up,
    Down,
    Left,
    Right,
    Enter,
    Escape,
    Space,
    Tab,
    Backspace,
    Character,    // printable ASCII, see KeyEvent::ch
    Interrupt,    // Ctrl-C (a byte, since ISIG is off in raw mode)
    Suspend,      // Ctrl-Z
    FocusGained,  // ESC [ I
    FocusLost,    // ESC [ O
};

struct KeyEvent {
    Key key = Key::Character;
    char ch = '\0';  // only meaningful for Key::Character

    friend constexpr bool operator==(const KeyEvent&, const KeyEvent&) = default;
};

class InputDecoder {
public:
    // Append raw bytes read from the terminal.
    void feed(std::string_view bytes);

    // Decode every complete event in the buffer and append it to `out`.
    // An incomplete escape sequence at the end is kept for the next call.
    void decode(std::vector<KeyEvent>& out);

    // True if the buffer ends in a partial escape sequence (e.g. a lone ESC).
    [[nodiscard]] bool hasIncompleteSequence() const { return !buffer_.empty(); }

    // Give up waiting for the rest of a sequence: a lone ESC becomes Escape,
    // anything else is discarded.
    void flushIncomplete(std::vector<KeyEvent>& out);

private:
    std::string buffer_;
};

class Input {
public:
    explicit Input(Terminal& terminal);

    // Sleep until input arrives or `timeout` passes, then decode everything
    // that's available into `events` (which is cleared first). Returns early
    // on signals such as SIGWINCH.
    void poll(std::chrono::milliseconds timeout, std::vector<KeyEvent>& events);

private:
    void readInto(std::vector<KeyEvent>& events);

    Terminal& terminal_;
    InputDecoder decoder_;
};

// Key bindings: translate a key into a game action for the current mode.
// Returns nullopt for keys that don't do anything in that mode.
[[nodiscard]] std::optional<core::Action> actionFor(const KeyEvent& event, core::GameMode mode);

}  // namespace tetromino::tui

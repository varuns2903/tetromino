#include "tui/Input.hpp"

#include <array>

#include "tui/Terminal.hpp"

namespace tetromino::tui {

namespace {

constexpr char kEsc = '\x1b';

// How long to wait for the rest of an escape sequence before deciding a lone
// ESC byte was the Escape key. Sequences from a local terminal arrive in one
// read(); this only matters over slow links.
constexpr std::chrono::milliseconds kEscapeTimeout{25};

// Guard against garbage: no sequence we care about is anywhere near this long.
constexpr std::size_t kMaxSequenceLength = 32;

bool isCsiFinalByte(char c) { return c >= '\x40' && c <= '\x7e'; }

std::optional<Key> arrowFor(char final) {
    switch (final) {
    case 'A': return Key::Up;
    case 'B': return Key::Down;
    case 'C': return Key::Right;
    case 'D': return Key::Left;
    default: return std::nullopt;
    }
}

std::optional<KeyEvent> decodeSingleByte(char byte) {
    switch (byte) {
    case '\r':
    case '\n': return KeyEvent{Key::Enter};
    case '\t': return KeyEvent{Key::Tab};
    case '\x7f':
    case '\x08': return KeyEvent{Key::Backspace};
    case '\x03': return KeyEvent{Key::Interrupt};
    case '\x1a': return KeyEvent{Key::Suspend};
    case ' ': return KeyEvent{Key::Space};
    default: break;
    }
    if (byte > ' ' && byte < '\x7f') {
        return KeyEvent{Key::Character, byte};
    }
    // Other control bytes and UTF-8 continuation bytes: not bound to anything.
    return std::nullopt;
}

// The numeric parameters of a CSI sequence that matter to us:
// "first[:...] ; modifiers[:event] ; ..." (missing values default to 1).
struct CsiParams {
    int first = 1;
    int modifiers = 1;  // 1 + bit flags: shift 1, alt 2, ctrl 4, ...
    int event = 1;      // kitty: 1 press, 2 repeat, 3 release
};

CsiParams parseCsiParams(std::string_view params) {
    CsiParams out;
    int field = 0;
    int subField = 0;
    int value = -1;
    const auto commit = [&] {
        if (value < 0) {
            return;
        }
        if (field == 0 && subField == 0) out.first = value;
        if (field == 1 && subField == 0) out.modifiers = value;
        if (field == 1 && subField == 1) out.event = value;
    };
    for (const char ch : params) {
        if (ch >= '0' && ch <= '9') {
            value = std::min((value < 0 ? 0 : value) * 10 + (ch - '0'), 1'000'000);
        } else if (ch == ':') {
            commit();
            value = -1;
            ++subField;
        } else if (ch == ';') {
            commit();
            value = -1;
            ++field;
            subField = 0;
        }
    }
    commit();
    return out;
}

KeyPhase phaseFor(int event) {
    switch (event) {
    case 2: return KeyPhase::Repeat;
    case 3: return KeyPhase::Release;
    default: return KeyPhase::Press;
    }
}

// Kitty "CSI code ; modifiers u": code is the Unicode code point of the key.
std::optional<KeyEvent> decodeKittyKey(const CsiParams& p) {
    const KeyPhase phase = phaseFor(p.event);
    const bool ctrl = ((p.modifiers - 1) & 4) != 0;
    const int code = p.first;
    if (ctrl && (code == 'c' || code == 'C')) return KeyEvent{Key::Interrupt, '\0', phase};
    if (ctrl && (code == 'z' || code == 'Z')) return KeyEvent{Key::Suspend, '\0', phase};
    if (ctrl) return std::nullopt;
    switch (code) {
    case 27: return KeyEvent{Key::Escape, '\0', phase};
    case 13: return KeyEvent{Key::Enter, '\0', phase};
    case 9: return KeyEvent{Key::Tab, '\0', phase};
    case 127:
    case 8: return KeyEvent{Key::Backspace, '\0', phase};
    case 32: return KeyEvent{Key::Space, '\0', phase};
    default: break;
    }
    if (code > 32 && code < 127) {
        return KeyEvent{Key::Character, static_cast<char>(code), phase};
    }
    return std::nullopt;  // function keys, modifiers on their own, ...
}

char lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

}  // namespace

// ---------------------------------------------------------------------------
// InputDecoder
// ---------------------------------------------------------------------------

void InputDecoder::feed(std::string_view bytes) { buffer_.append(bytes); }

void InputDecoder::decode(std::vector<KeyEvent>& out) {
    std::size_t i = 0;
    const std::size_t n = buffer_.size();

    while (i < n) {
        const char c = buffer_[i];
        if (c != kEsc) {
            if (auto ev = decodeSingleByte(c)) {
                out.push_back(*ev);
            }
            ++i;
            continue;
        }

        // ESC: need at least one more byte to know what this is.
        if (i + 1 >= n) {
            break;  // incomplete; keep it
        }
        const char intro = buffer_[i + 1];

        if (intro == '[') {
            // CSI: ESC [ <parameter bytes> <final byte>
            std::size_t j = i + 2;
            while (j < n && !isCsiFinalByte(buffer_[j]) && j - i < kMaxSequenceLength) {
                ++j;
            }
            if (j >= n) {
                break;  // incomplete
            }
            if (!isCsiFinalByte(buffer_[j])) {
                i = j;  // runaway garbage, drop it
                continue;
            }
            const char final = buffer_[j];
            const CsiParams params = parseCsiParams(std::string_view{buffer_}.substr(i + 2, j - i - 2));
            if (auto arrow = arrowFor(final)) {
                // Modifiers (e.g. ESC [ 1 ; 2 A for Shift+Up) are ignored;
                // the kitty event type (ESC [ 1 ; 1 : 3 D = released) isn't.
                out.push_back(KeyEvent{*arrow, '\0', phaseFor(params.event)});
            } else if (final == 'u' && buffer_[i + 2] != '?') {
                if (auto key = decodeKittyKey(params)) {
                    out.push_back(*key);
                }
            } else if (final == 'I' && j == i + 2) {
                out.push_back(KeyEvent{Key::FocusGained});
            } else if (final == 'O' && j == i + 2) {
                out.push_back(KeyEvent{Key::FocusLost});
            }
            // Everything else (F-keys, Home/End, ...) is unbound.
            i = j + 1;
            continue;
        }

        if (intro == 'O') {
            // SS3: ESC O <final>. Arrow keys in "application cursor" mode.
            if (i + 2 >= n) {
                break;  // incomplete
            }
            if (auto arrow = arrowFor(buffer_[i + 2])) {
                out.push_back(KeyEvent{*arrow});
            }
            i += 3;
            continue;
        }

        // ESC followed by an ordinary byte: either Alt+key, or Escape pressed
        // quickly before another key. Treat it as Escape; the next byte is
        // decoded normally on the next iteration.
        out.push_back(KeyEvent{Key::Escape});
        ++i;
    }

    buffer_.erase(0, i);
}

void InputDecoder::flushIncomplete(std::vector<KeyEvent>& out) {
    if (buffer_ == std::string_view{&kEsc, 1}) {
        out.push_back(KeyEvent{Key::Escape});
    }
    buffer_.clear();
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

Input::Input(Terminal& terminal) : terminal_(terminal) {}

void Input::readInto(std::vector<KeyEvent>& events) {
    std::array<char, 256> chunk{};
    for (;;) {
        const std::size_t got = terminal_.readAvailable(chunk);
        if (got == 0) {
            break;
        }
        decoder_.feed(std::string_view{chunk.data(), got});
        if (got < chunk.size()) {
            break;
        }
    }
    decoder_.decode(events);
}

void Input::poll(std::chrono::milliseconds timeout, std::vector<KeyEvent>& events) {
    events.clear();
    if (!terminal_.waitForInput(timeout)) {
        return;
    }
    readInto(events);

    if (decoder_.hasIncompleteSequence()) {
        // Maybe the rest of an escape sequence is still in flight.
        if (terminal_.waitForInput(kEscapeTimeout)) {
            readInto(events);
        }
        if (decoder_.hasIncompleteSequence()) {
            decoder_.flushIncomplete(events);
        }
    }
}

// ---------------------------------------------------------------------------
// Key bindings
// ---------------------------------------------------------------------------

std::optional<core::Action> actionFor(const KeyEvent& event, core::GameMode mode) {
    using core::Action;
    using core::GameMode;

    const char ch = event.key == Key::Character ? lower(event.ch) : '\0';

    // Global: Ctrl-C and Q always quit.
    if (event.key == Key::Interrupt || ch == 'q') {
        return Action::Quit;
    }
    // Escape toggles pause during a game, and backs out (quits) from the
    // start and game-over screens.
    if (event.key == Key::Escape) {
        return mode == GameMode::Playing || mode == GameMode::Paused || mode == GameMode::Countdown ? Action::Pause
                                                                                                   : Action::Quit;
    }

    switch (mode) {
    case GameMode::StartScreen:
        switch (event.key) {
        case Key::Enter:
        case Key::Space: return Action::Start;
        case Key::Up: return Action::MenuUp;
        case Key::Down: return Action::MenuDown;
        case Key::Left: return Action::MenuLeft;
        case Key::Right: return Action::MenuRight;
        case Key::Tab: return Action::MenuDown;
        default: break;
        }
        switch (ch) {
        case 'w': return Action::MenuUp;
        case 's': return Action::MenuDown;
        case 'a': return Action::MenuLeft;
        case 'd': return Action::MenuRight;
        default: return std::nullopt;
        }

    case GameMode::Playing:
        switch (event.key) {
        case Key::Left: return Action::MoveLeft;
        case Key::Right: return Action::MoveRight;
        case Key::Down: return Action::SoftDrop;
        case Key::Up: return Action::RotateClockwise;
        case Key::Space: return Action::HardDrop;
        case Key::Character: break;
        default: return std::nullopt;
        }
        switch (ch) {
        case 'a': return Action::MoveLeft;
        case 'd': return Action::MoveRight;
        case 's': return Action::SoftDrop;
        case 'w':
        case 'x': return Action::RotateClockwise;
        case 'z': return Action::RotateCounterClockwise;
        case 'c': return Action::Hold;
        case 'p': return Action::Pause;
        default: return std::nullopt;
        }

    case GameMode::Paused:
        if (ch == 'p' || event.key == Key::Enter || event.key == Key::Space) {
            return Action::Pause;
        }
        if (ch == 'r') {
            return Action::Restart;
        }
        if (ch == 'm') {
            return Action::OpenMenu;
        }
        return std::nullopt;

    case GameMode::Countdown:
        if (ch == 'p') {
            return Action::Pause;  // pause again
        }
        return std::nullopt;

    case GameMode::GameOver:
        if (ch == 'r' || event.key == Key::Enter) {
            return Action::Restart;
        }
        if (ch == 'm') {
            return Action::OpenMenu;
        }
        return std::nullopt;

    case GameMode::Quit:
        return std::nullopt;
    }
    return std::nullopt;
}

}  // namespace tetromino::tui

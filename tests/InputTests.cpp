#include <vector>

#include "TestFramework.hpp"
#include "tui/Input.hpp"

using namespace tetromino;
using core::Action;
using core::GameMode;
using tui::InputDecoder;
using tui::Key;
using tui::KeyEvent;

namespace {

std::vector<KeyEvent> decode(std::string_view bytes) {
    InputDecoder d;
    std::vector<KeyEvent> out;
    d.feed(bytes);
    d.decode(out);
    d.flushIncomplete(out);
    return out;
}

bool keysAre(const std::vector<KeyEvent>& got, std::vector<Key> expected) {
    if (got.size() != expected.size()) {
        return false;
    }
    for (std::size_t i = 0; i < got.size(); ++i) {
        if (got[i].key != expected[i]) {
            return false;
        }
    }
    return true;
}

}  // namespace

TEST(arrow_keys_csi) {
    CHECK(keysAre(decode("\x1b[A\x1b[B\x1b[C\x1b[D"), {Key::Up, Key::Down, Key::Right, Key::Left}));
}

TEST(arrow_keys_ss3_application_mode) {
    CHECK(keysAre(decode("\x1bOA\x1bOD"), {Key::Up, Key::Left}));
}

TEST(arrow_with_modifiers_still_an_arrow) {
    CHECK(keysAre(decode("\x1b[1;2C"), {Key::Right}));
}

TEST(lone_escape_is_escape_key) {
    CHECK(keysAre(decode("\x1b"), {Key::Escape}));
}

TEST(escape_waits_for_rest_of_sequence) {
    InputDecoder d;
    std::vector<KeyEvent> out;
    d.feed("\x1b");
    d.decode(out);
    CHECK(out.empty());
    CHECK(d.hasIncompleteSequence());
    d.feed("[");
    d.decode(out);
    CHECK(out.empty());
    d.feed("A");
    d.decode(out);
    CHECK(keysAre(out, {Key::Up}));
    CHECK(!d.hasIncompleteSequence());
}

TEST(printable_and_control_keys) {
    const auto ev = decode("a D\r\x03\x1a\x7f");
    CHECK(keysAre(ev, {Key::Character, Key::Space, Key::Character, Key::Enter, Key::Interrupt, Key::Suspend,
                       Key::Backspace}));
    CHECK_EQ(ev[0].ch, 'a');
    CHECK_EQ(ev[2].ch, 'D');
}

TEST(focus_events) {
    CHECK(keysAre(decode("\x1b[I\x1b[O"), {Key::FocusGained, Key::FocusLost}));
}

TEST(unknown_sequences_are_ignored) {
    // F5 (ESC [ 1 5 ~) and Delete (ESC [ 3 ~) aren't bound to anything.
    CHECK(keysAre(decode("\x1b[15~x\x1b[3~"), {Key::Character}));
}

TEST(many_keys_in_one_read) {
    // Key repeat / fast typing delivers several keys in a single read().
    CHECK(keysAre(decode("\x1b[D\x1b[D\x1b[D "), {Key::Left, Key::Left, Key::Left, Key::Space}));
}

TEST(keymap_playing) {
    const auto act = [](KeyEvent e) { return tui::actionFor(e, GameMode::Playing); };
    CHECK(act({Key::Left}) == Action::MoveLeft);
    CHECK(act({Key::Right}) == Action::MoveRight);
    CHECK(act({Key::Down}) == Action::SoftDrop);
    CHECK(act({Key::Up}) == Action::RotateClockwise);
    CHECK(act({Key::Space}) == Action::HardDrop);
    CHECK(act({Key::Character, 'a'}) == Action::MoveLeft);
    CHECK(act({Key::Character, 'D'}) == Action::MoveRight);  // case-insensitive
    CHECK(act({Key::Character, 's'}) == Action::SoftDrop);
    CHECK(act({Key::Character, 'w'}) == Action::RotateClockwise);
    CHECK(act({Key::Character, 'z'}) == Action::RotateCounterClockwise);
    CHECK(act({Key::Character, 'c'}) == Action::Hold);
    CHECK(act({Key::Character, 'p'}) == Action::Pause);
    CHECK(act({Key::Character, 'q'}) == Action::Quit);
    CHECK(act({Key::Escape}) == Action::Pause);
    CHECK(act({Key::Interrupt}) == Action::Quit);
    CHECK(!act({Key::Character, 'r'}).has_value());
}

TEST(keymap_other_modes) {
    CHECK(tui::actionFor({Key::Enter}, GameMode::StartScreen) == Action::Start);
    CHECK(tui::actionFor({Key::Left}, GameMode::StartScreen) == Action::MenuLeft);
    CHECK(tui::actionFor({Key::Right}, GameMode::StartScreen) == Action::MenuRight);
    CHECK(tui::actionFor({Key::Up}, GameMode::StartScreen) == Action::MenuUp);
    CHECK(tui::actionFor({Key::Character, 's'}, GameMode::StartScreen) == Action::MenuDown);
    CHECK(tui::actionFor({Key::Character, 'm'}, GameMode::Paused) == Action::OpenMenu);
    CHECK(tui::actionFor({Key::Character, 'm'}, GameMode::GameOver) == Action::OpenMenu);
    CHECK(!tui::actionFor({Key::Character, 'm'}, GameMode::Playing).has_value());
    CHECK(tui::actionFor({Key::Character, 'p'}, GameMode::Paused) == Action::Pause);
    CHECK(tui::actionFor({Key::Escape}, GameMode::Paused) == Action::Pause);
    CHECK(tui::actionFor({Key::Escape}, GameMode::StartScreen) == Action::Quit);
    CHECK(tui::actionFor({Key::Escape}, GameMode::GameOver) == Action::Quit);
    CHECK(tui::actionFor({Key::Character, 'r'}, GameMode::Paused) == Action::Restart);
    CHECK(tui::actionFor({Key::Character, 'r'}, GameMode::GameOver) == Action::Restart);
    CHECK(!tui::actionFor({Key::Space}, GameMode::GameOver).has_value());
}

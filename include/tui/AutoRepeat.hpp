#pragma once

// Held-key movement, timed by the game rather than the OS.
//
// With only key *presses* to go on, holding ← relies on the OS key repeat:
// typically a 500 ms pause, then ~30 moves a second. Terminals that also
// report key *releases* (kitty keyboard protocol) let us do what dedicated
// falling-block games do:
//
//   press ←     move once immediately (the caller does this)
//   hold        after `delay` ("DAS"), move again, then every `interval`
//               ("ARR"); an interval of 0 slides straight to the wall
//   release     stop
//
// If both ← and → are held, the most recently pressed one wins; releasing it
// hands control back to the other. Soft drop repeats every `softDrop` for as
// long as it's held. Everything here is pure: time goes in, actions come out.

#include <optional>
#include <vector>

#include "core/Types.hpp"

namespace tetromino::tui {

struct RepeatTiming {
    core::Duration delay = std::chrono::milliseconds{167};
    core::Duration interval = std::chrono::milliseconds{33};
    core::Duration softDrop = std::chrono::milliseconds{33};
};

class AutoRepeat {
public:
    explicit AutoRepeat(RepeatTiming timing = {}) : timing_(timing) {}

    // MoveLeft, MoveRight and SoftDrop are tracked; other actions are ignored.
    void press(core::Action action);
    void release(core::Action action);
    // Forget every held key (focus lost, paused, game over).
    void releaseAll();

    // Advance time by `dt` and append the repeated moves that are due.
    void update(core::Duration dt, std::vector<core::Action>& out);

    [[nodiscard]] bool anyHeld() const { return left_.down || right_.down || drop_.down; }
    [[nodiscard]] const RepeatTiming& timing() const { return timing_; }

private:
    struct Held {
        bool down = false;
        bool repeating = false;  // past the initial delay
        core::Duration timer{};
    };

    Held* heldFor(core::Action action);  // nullptr for untracked actions
    Held& side(core::Action direction);  // MoveLeft -> left_, otherwise right_
    void restart(Held& key);

    RepeatTiming timing_;
    Held left_;
    Held right_;
    Held drop_;
    std::optional<core::Action> horizontal_;  // the direction currently in control
};

}  // namespace tetromino::tui

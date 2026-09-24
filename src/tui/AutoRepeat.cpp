#include "tui/AutoRepeat.hpp"

namespace tetromino::tui {

using core::Action;
using core::Duration;

namespace {

// Upper bounds on moves emitted by one update(): enough to cross any board,
// and protection against a burst after a long stall.
constexpr int kMaxShiftsPerUpdate = 32;
constexpr int kMaxDropsPerUpdate = 32;

}  // namespace

AutoRepeat::Held* AutoRepeat::heldFor(Action action) {
    switch (action) {
    case Action::MoveLeft: return &left_;
    case Action::MoveRight: return &right_;
    case Action::SoftDrop: return &drop_;
    default: return nullptr;
    }
}

void AutoRepeat::restart(Held& key) {
    key.repeating = false;
    key.timer = Duration::zero();
}

void AutoRepeat::press(Action action) {
    Held* key = heldFor(action);
    if (key == nullptr) {
        return;
    }
    key->down = true;
    restart(*key);
    if (action != Action::SoftDrop) {
        horizontal_ = action;
    }
}

void AutoRepeat::release(Action action) {
    Held* key = heldFor(action);
    if (key == nullptr) {
        return;
    }
    key->down = false;
    restart(*key);
    if (horizontal_ == action) {
        // Hand control back to the other direction if it's still held; it
        // starts its delay again, like a fresh press.
        const Action other = action == Action::MoveLeft ? Action::MoveRight : Action::MoveLeft;
        Held& otherKey = *heldFor(other);
        if (otherKey.down) {
            horizontal_ = other;
            restart(otherKey);
        } else {
            horizontal_.reset();
        }
    }
}

void AutoRepeat::releaseAll() {
    left_ = Held{};
    right_ = Held{};
    drop_ = Held{};
    horizontal_.reset();
}

void AutoRepeat::update(Duration dt, std::vector<Action>& out) {
    if (horizontal_) {
        Held& key = *heldFor(*horizontal_);
        key.timer += dt;
        int emitted = 0;
        if (!key.repeating && key.timer >= timing_.delay) {
            key.repeating = true;
            key.timer -= timing_.delay;
            out.push_back(*horizontal_);
            ++emitted;
        }
        if (key.repeating) {
            if (timing_.interval <= Duration::zero()) {
                // Instant repeat: slide to the wall (extra moves just fail).
                for (; emitted < kMaxShiftsPerUpdate; ++emitted) {
                    out.push_back(*horizontal_);
                }
                key.timer = Duration::zero();
            } else {
                while (key.timer >= timing_.interval && emitted < kMaxShiftsPerUpdate) {
                    key.timer -= timing_.interval;
                    out.push_back(*horizontal_);
                    ++emitted;
                }
                if (emitted == kMaxShiftsPerUpdate) {
                    key.timer = Duration::zero();
                }
            }
        }
    }

    if (drop_.down && timing_.softDrop > Duration::zero()) {
        drop_.timer += dt;
        int emitted = 0;
        while (drop_.timer >= timing_.softDrop && emitted < kMaxDropsPerUpdate) {
            drop_.timer -= timing_.softDrop;
            out.push_back(Action::SoftDrop);
            ++emitted;
        }
        if (emitted == kMaxDropsPerUpdate) {
            drop_.timer = Duration::zero();
        }
    }
}

}  // namespace tetromino::tui

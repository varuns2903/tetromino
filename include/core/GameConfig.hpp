#pragma once

// Tunable game rules. Defaults follow common modern falling-block conventions.

#include <chrono>

#include "game/Scoring.hpp"

namespace tetromino::core {

using Duration = std::chrono::nanoseconds;

// Time for a piece to fall one row at a given level.
using GravityCurve = Duration (*)(int level);

// The widely used modern curve: (0.8 - (level - 1) * 0.007) ^ (level - 1)
// seconds per row. Level 1 = 1 s/row, level 10 ~ 0.064 s, level 15 ~ 7 ms,
// beyond that pieces effectively teleport to the floor ("20G").
[[nodiscard]] Duration standardGravity(int level);

struct GameConfig {
    int startLevel = 1;

    // How long a grounded piece may still be moved/rotated before locking.
    Duration lockDelay = std::chrono::milliseconds{500};
    // Each successful move/rotate while grounded restarts the lock timer, at
    // most this many times per piece ("move reset"), so a piece can't be
    // spun forever.
    int maxLockResets = 15;

    // Pause after completing lines, used for the clear animation.
    Duration lineClearDelay = std::chrono::milliseconds{280};

    GravityCurve gravity = &standardGravity;

    game::ScoringRules scoring{};
};

}  // namespace tetromino::core

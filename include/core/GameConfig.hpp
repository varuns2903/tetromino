#pragma once

// Tunable game rules. Defaults follow common modern falling-block conventions.

#include <chrono>
#include <cstddef>
#include <optional>

#include "game/Board.hpp"
#include "game/Scoring.hpp"

#include "core/Types.hpp"

namespace tetromino::core {

// Time for a piece to fall one row at a given level.
using GravityCurve = Duration (*)(int level);

// The widely used modern curve: (0.8 - (level - 1) * 0.007) ^ (level - 1)
// seconds per row. Level 1 = 1 s/row, level 10 ~ 0.064 s, level 15 ~ 7 ms,
// beyond that pieces effectively teleport to the floor ("20G").
[[nodiscard]] Duration standardGravity(int level);

struct GameConfig {
    int startLevel = 1;

    // Playfield size (visible rows; the hidden spawn buffer is added on top).
    int boardWidth = game::Board::kDefaultWidth;
    int boardHeight = game::Board::kDefaultVisibleHeight;

    // Difficulty knobs. The defaults are the most forgiving full-featured
    // rules; presets (core/Presets.hpp) adjust them.
    bool holdEnabled = true;
    int previewCount = 5;
    bool ghostEnabled = true;
    // Multiplies the gravity interval: 0.5 = pieces fall twice as fast.
    double gravityScale = 1.0;

    // How long a grounded piece may still be moved/rotated before locking.
    Duration lockDelay = std::chrono::milliseconds{500};
    // Each successful move/rotate while grounded restarts the lock timer, at
    // most this many times per piece ("move reset"), so a piece can't be
    // spun forever.
    int maxLockResets = 15;

    // Pause after completing lines, used for the clear animation.
    Duration lineClearDelay = std::chrono::milliseconds{280};

    // "3, 2, 1" before play resumes after a pause (zero: resume at once).
    Duration resumeCountdown = std::chrono::seconds{3};

    // Timed modes: the game ends after this much playing time.
    std::optional<Duration> timeLimit;

    GravityCurve gravity = &standardGravity;

    game::ScoringRules scoring{};
};

// `base` with the board size, difficulty and game type presets at the given
// indices (into kBoardSizes / kDifficulties / kGameTypes) applied.
// Out-of-range indices fall back to the defaults.
[[nodiscard]] GameConfig configFor(const GameConfig& base, std::size_t boardSize, std::size_t difficulty,
                                   std::size_t gameType = 0);

}  // namespace tetromino::core

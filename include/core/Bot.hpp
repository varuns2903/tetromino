#pragma once

// A computer player, used for the attract-mode demo.
//
// For each new piece the bot tries every rotation and column on a copy of the
// game, scores the resulting board, and then presses the keys that lead to the
// best one, one at a time, like a player would. The scoring is the classic
// heuristic for stacking games: fewer holes, a lower and flatter stack, and
// cleared lines are good.
//
// It only ever talks to the game through apply(Action), so it can't cheat and
// it can't desync from what's on screen.

#include <deque>
#include <optional>

#include "core/Game.hpp"
#include "core/Types.hpp"

namespace tetromino::core {

class Bot {
public:
    // The next key to press, or nullopt if there's nothing to do right now
    // (no active piece, e.g. during a line clear, or the game isn't running).
    [[nodiscard]] std::optional<Action> nextAction(const Game& game);

    void reset() { plan_.clear(); }

private:
    void plan(const Game& game);

    std::deque<Action> plan_;
};

// How good a board is for the bot (higher is better). Exposed for tests.
[[nodiscard]] double evaluateBoard(const game::Board& board, int linesCleared);

}  // namespace tetromino::core

#pragma once

// A complete, read-only snapshot of the game, as seen by a front end.
//
// Game mutates it; renderers only ever get a `const GameState&`. Everything a
// renderer needs is in here, so an ANSI renderer, an ncurses renderer or a
// test can all consume the same data without touching the engine.

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "core/Types.hpp"
#include "game/Board.hpp"
#include "game/Piece.hpp"

namespace tetromino::core {

inline constexpr std::size_t kPreviewCount = 5;

struct Stats {
    std::uint64_t score = 0;
    int lines = 0;
    int level = 1;
    int pieces = 0;
    int quads = 0;
};

struct GameState {
    GameMode mode = GameMode::StartScreen;

    game::Board board;
    std::optional<game::Piece> active;  // empty between pieces (line-clear delay)

    std::optional<PieceType> held;
    bool holdAvailable = true;

    std::array<PieceType, kPreviewCount> preview{};

    Stats stats;

    // Rows being cleared right now. While non-empty the rows are still on the
    // board (so they can be animated) and no piece is active.
    std::vector<int> clearingRows;
    float clearProgress = 0.0F;  // 0 -> 1 over the line-clear delay

    // Where the active piece would land. Computed, never stored in the board.
    [[nodiscard]] std::optional<game::Piece> ghost() const;

    [[nodiscard]] bool isClearingLines() const { return !clearingRows.empty(); }
};

}  // namespace tetromino::core

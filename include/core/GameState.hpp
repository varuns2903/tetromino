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

#include "core/Presets.hpp"
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

// Which features the current game has; set from the difficulty when a game
// starts. Front ends use this to hide what's disabled.
struct Rules {
    bool holdEnabled = true;
    int previewCount = static_cast<int>(kPreviewCount);
    bool ghostEnabled = true;
};

// The start-screen setup menu: which row has focus and what's selected.
struct Setup {
    enum class Field : std::uint8_t { BoardSize, Difficulty };
    static constexpr int kFieldCount = 2;

    Field focus = Field::BoardSize;
    std::size_t boardSize = kDefaultBoardSize;    // index into kBoardSizes
    std::size_t difficulty = kDefaultDifficulty;  // index into kDifficulties
};

struct GameState {
    GameMode mode = GameMode::StartScreen;
    Setup setup;
    Rules rules;

    game::Board board;
    std::optional<game::Piece> active;  // empty between pieces (line-clear delay)

    std::optional<PieceType> held;
    bool holdAvailable = true;

    // The next pieces, soonest first. Always filled; rules.previewCount says
    // how many the player is allowed to see.
    std::array<PieceType, kPreviewCount> preview{};

    Stats stats;

    // Rows being cleared right now. While non-empty the rows are still on the
    // board (so they can be animated) and no piece is active.
    std::vector<int> clearingRows;
    float clearProgress = 0.0F;  // 0 -> 1 over the line-clear delay

    // Where the active piece would land. Computed, never stored in the board.
    // Empty when there's no active piece or the ghost is disabled.
    [[nodiscard]] std::optional<game::Piece> ghost() const;

    [[nodiscard]] bool isClearingLines() const { return !clearingRows.empty(); }
};

}  // namespace tetromino::core

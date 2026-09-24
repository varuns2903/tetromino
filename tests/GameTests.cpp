// Integration tests for the Game engine: actions + time in, state out.

#include <chrono>

#include "TestFramework.hpp"
#include "core/Game.hpp"
#include "game/Collision.hpp"

using namespace tetromino;
using core::Action;
using core::Game;
using core::GameMode;
using core::PieceType;
using game::Board;
using namespace std::chrono_literals;

namespace {

constexpr int kBottom = Board::kHeight - 1;

// Find a seed whose first piece is `type`, and start a game on `board`.
Game gameStartingWith(PieceType type, const Board& board = Board{}, core::GameConfig config = {}) {
    for (std::uint64_t seed = 0;; ++seed) {
        Game game{seed, config};
        game.startWithBoard(board);
        if (game.state().active && game.state().active->type == type) {
            return game;
        }
    }
}

void fillRow(Board& board, int y, int gapFrom, int gapTo) {
    for (int x = 0; x < Board::kWidth; ++x) {
        if (x < gapFrom || x > gapTo) {
            board.set({x, y}, core::CellType::Z);
        }
    }
}

void finishClear(Game& game) { game.update(core::GameConfig{}.lineClearDelay + 1ms); }

}  // namespace

TEST(starts_on_start_screen_and_enters_play) {
    Game game{1};
    CHECK(game.mode() == GameMode::StartScreen);
    CHECK(!game.state().active);
    game.apply(Action::Start);
    CHECK(game.mode() == GameMode::Playing);
    CHECK(game.state().active.has_value());
    CHECK_EQ(game.state().stats.level, 1);
}

TEST(gravity_moves_piece_one_row_per_interval_at_level_1) {
    Game game = gameStartingWith(PieceType::T);
    const int y0 = game.state().active->position.y;
    game.update(999ms);
    CHECK_EQ(game.state().active->position.y, y0);
    game.update(2ms);
    CHECK_EQ(game.state().active->position.y, y0 + 1);
}

TEST(gravity_is_independent_of_update_granularity) {
    Game coarse = gameStartingWith(PieceType::T);
    Game fine = gameStartingWith(PieceType::T);
    coarse.update(3500ms);
    for (int i = 0; i < 350; ++i) {
        fine.update(10ms);
    }
    CHECK_EQ(coarse.state().active->position.y, fine.state().active->position.y);
}

TEST(soft_drop_moves_down_and_scores_one_point) {
    Game game = gameStartingWith(PieceType::T);
    const int y0 = game.state().active->position.y;
    game.apply(Action::SoftDrop);
    CHECK_EQ(game.state().active->position.y, y0 + 1);
    CHECK_EQ(game.state().stats.score, 1u);
}

TEST(hard_drop_locks_immediately_and_scores_two_per_cell) {
    Game game = gameStartingWith(PieceType::O);
    const int distance = game::dropDistance(game.state().board, *game.state().active);
    game.apply(Action::HardDrop);
    CHECK_EQ(game.state().stats.score, static_cast<std::uint64_t>(2 * distance));
    CHECK_EQ(game.state().stats.pieces, 1);
    CHECK(game.state().board.isOccupied({4, kBottom}));
    CHECK(game.state().board.isOccupied({5, kBottom}));
}

TEST(grounded_piece_locks_after_lock_delay) {
    Game game = gameStartingWith(PieceType::O);
    while (game.state().active && !game::isGrounded(game.state().board, *game.state().active)) {
        game.apply(Action::SoftDrop);
    }
    game.update(400ms);
    CHECK_EQ(game.state().stats.pieces, 0);  // still inside the 500 ms window
    game.update(150ms);
    CHECK_EQ(game.state().stats.pieces, 1);
}

TEST(moving_a_grounded_piece_resets_lock_delay) {
    Game game = gameStartingWith(PieceType::O);
    while (!game::isGrounded(game.state().board, *game.state().active)) {
        game.apply(Action::SoftDrop);
    }
    game.update(400ms);
    game.apply(Action::MoveLeft);
    game.update(400ms);
    CHECK_EQ(game.state().stats.pieces, 0);
    game.update(150ms);
    CHECK_EQ(game.state().stats.pieces, 1);
}

TEST(single_line_clear_scores_and_removes_row) {
    Board board;
    fillRow(board, kBottom, 3, 6);  // gap exactly where a flat I lands
    Game game = gameStartingWith(PieceType::I, board);
    const int distance = game::dropDistance(game.state().board, *game.state().active);
    game.apply(Action::HardDrop);

    // Row is flagged for clearing, but still on the board for the animation.
    CHECK_EQ(game.state().clearingRows.size(), 1u);
    CHECK(!game.state().active);
    CHECK_EQ(game.state().stats.lines, 1);
    CHECK_EQ(game.state().stats.score, 100u + static_cast<std::uint64_t>(2 * distance));

    finishClear(game);
    CHECK(game.state().board.isEmpty());
    CHECK(game.state().active.has_value());
}

TEST(quad_with_vertical_i_scores_800) {
    Board board;
    for (int y = kBottom - 3; y <= kBottom; ++y) {
        fillRow(board, y, 0, 0);  // well in column 0
    }
    Game game = gameStartingWith(PieceType::I, board);
    game.apply(Action::RotateClockwise);  // vertical, in column 5
    for (int i = 0; i < 6; ++i) {
        game.apply(Action::MoveLeft);
    }
    CHECK_EQ(game::cellsOf(*game.state().active)[0].x, 0);
    const int distance = game::dropDistance(game.state().board, *game.state().active);
    game.apply(Action::HardDrop);
    CHECK_EQ(game.state().stats.lines, 4);
    CHECK_EQ(game.state().stats.quads, 1);
    CHECK_EQ(game.state().stats.score, 800u + static_cast<std::uint64_t>(2 * distance));
    finishClear(game);
    CHECK(game.state().board.isEmpty());
}

TEST(line_clear_score_uses_level) {
    core::GameConfig config;
    config.startLevel = 3;
    Board board;
    fillRow(board, kBottom, 3, 6);
    Game game = gameStartingWith(PieceType::I, board, config);
    CHECK_EQ(game.state().stats.level, 3);
    const int distance = game::dropDistance(game.state().board, *game.state().active);
    game.apply(Action::HardDrop);
    CHECK_EQ(game.state().stats.score, 300u + static_cast<std::uint64_t>(2 * distance));
}

TEST(level_increases_with_cleared_lines) {
    core::GameConfig config;
    config.scoring.linesPerLevel = 1;  // one line per level keeps the setup tiny
    Board board;
    fillRow(board, kBottom, 3, 6);
    Game game = gameStartingWith(PieceType::I, board, config);
    game.apply(Action::HardDrop);
    CHECK_EQ(game.state().stats.lines, 1);
    CHECK_EQ(game.state().stats.level, 2);
}

// --- Milestone 7: hold, preview, pause, restart, game over -----------------

TEST(preview_shows_the_pieces_that_actually_spawn) {
    Game game{99};
    game.apply(Action::Start);
    const auto preview = game.state().preview;
    for (std::size_t i = 0; i < 3; ++i) {
        game.apply(Action::HardDrop);
        CHECK(game.state().active->type == preview[i]);
    }
}

TEST(first_hold_stores_piece_and_takes_next_from_queue) {
    Game game = gameStartingWith(PieceType::T);
    const PieceType next = game.state().preview[0];
    game.apply(Action::Hold);
    CHECK(game.state().held == PieceType::T);
    CHECK(game.state().active->type == next);
    CHECK(!game.state().holdAvailable);
}

TEST(hold_only_once_per_piece) {
    Game game = gameStartingWith(PieceType::T);
    game.apply(Action::Hold);
    const PieceType afterFirstHold = game.state().active->type;
    game.apply(Action::Hold);  // ignored
    CHECK(game.state().held == PieceType::T);
    CHECK(game.state().active->type == afterFirstHold);
}

TEST(hold_swaps_after_next_piece_locks) {
    Game game = gameStartingWith(PieceType::T);
    game.apply(Action::Hold);
    game.apply(Action::HardDrop);  // lock; hold becomes available again
    CHECK(game.state().holdAvailable);
    const PieceType current = game.state().active->type;
    game.apply(Action::Hold);
    CHECK(game.state().active->type == PieceType::T);
    CHECK(game.state().held == current);
    // A swapped-in piece starts from the spawn position.
    CHECK(game.state().active->position == game::spawnPiece(PieceType::T).position);
}

TEST(pause_freezes_gravity_and_ignores_moves) {
    Game game = gameStartingWith(PieceType::T);
    const game::Piece before = *game.state().active;
    game.apply(Action::Pause);
    CHECK(game.mode() == GameMode::Paused);
    CHECK(!game.needsUpdates());
    game.update(5s);
    game.apply(Action::MoveLeft);
    game.apply(Action::HardDrop);
    CHECK(*game.state().active == before);
    game.apply(Action::Pause);
    CHECK(game.mode() == GameMode::Playing);
}

TEST(stacking_to_the_top_ends_the_game) {
    Game game{5};
    game.apply(Action::Start);
    int guard = 0;
    while (game.mode() == GameMode::Playing && guard++ < 100) {
        game.apply(Action::HardDrop);
        game.update(1s);  // finish any line-clear delay
    }
    CHECK(game.mode() == GameMode::GameOver);
    CHECK(!game.state().active);
}

TEST(restart_after_game_over_resets_everything) {
    Game game{5};
    game.apply(Action::Start);
    while (game.mode() == GameMode::Playing) {
        game.apply(Action::HardDrop);
        game.update(1s);
    }
    game.apply(Action::Restart);
    CHECK(game.mode() == GameMode::Playing);
    CHECK(game.state().board.isEmpty());
    CHECK_EQ(game.state().stats.score, 0u);
    CHECK_EQ(game.state().stats.pieces, 0);
    CHECK(!game.state().held);
    CHECK(game.state().active.has_value());
}

TEST(restart_is_ignored_while_playing) {
    Game game = gameStartingWith(PieceType::T);
    game.apply(Action::SoftDrop);
    game.apply(Action::Restart);
    CHECK_EQ(game.state().stats.score, 1u);
}

TEST(quit_works_from_every_mode) {
    for (int m = 0; m < 4; ++m) {
        Game game{1};
        if (m >= 1) game.apply(Action::Start);
        if (m == 2) game.apply(Action::Pause);
        if (m == 3) {
            while (game.mode() == GameMode::Playing) {
                game.apply(Action::HardDrop);
                game.update(1s);
            }
        }
        game.apply(Action::Quit);
        CHECK(game.mode() == GameMode::Quit);
    }
}

TEST(revision_changes_only_when_state_changes) {
    Game game{1};
    const auto r0 = game.revision();
    game.update(1s);  // start screen: nothing happens
    CHECK_EQ(game.revision(), r0);
    game.apply(Action::Start);
    CHECK(game.revision() != r0);
}

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

constexpr int kBottom = Board::kDefaultHeight - 1;

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
    for (int x = 0; x < Board::kDefaultWidth; ++x) {
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
    fillRow(board, kBottom, 3, 6);                    // gap exactly where a flat I lands
    board.set({9, kBottom - 1}, core::CellType::Z);  // survives: no perfect-clear bonus
    Game game = gameStartingWith(PieceType::I, board);
    const int distance = game::dropDistance(game.state().board, *game.state().active);
    game.apply(Action::HardDrop);

    // Row is flagged for clearing, but still on the board for the animation.
    CHECK_EQ(game.state().clearingRows.size(), 1u);
    CHECK(!game.state().active);
    CHECK_EQ(game.state().stats.lines, 1);
    CHECK_EQ(game.state().stats.score, 100u + static_cast<std::uint64_t>(2 * distance));

    finishClear(game);
    CHECK(game.state().board.at({9, kBottom}) == core::CellType::Z);  // shifted down
    CHECK(game.state().board.isRowEmpty(kBottom - 1));
    CHECK(game.state().active.has_value());
}

TEST(quad_with_vertical_i_scores_800) {
    Board board;
    for (int y = kBottom - 3; y <= kBottom; ++y) {
        fillRow(board, y, 0, 0);  // well in column 0
    }
    board.set({5, kBottom - 4}, core::CellType::Z);  // survives: no perfect-clear bonus
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
    CHECK(game.state().board.at({5, kBottom}) == core::CellType::Z);
}

TEST(line_clear_score_uses_level) {
    core::GameConfig config;
    config.startLevel = 3;
    Board board;
    fillRow(board, kBottom, 3, 6);
    board.set({9, kBottom - 1}, core::CellType::Z);  // no perfect clear
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
    CHECK(game.state().active->position == game::spawnPiece(PieceType::T, Board::kDefaultWidth).position);
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
    // Resuming counts down first; the piece still doesn't move meanwhile.
    CHECK(game.mode() == GameMode::Countdown);
    CHECK(game.needsUpdates());
    game.apply(Action::MoveLeft);
    game.update(2900ms);
    CHECK(game.mode() == GameMode::Countdown);
    CHECK(*game.state().active == before);
    game.update(200ms);
    CHECK(game.mode() == GameMode::Playing);
}

TEST(pause_during_countdown_pauses_again) {
    Game game = gameStartingWith(PieceType::T);
    game.apply(Action::Pause);
    game.apply(Action::Pause);
    game.update(1s);
    game.apply(Action::Pause);
    CHECK(game.mode() == GameMode::Paused);
    game.apply(Action::Pause);
    CHECK(game.state().countdown == core::Duration{3s});  // full countdown again
}

TEST(countdown_can_be_disabled) {
    core::GameConfig config;
    config.resumeCountdown = core::Duration::zero();
    Game game = gameStartingWith(PieceType::T, Board{}, config);
    game.apply(Action::Pause);
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

// --- Setup menu, board sizes and difficulty --------------------------------

namespace {

// Start a game from the menu with the given preset indices.
Game gameFromMenu(std::size_t boardSize, std::size_t difficulty, std::uint64_t seed = 1) {
    Game game{seed};
    game.selectSetup(boardSize, difficulty);
    game.apply(Action::Start);
    return game;
}

std::size_t difficultyIndex(std::string_view name) { return *core::findDifficulty(name); }
std::size_t sizeIndex(std::string_view name) { return *core::findBoardSize(name); }

}  // namespace

TEST(menu_defaults_to_classic_normal) {
    Game game{1};
    CHECK(game.state().setup.boardSize == core::kDefaultBoardSize);
    CHECK(game.state().setup.difficulty == core::kDefaultDifficulty);
    CHECK(core::kBoardSizes[core::kDefaultBoardSize].name == "Classic");
    CHECK(core::kDifficulties[core::kDefaultDifficulty].name == "Normal");
}

TEST(menu_navigation_moves_focus_and_clamps_values) {
    Game game{1};
    using Field = core::Setup::Field;
    CHECK(game.state().setup.focus == Field::GameType);
    game.apply(Action::MenuRight);
    CHECK_EQ(game.state().setup.gameType, 1u);  // 2-Minute
    game.apply(Action::MenuLeft);
    game.apply(Action::MenuDown);
    CHECK(game.state().setup.focus == Field::BoardSize);
    game.apply(Action::MenuRight);
    CHECK_EQ(game.state().setup.boardSize, core::kDefaultBoardSize + 1);
    for (int i = 0; i < 10; ++i) {
        game.apply(Action::MenuRight);
    }
    CHECK_EQ(game.state().setup.boardSize, core::kBoardSizes.size() - 1);  // clamped
    game.apply(Action::MenuDown);
    CHECK(game.state().setup.focus == Field::Difficulty);
    for (int i = 0; i < 10; ++i) {
        game.apply(Action::MenuLeft);
    }
    CHECK_EQ(game.state().setup.difficulty, 0u);  // clamped at Easy
    game.apply(Action::MenuUp);
    CHECK(game.state().setup.focus == Field::BoardSize);
    game.apply(Action::MenuDown);
    game.apply(Action::MenuDown);  // wraps from the last row to the first
    CHECK(game.state().setup.focus == Field::GameType);
    game.apply(Action::MenuUp);    // and back
    CHECK(game.state().setup.focus == Field::Difficulty);
    // Menu actions do nothing outside the start screen.
    game.apply(Action::Start);
    const auto setup = game.state().setup;
    game.apply(Action::MenuLeft);
    CHECK_EQ(game.state().setup.boardSize, setup.boardSize);
}

TEST(start_uses_selected_board_size) {
    for (std::size_t i = 0; i < core::kBoardSizes.size(); ++i) {
        const Game game = gameFromMenu(i, core::kDefaultDifficulty);
        const auto& size = core::kBoardSizes[i];
        CHECK_EQ(game.state().board.width(), size.width);
        CHECK_EQ(game.state().board.visibleHeight(), size.height);
        CHECK(game::fits(game.state().board, *game.state().active));
    }
}

TEST(pieces_spawn_centred_on_wide_board) {
    const Game game = gameFromMenu(sizeIndex("wide"), core::kDefaultDifficulty);
    const auto cells = game::cellsOf(*game.state().active);
    int lo = 99;
    int hi = -1;
    for (const auto c : cells) {
        lo = std::min(lo, c.x);
        hi = std::max(hi, c.x);
    }
    // Left and right margins differ by at most one column.
    CHECK(std::abs(lo - (game.state().board.width() - 1 - hi)) <= 1);
}

TEST(lines_clear_on_non_classic_board) {
    Board board{8, 16};
    for (int x = 0; x < 8; ++x) {
        if (x < 2 || x > 5) {
            board.set({x, board.height() - 1}, core::CellType::Z);
        }
    }
    // Flat I spawns centred on 8 columns: columns 2-5, exactly the gap.
    Game game = gameStartingWith(PieceType::I, board);
    game.apply(Action::HardDrop);
    CHECK_EQ(game.state().stats.lines, 1);
    finishClear(game);
    CHECK(game.state().board.isEmpty());
}

TEST(easy_rules) {
    const Game game = gameFromMenu(core::kDefaultBoardSize, difficultyIndex("easy"));
    const auto& rules = game.state().rules;
    CHECK(rules.holdEnabled);
    CHECK(rules.ghostEnabled);
    CHECK_EQ(rules.previewCount, 5);
    CHECK_EQ(game.state().stats.level, 1);
    CHECK(game.config().lockDelay == std::chrono::nanoseconds{700ms});
}

TEST(normal_rules) {
    const Game game = gameFromMenu(core::kDefaultBoardSize, difficultyIndex("normal"));
    CHECK(game.state().rules.holdEnabled);
    CHECK(game.state().rules.ghostEnabled);
    CHECK_EQ(game.state().rules.previewCount, 3);
}

TEST(hard_disables_hold_and_shows_one_preview) {
    Game game = gameFromMenu(core::kDefaultBoardSize, difficultyIndex("hard"));
    CHECK(!game.state().rules.holdEnabled);
    CHECK_EQ(game.state().rules.previewCount, 1);
    CHECK(game.state().ghost().has_value());
    const auto before = game.state().active->type;
    game.apply(Action::Hold);
    CHECK(!game.state().held.has_value());
    CHECK(game.state().active->type == before);
}

TEST(expert_hides_previews_and_ghost) {
    const Game game = gameFromMenu(core::kDefaultBoardSize, difficultyIndex("expert"));
    CHECK(!game.state().rules.holdEnabled);
    CHECK_EQ(game.state().rules.previewCount, 0);
    CHECK(!game.state().rules.ghostEnabled);
    CHECK(!game.state().ghost().has_value());
    CHECK(game.state().active.has_value());
}

TEST(harder_difficulties_start_higher_and_fall_faster) {
    // Time for the first piece to fall one row, per difficulty.
    const auto firstFall = [](std::size_t difficulty) {
        Game game = gameFromMenu(core::kDefaultBoardSize, difficulty);
        const int y0 = game.state().active->position.y;
        int ms = 0;
        while (game.state().active->position.y == y0 && ms < 5000) {
            game.update(1ms);
            ++ms;
        }
        return ms;
    };
    const int easy = firstFall(difficultyIndex("easy"));
    const int normal = firstFall(difficultyIndex("normal"));
    const int hard = firstFall(difficultyIndex("hard"));
    const int expert = firstFall(difficultyIndex("expert"));
    CHECK(easy > normal);
    CHECK(normal > hard);
    CHECK(hard > expert);
    CHECK_EQ(normal, 1000);  // level 1 baseline
    CHECK_EQ(easy, 1500);

    CHECK_EQ(gameFromMenu(1, difficultyIndex("hard")).state().stats.level, 3);
    CHECK_EQ(gameFromMenu(1, difficultyIndex("expert")).state().stats.level, 5);
}

TEST(restart_keeps_setup_and_menu_allows_changing_it) {
    Game game = gameFromMenu(sizeIndex("small"), difficultyIndex("hard"));
    game.apply(Action::Pause);
    game.apply(Action::Restart);
    CHECK_EQ(game.state().board.width(), 8);
    CHECK(!game.state().rules.holdEnabled);

    game.apply(Action::Pause);
    game.apply(Action::OpenMenu);
    CHECK(game.mode() == GameMode::StartScreen);
    CHECK(!game.state().active);
    game.apply(Action::MenuDown);   // mode -> board
    game.apply(Action::MenuRight);  // Small -> Classic
    game.apply(Action::MenuDown);   // board -> difficulty
    game.apply(Action::MenuLeft);   // Hard -> Normal
    game.apply(Action::Start);
    CHECK_EQ(game.state().board.width(), 10);
    CHECK(game.state().rules.holdEnabled);
}

TEST(config_for_combines_base_and_presets) {
    core::GameConfig base;
    base.startLevel = 4;
    const core::GameConfig c = core::configFor(base, sizeIndex("tall"), difficultyIndex("expert"));
    CHECK_EQ(c.boardWidth, 10);
    CHECK_EQ(c.boardHeight, 24);
    CHECK_EQ(c.startLevel, 8);  // base 4 + expert bonus 4
    CHECK(!c.holdEnabled);
    CHECK_EQ(c.previewCount, 0);
    // Out-of-range indices fall back to the defaults.
    const core::GameConfig d = core::configFor(base, 99, 99);
    CHECK_EQ(d.boardWidth, 10);
    CHECK(d.holdEnabled);
}

TEST(preset_lookup_is_case_insensitive) {
    CHECK(core::findDifficulty("HARD") == difficultyIndex("hard"));
    CHECK(core::findBoardSize("Wide").has_value());
    CHECK(!core::findDifficulty("impossible").has_value());
}

// --- Game types, play time --------------------------------------------------

TEST(play_time_counts_only_while_playing) {
    Game game = gameStartingWith(PieceType::T);
    game.update(1500ms);
    CHECK(game.state().stats.playTime == core::Duration{1500ms});
    game.apply(Action::Pause);
    game.update(10s);
    game.apply(Action::Pause);
    game.update(3s);  // countdown
    CHECK(game.state().stats.playTime == core::Duration{1500ms});
}

TEST(endless_has_no_time_limit) {
    Game game{1};
    game.apply(Action::Start);
    CHECK(!game.state().rules.timeLimit.has_value());
    CHECK(game.state().timeLeft() == core::Duration::zero());
}

TEST(two_minute_mode_ends_when_time_is_up) {
    Game game{1};
    game.selectSetup(core::kDefaultBoardSize, core::kDefaultDifficulty, *core::findGameType("2-minute"));
    game.apply(Action::Start);
    CHECK(game.state().rules.timeLimit == core::Duration{120s});
    // One big step: at most one piece lands, so the stack can't top out.
    game.update(119s);
    CHECK(game.mode() == GameMode::Playing);
    CHECK(game.state().timeLeft() == core::Duration{1s});
    game.update(2s);
    CHECK(game.mode() == GameMode::GameOver);
    CHECK(game.state().endReason == core::EndReason::TimeUp);
    CHECK(game.state().stats.playTime == core::Duration{120s});
    CHECK(game.state().timeLeft() == core::Duration::zero());
}

TEST(topping_out_records_the_reason) {
    Game game{5};
    game.apply(Action::Start);
    while (game.mode() == GameMode::Playing) {
        game.apply(Action::HardDrop);
        game.update(1s);
    }
    CHECK(game.state().endReason == core::EndReason::ToppedOut);
}

TEST(game_type_names_accept_variants) {
    CHECK(core::findGameType("endless") == 0u);
    CHECK(core::findGameType("2-Minute") == 1u);
    CHECK(core::findGameType("2minute") == 1u);
    CHECK(!core::findGameType("marathon").has_value());
}

TEST(clear_counts_by_type) {
    Board board;
    fillRow(board, kBottom, 3, 6);
    board.set({9, kBottom - 1}, core::CellType::Z);
    Game game = gameStartingWith(PieceType::I, board);
    game.apply(Action::HardDrop);
    CHECK_EQ(game.state().stats.singles, 1);
    CHECK_EQ(game.state().stats.doubles, 0);
    CHECK_EQ(game.state().stats.quads, 0);
}

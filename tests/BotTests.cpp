#include <chrono>

#include "TestFramework.hpp"
#include "core/Bot.hpp"

using namespace tetromino;
using core::Action;
using core::Bot;
using core::Game;
using core::GameMode;
using game::Board;
using namespace std::chrono_literals;

TEST(prefers_flat_boards_without_holes) {
    Board flat;
    Board holey;
    for (int x = 0; x < Board::kDefaultWidth; ++x) {
        flat.set({x, Board::kDefaultHeight - 1}, core::CellType::Z);
        holey.set({x, Board::kDefaultHeight - 2}, core::CellType::Z);  // same blocks, one row up
    }
    holey.set({0, Board::kDefaultHeight - 2}, core::CellType::Empty);
    holey.set({0, Board::kDefaultHeight - 1}, core::CellType::Empty);
    CHECK(core::evaluateBoard(flat, 0) > core::evaluateBoard(holey, 0));
    CHECK(core::evaluateBoard(Board{}, 1) > core::evaluateBoard(Board{}, 0));
}

TEST(does_nothing_outside_a_game) {
    Bot bot;
    const Game menu{1};
    CHECK(!bot.nextAction(menu).has_value());
}

TEST(always_ends_a_plan_with_a_hard_drop) {
    Bot bot;
    Game game{7};
    game.apply(Action::Start);
    const int piecesBefore = game.state().stats.pieces;
    for (int i = 0; i < 20 && game.state().stats.pieces == piecesBefore; ++i) {
        const auto action = bot.nextAction(game);
        CHECK(action.has_value());
        game.apply(*action);
    }
    CHECK_EQ(game.state().stats.pieces, piecesBefore + 1);
}

TEST(plays_a_long_game_and_clears_lines) {
    // The demo should look competent: survive a good while and clear lines.
    for (const std::uint64_t seed : {1u, 2u, 3u}) {
        Bot bot;
        Game game{seed};
        game.apply(Action::Start);
        int steps = 0;
        while (game.mode() != GameMode::GameOver && game.state().stats.pieces < 200 && steps < 20000) {
            if (const auto action = bot.nextAction(game)) {
                game.apply(*action);
            }
            game.update(50ms);
            ++steps;
        }
        CHECK(game.state().stats.pieces >= 200);  // didn't top out
        CHECK(game.state().stats.lines >= 60);
    }
}

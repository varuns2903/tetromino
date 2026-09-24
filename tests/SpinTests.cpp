// Spin detection (three-corner rule) and scoring a spin through the engine.

#include <deque>
#include <map>
#include <tuple>

#include "TestFramework.hpp"
#include "core/Game.hpp"
#include "game/Collision.hpp"
#include "game/Spin.hpp"

using namespace tetromino;
using core::Action;
using core::CellType;
using core::PieceType;
using core::Point;
using core::Rotation;
using game::Board;
using game::Piece;
using game::SpinKind;

namespace {

constexpr int kBottom = Board::kDefaultHeight - 1;

// A T pointing down, box at (3, 20): centre (4, 21), corners
// TL (3, 20), TR (5, 20), BR (5, 22), BL (3, 22). Front = BR + BL.
const Piece kDownT{PieceType::T, Rotation::Reverse, {3, 20}};

}  // namespace

TEST(only_rotated_t_pieces_spin) {
    Board b;
    b.set({3, 20}, CellType::Z);
    b.set({3, 22}, CellType::Z);
    b.set({5, 22}, CellType::Z);
    CHECK(game::detectSpin(b, kDownT, true, 0) == SpinKind::Full);
    CHECK(game::detectSpin(b, kDownT, false, 0) == SpinKind::None);  // last move wasn't a rotation
    const Piece j{PieceType::J, Rotation::Reverse, {3, 20}};
    CHECK(game::detectSpin(b, j, true, 0) == SpinKind::None);
}

TEST(full_spin_needs_both_front_corners) {
    Board b;
    b.set({3, 20}, CellType::Z);  // TL (back)
    b.set({5, 20}, CellType::Z);  // TR (back)
    b.set({5, 22}, CellType::Z);  // BR (front)
    CHECK(game::detectSpin(b, kDownT, true, 0) == SpinKind::Mini);
    b.set({3, 22}, CellType::Z);  // BL (front) too
    CHECK(game::detectSpin(b, kDownT, true, 0) == SpinKind::Full);
}

TEST(last_resort_kick_upgrades_mini_to_full) {
    Board b;
    b.set({3, 20}, CellType::Z);
    b.set({5, 20}, CellType::Z);
    b.set({5, 22}, CellType::Z);
    CHECK(game::detectSpin(b, kDownT, true, 4) == SpinKind::Full);
}

TEST(fewer_than_three_corners_is_not_a_spin) {
    Board b;
    b.set({3, 22}, CellType::Z);
    b.set({5, 22}, CellType::Z);
    CHECK(game::detectSpin(b, kDownT, true, 0) == SpinKind::None);
}

TEST(walls_and_floor_count_as_corners) {
    // T pointing right against the left wall: box x = -1, so both left
    // corners are outside the board.
    Board b;
    const Piece t{PieceType::T, Rotation::Right, {-1, 20}};
    CHECK(game::fits(b, t));
    CHECK(game::detectSpin(b, t, true, 0) == SpinKind::None);  // 2 corners (wall)
    b.set({1, 22}, CellType::Z);                                // BR (front)
    CHECK(game::detectSpin(b, t, true, 0) == SpinKind::Mini);
    b.set({1, 20}, CellType::Z);                                // TR (front)
    CHECK(game::detectSpin(b, t, true, 0) == SpinKind::Full);
}

// End to end: rotate a T into a covered slot and clear two lines.
//
//   col: 0 1 2 3 4 5 6 7 8 9
//   y21  . . . # . . . . . .     roof over the slot
//   y22  # # # . . . # # # #
//   y23  # # # # . # # # # #
TEST(spin_double_through_the_engine) {
    Board board;
    for (int x = 0; x < Board::kDefaultWidth; ++x) {
        if (x < 3 || x > 5) {
            board.set({x, kBottom - 1}, CellType::Z);
        }
        if (x != 4) {
            board.set({x, kBottom}, CellType::Z);
        }
    }
    board.set({3, kBottom - 2}, CellType::Z);
    board.set({0, kBottom - 2}, CellType::Z);  // keep the board non-empty afterwards

    core::Game start{1};
    for (std::uint64_t seed = 1; seed < 64; ++seed) {
        core::Game g{seed};
        g.startWithBoard(board);
        if (g.state().active && g.state().active->type == PieceType::T) {
            start = g;
            break;
        }
    }

    // The T ends up pointing down with its centre at (4, 22).
    const Piece target{PieceType::T, Rotation::Reverse, {3, kBottom - 2}};
    CHECK(game::fits(board, target));

    // Breadth-first search over key presses (no gravity) for a sequence
    // whose final press is a rotation that lands exactly on the target.
    using Key = std::tuple<int, int, int>;
    const auto keyOf = [](const Piece& p) { return Key{p.position.x, p.position.y, static_cast<int>(p.rotation)}; };
    std::map<Key, bool> seen;
    std::deque<core::Game> queue{start};
    seen[keyOf(*start.state().active)] = true;
    const Action moves[] = {Action::MoveLeft, Action::MoveRight, Action::SoftDrop, Action::RotateClockwise,
                            Action::RotateCounterClockwise};
    std::optional<core::Game> found;
    while (!queue.empty() && !found) {
        const core::Game g = queue.front();
        queue.pop_front();
        for (const Action a : moves) {
            core::Game next = g;
            next.apply(a);
            if (!next.state().active) {
                continue;
            }
            const Piece& p = *next.state().active;
            const bool rotation = a == Action::RotateClockwise || a == Action::RotateCounterClockwise;
            if (rotation && p == target) {
                found = next;
                break;
            }
            if (!seen[keyOf(p)]) {
                seen[keyOf(p)] = true;
                queue.push_back(next);
            }
        }
    }
    CHECK(found.has_value());

    core::Game g = *found;
    const auto before = g.state().stats.score;
    g.apply(Action::HardDrop);  // already resting: stays where it was rotated
    CHECK_EQ(g.state().stats.lines, 2);
    CHECK_EQ(g.state().stats.spins, 1);
    CHECK(g.state().feedback.spin == SpinKind::Full);
    CHECK_EQ(g.state().feedback.lines, 2);
    CHECK_EQ(g.state().stats.score - before, 1200u);  // spin double at level 1, no drop points
}

TEST(perfect_clear_through_the_engine) {
    Board board;
    for (int x = 0; x < Board::kDefaultWidth; ++x) {
        if (x < 3 || x > 6) {
            board.set({x, kBottom}, CellType::Z);
        }
    }
    for (std::uint64_t seed = 1; seed < 64; ++seed) {
        core::Game g{seed};
        g.startWithBoard(board);
        if (g.state().active->type != PieceType::I) {
            continue;
        }
        const int distance = game::dropDistance(g.state().board, *g.state().active);
        g.apply(Action::HardDrop);
        CHECK_EQ(g.state().stats.perfectClears, 1);
        CHECK(g.state().feedback.perfectClear);
        CHECK_EQ(g.state().feedback.id, 1u);
        CHECK_EQ(g.state().stats.score, 900u + static_cast<std::uint64_t>(2 * distance));
        break;
    }
}

TEST(feedback_records_the_clear_and_ages) {
    Board board;
    for (int x = 0; x < Board::kDefaultWidth; ++x) {
        if (x < 3 || x > 6) {
            board.set({x, kBottom}, CellType::Z);
        }
    }
    board.set({9, kBottom - 1}, CellType::Z);  // no perfect clear
    // Seeds deal every piece type within the first seven, so this ends.
    for (std::uint64_t seed = 1; seed < 64; ++seed) {
        core::Game g{seed};
        g.startWithBoard(board);
        if (g.state().active->type != PieceType::I) {
            continue;
        }
        CHECK_EQ(g.state().feedback.id, 0u);
        g.apply(Action::HardDrop);
        const core::Feedback& f = g.state().feedback;
        CHECK_EQ(f.id, 1u);
        CHECK_EQ(f.lines, 1);
        CHECK(!f.perfectClear);
        CHECK(f.age == core::Duration::zero());
        g.update(std::chrono::milliseconds{500});
        CHECK(g.state().feedback.age == core::Duration{std::chrono::milliseconds{500}});
        g.apply(Action::Pause);
        g.update(std::chrono::seconds{5});  // paused: doesn't age
        CHECK(g.state().feedback.age == core::Duration{std::chrono::milliseconds{500}});
        return;
    }
    CHECK(false);  // no suitable seed
}

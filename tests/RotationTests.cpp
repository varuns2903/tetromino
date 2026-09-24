#include <algorithm>
#include <set>
#include <utility>

#include "TestFramework.hpp"
#include "game/Collision.hpp"
#include "game/Rotation.hpp"

using namespace tetromino;
using core::PieceType;
using core::Point;
using core::Rotation;
using core::RotationDirection;
using game::Board;
using game::Piece;

namespace {

constexpr auto CW = RotationDirection::Clockwise;
constexpr auto CCW = RotationDirection::CounterClockwise;

std::set<std::pair<int, int>> asSet(const game::PieceCells& cells) {
    std::set<std::pair<int, int>> s;
    for (const Point p : cells) {
        s.insert({p.x, p.y});
    }
    return s;
}

bool shapeIs(PieceType type, Rotation r, std::set<std::pair<int, int>> expected) {
    return asSet(game::shapeOf(type, r)) == expected;
}

}  // namespace

// --- Shape tables: every piece, every rotation state ----------------------
// Written out by hand from the SRS reference pictures (y down, box origin
// top-left), so a bug in the generated tables can't hide.

TEST(i_piece_all_states) {
    CHECK(shapeIs(PieceType::I, Rotation::Spawn, {{0, 1}, {1, 1}, {2, 1}, {3, 1}}));
    CHECK(shapeIs(PieceType::I, Rotation::Right, {{2, 0}, {2, 1}, {2, 2}, {2, 3}}));
    CHECK(shapeIs(PieceType::I, Rotation::Reverse, {{0, 2}, {1, 2}, {2, 2}, {3, 2}}));
    CHECK(shapeIs(PieceType::I, Rotation::Left, {{1, 0}, {1, 1}, {1, 2}, {1, 3}}));
}

TEST(o_piece_all_states_identical) {
    for (const auto r : {Rotation::Spawn, Rotation::Right, Rotation::Reverse, Rotation::Left}) {
        CHECK(shapeIs(PieceType::O, r, {{0, 0}, {1, 0}, {0, 1}, {1, 1}}));
    }
}

TEST(t_piece_all_states) {
    CHECK(shapeIs(PieceType::T, Rotation::Spawn, {{1, 0}, {0, 1}, {1, 1}, {2, 1}}));
    CHECK(shapeIs(PieceType::T, Rotation::Right, {{1, 0}, {1, 1}, {2, 1}, {1, 2}}));
    CHECK(shapeIs(PieceType::T, Rotation::Reverse, {{0, 1}, {1, 1}, {2, 1}, {1, 2}}));
    CHECK(shapeIs(PieceType::T, Rotation::Left, {{1, 0}, {0, 1}, {1, 1}, {1, 2}}));
}

TEST(s_piece_all_states) {
    CHECK(shapeIs(PieceType::S, Rotation::Spawn, {{1, 0}, {2, 0}, {0, 1}, {1, 1}}));
    CHECK(shapeIs(PieceType::S, Rotation::Right, {{1, 0}, {1, 1}, {2, 1}, {2, 2}}));
    CHECK(shapeIs(PieceType::S, Rotation::Reverse, {{1, 1}, {2, 1}, {0, 2}, {1, 2}}));
    CHECK(shapeIs(PieceType::S, Rotation::Left, {{0, 0}, {0, 1}, {1, 1}, {1, 2}}));
}

TEST(z_piece_all_states) {
    CHECK(shapeIs(PieceType::Z, Rotation::Spawn, {{0, 0}, {1, 0}, {1, 1}, {2, 1}}));
    CHECK(shapeIs(PieceType::Z, Rotation::Right, {{2, 0}, {1, 1}, {2, 1}, {1, 2}}));
    CHECK(shapeIs(PieceType::Z, Rotation::Reverse, {{0, 1}, {1, 1}, {1, 2}, {2, 2}}));
    CHECK(shapeIs(PieceType::Z, Rotation::Left, {{1, 0}, {0, 1}, {1, 1}, {0, 2}}));
}

TEST(j_piece_all_states) {
    CHECK(shapeIs(PieceType::J, Rotation::Spawn, {{0, 0}, {0, 1}, {1, 1}, {2, 1}}));
    CHECK(shapeIs(PieceType::J, Rotation::Right, {{1, 0}, {2, 0}, {1, 1}, {1, 2}}));
    CHECK(shapeIs(PieceType::J, Rotation::Reverse, {{0, 1}, {1, 1}, {2, 1}, {2, 2}}));
    CHECK(shapeIs(PieceType::J, Rotation::Left, {{1, 0}, {1, 1}, {0, 2}, {1, 2}}));
}

TEST(l_piece_all_states) {
    CHECK(shapeIs(PieceType::L, Rotation::Spawn, {{2, 0}, {0, 1}, {1, 1}, {2, 1}}));
    CHECK(shapeIs(PieceType::L, Rotation::Right, {{1, 0}, {1, 1}, {1, 2}, {2, 2}}));
    CHECK(shapeIs(PieceType::L, Rotation::Reverse, {{0, 1}, {1, 1}, {2, 1}, {0, 2}}));
    CHECK(shapeIs(PieceType::L, Rotation::Left, {{0, 0}, {1, 0}, {1, 1}, {1, 2}}));
}

// --- Rotation behaviour -----------------------------------------------------

TEST(rotation_state_cycles) {
    CHECK(core::rotated(Rotation::Spawn, CW) == Rotation::Right);
    CHECK(core::rotated(Rotation::Left, CW) == Rotation::Spawn);
    CHECK(core::rotated(Rotation::Spawn, CCW) == Rotation::Left);
    CHECK(core::rotated(Rotation::Right, CCW) == Rotation::Spawn);
}

TEST(four_rotations_return_to_start_in_open_space) {
    const Board board;
    for (const PieceType type : core::kAllPieceTypes) {
        for (const auto dir : {CW, CCW}) {
            Piece p{type, Rotation::Spawn, {3, 10}};
            const Piece start = p;
            for (int i = 0; i < 4; ++i) {
                const auto r = game::tryRotate(board, p, dir);
                CHECK(r.has_value());
                p = *r;
            }
            CHECK(p == start);
        }
    }
}

TEST(rotation_in_open_space_uses_no_kick) {
    const Board board;
    for (const PieceType type : core::kAllPieceTypes) {
        const Piece p{type, Rotation::Spawn, {3, 10}};
        const auto r = game::tryRotate(board, p, CW);
        CHECK(r.has_value());
        CHECK(r->position == p.position);
        CHECK(r->rotation == Rotation::Right);
    }
}

TEST(o_piece_rotation_never_moves_cells) {
    const Board board;
    Piece p{PieceType::O, Rotation::Spawn, {4, 10}};
    for (int i = 0; i < 4; ++i) {
        const auto r = game::tryRotate(board, p, CW);
        CHECK(r.has_value());
        CHECK(asSet(game::cellsOf(*r)) == asSet(game::cellsOf(p)));
        p = *r;
    }
}

TEST(every_kick_table_starts_with_zero_offset) {
    for (const PieceType type : core::kAllPieceTypes) {
        for (const auto r : {Rotation::Spawn, Rotation::Right, Rotation::Reverse, Rotation::Left}) {
            for (const auto dir : {CW, CCW}) {
                CHECK(game::kicksFor(type, r, dir)[0] == (Point{0, 0}));
            }
        }
    }
}

TEST(t_kicks_off_left_wall) {
    // T in state R hugging the left wall: its box column 0 is empty, so the
    // box sits at x = -1. Rotating to state 2 needs column -1 -> kick (+1, 0).
    const Board board;
    const Piece p{PieceType::T, Rotation::Right, {-1, 10}};
    CHECK(game::fits(board, p));
    const auto r = game::tryRotate(board, p, CW);
    CHECK(r.has_value());
    CHECK(r->rotation == Rotation::Reverse);
    CHECK(r->position == (Point{0, 10}));
}

TEST(i_kicks_off_right_wall) {
    // Vertical I (state L) in the rightmost column. Rotating clockwise to
    // flat needs 4 columns: tests (0,0) and (+1,0) fail, (-2,0) fits.
    const Board board;
    const Piece p{PieceType::I, Rotation::Left, {8, 10}};
    CHECK(game::fits(board, p));
    const auto r = game::tryRotate(board, p, CW);
    CHECK(r.has_value());
    CHECK(r->rotation == Rotation::Spawn);
    CHECK(r->position == (Point{6, 10}));
    for (const Point c : game::cellsOf(*r)) {
        CHECK(c.x >= 0 && c.x < Board::kWidth);
    }
}

TEST(i_kicks_off_left_wall) {
    // Vertical I (state R, cells in box column 2) in column 0: box x = -2.
    // R -> 2 (clockwise) tests (0,0), (-1,0) fail; (+2,0) fits.
    const Board board;
    const Piece p{PieceType::I, Rotation::Right, {-2, 10}};
    CHECK(game::fits(board, p));
    const auto r = game::tryRotate(board, p, CW);
    CHECK(r.has_value());
    CHECK(r->position == (Point{0, 10}));
}

TEST(t_kicks_up_off_floor) {
    // Flat T resting on the floor. Rotating to R would poke a cell below the
    // floor; SRS test 3 for 0->R is (-1, +1 up), i.e. one left and one up.
    const Board board;
    const int y = Board::kHeight - 2;  // box row 1 on the bottom row
    const Piece p{PieceType::T, Rotation::Spawn, {4, y}};
    CHECK(game::fits(board, p));
    CHECK(!game::fits(board, Piece{PieceType::T, Rotation::Right, {4, y}}));
    const auto r = game::tryRotate(board, p, CW);
    CHECK(r.has_value());
    CHECK(r->rotation == Rotation::Right);
    CHECK(r->position == (Point{3, y - 1}));
}

TEST(rotation_fails_when_every_kick_collides) {
    // Fill everything except the piece's own cells.
    Board board;
    const Piece p{PieceType::T, Rotation::Spawn, {4, 10}};
    for (int y = 0; y < Board::kHeight; ++y) {
        for (int x = 0; x < Board::kWidth; ++x) {
            board.set({x, y}, core::CellType::Z);
        }
    }
    for (const Point c : game::cellsOf(p)) {
        board.set(c, core::CellType::Empty);
    }
    CHECK(!game::canRotate(board, p, CW));
    CHECK(!game::canRotate(board, p, CCW));
}

TEST(rotation_uses_first_fitting_kick_in_table_order) {
    // A floor under a flat T with a two-cell hole: some kicks fit, some
    // don't. The result must be the *first* fitting entry in table order.
    Board board;
    const Piece p{PieceType::T, Rotation::Spawn, {3, 15}};
    for (int x = 0; x < Board::kWidth; ++x) {
        board.set({x, 17}, core::CellType::Z);
    }
    board.set({3, 17}, core::CellType::Empty);
    board.set({4, 17}, core::CellType::Empty);
    const auto r = game::tryRotate(board, p, CCW);
    CHECK(r.has_value());
    CHECK(game::fits(board, *r));
    const auto& kicks = game::kicksFor(PieceType::T, Rotation::Spawn, CCW);
    Piece rotatedInPlace = p;
    rotatedInPlace.rotation = Rotation::Left;
    int firstFit = -1;
    for (std::size_t i = 0; i < kicks.size(); ++i) {
        if (game::fits(board, game::moved(rotatedInPlace, kicks[i].x, kicks[i].y))) {
            firstFit = static_cast<int>(i);
            break;
        }
    }
    CHECK(firstFit >= 0);
    const auto idx = static_cast<std::size_t>(firstFit);
    CHECK(r->position == (Point{p.position.x + kicks[idx].x, p.position.y + kicks[idx].y}));
}

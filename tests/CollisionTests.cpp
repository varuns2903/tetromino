#include "TestFramework.hpp"
#include "game/Collision.hpp"

using namespace tetromino;
using core::CellType;
using core::PieceType;
using core::Rotation;
using game::Board;
using game::Piece;

TEST(piece_fits_on_empty_board_at_spawn) {
    const Board b;
    for (const PieceType type : core::kAllPieceTypes) {
        CHECK(game::fits(b, game::spawnPiece(type, Board::kDefaultWidth)));
    }
}

TEST(spawn_positions_are_centred) {
    const auto minMaxX = [](const Piece& p) {
        int lo = 99;
        int hi = -99;
        for (const auto c : game::cellsOf(p)) {
            lo = std::min(lo, c.x);
            hi = std::max(hi, c.x);
        }
        return std::pair{lo, hi};
    };
    CHECK(minMaxX(game::spawnPiece(PieceType::I, Board::kDefaultWidth)) == (std::pair{3, 6}));
    CHECK(minMaxX(game::spawnPiece(PieceType::O, Board::kDefaultWidth)) == (std::pair{4, 5}));
    CHECK(minMaxX(game::spawnPiece(PieceType::T, Board::kDefaultWidth)) == (std::pair{3, 5}));
}

TEST(spawned_pieces_start_on_first_visible_row) {
    for (const PieceType type : core::kAllPieceTypes) {
        int top = 99;
        for (const auto c : game::cellsOf(game::spawnPiece(type, Board::kDefaultWidth))) {
            top = std::min(top, c.y);
        }
        CHECK_EQ(top, Board::kHiddenRows);
    }
}

TEST(left_wall) {
    const Board b;
    Piece p{PieceType::O, Rotation::Spawn, {0, 10}};
    CHECK(!game::canMove(b, p, -1, 0));
    CHECK(game::canMove(b, p, +1, 0));
    // T in spawn state has its leftmost cell in box column 0.
    Piece t{PieceType::T, Rotation::Spawn, {0, 10}};
    CHECK(!game::canMove(b, t, -1, 0));
    // In state R the box's column 0 is empty, so the box may go to x = -1.
    Piece tr{PieceType::T, Rotation::Right, {0, 10}};
    CHECK(game::canMove(b, tr, -1, 0));
    CHECK(!game::canMove(b, game::moved(tr, -1, 0), -1, 0));
}

TEST(right_wall) {
    const Board b;
    Piece i{PieceType::I, Rotation::Spawn, {Board::kDefaultWidth - 4, 10}};
    CHECK(game::fits(b, i));
    CHECK(!game::canMove(b, i, +1, 0));
    CHECK(game::canMove(b, i, -1, 0));
}

TEST(floor) {
    const Board b;
    Piece o{PieceType::O, Rotation::Spawn, {4, Board::kDefaultHeight - 2}};
    CHECK(game::fits(b, o));
    CHECK(game::isGrounded(b, o));
    CHECK(!game::canMove(b, o, 0, 1));
    CHECK_EQ(game::dropDistance(b, o), 0);
}

TEST(existing_blocks_block_movement) {
    Board b;
    b.set({6, 10}, CellType::Z);
    Piece o{PieceType::O, Rotation::Spawn, {4, 10}};  // occupies x 4-5
    CHECK(!game::canMove(b, o, +1, 0));
    CHECK(game::canMove(b, o, -1, 0));
    b.set({4, 12}, CellType::Z);
    CHECK(!game::canMove(b, o, 0, 1));
}

TEST(overlapping_blocks_do_not_fit) {
    Board b;
    b.set({5, 11}, CellType::S);
    const Piece o{PieceType::O, Rotation::Spawn, {4, 10}};
    CHECK(!game::fits(b, o));
}

TEST(drop_distance_lands_on_stack) {
    Board b;
    for (int x = 0; x < Board::kDefaultWidth; ++x) {
        b.set({x, 20}, CellType::L);
    }
    const Piece o{PieceType::O, Rotation::Spawn, {4, 4}};  // cells rows 4-5
    CHECK_EQ(game::dropDistance(b, o), 14);                // bottom cell: row 5 -> row 19
    const Piece landed = game::droppedPiece(b, o);
    CHECK(game::fits(b, landed));
    CHECK(game::isGrounded(b, landed));
}

TEST(ghost_matches_hard_drop_target) {
    const Board b;
    const Piece i = game::spawnPiece(PieceType::I, Board::kDefaultWidth);
    const Piece ghost = game::droppedPiece(b, i);
    for (const auto c : game::cellsOf(ghost)) {
        CHECK_EQ(c.y, Board::kDefaultHeight - 1);
    }
}

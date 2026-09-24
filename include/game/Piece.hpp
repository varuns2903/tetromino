#pragma once

// Tetromino geometry.
//
// Every piece lives in a square bounding box (4x4 for I, 2x2 for O, 3x3 for
// the rest). A Piece is just (type, rotation state, box position); its four
// cells are looked up in a precomputed table rather than computed by rotating
// coordinates at runtime. Lookup tables make every rotation state exact and
// reproducible, which is what SRS wall kicks and collision tests rely on.
//
//   T, rotation Spawn, position (3, 4):      box origin
//                                            v
//        x: 3 4 5                          (3,4) . # .     cells (4,4)
//           . # .   y=4                          # # #           (3,5) (4,5) (5,5)
//           # # #   y=5                          . . .
//           . . .   y=6

#include <array>

#include "core/Types.hpp"

namespace tetromino::game {

using PieceCells = std::array<core::Point, 4>;

struct Piece {
    core::PieceType type = core::PieceType::T;
    core::Rotation rotation = core::Rotation::Spawn;
    core::Point position{};  // top-left of the bounding box, in board coords

    friend constexpr bool operator==(const Piece&, const Piece&) = default;
};

// Side length of the piece's bounding box.
[[nodiscard]] int boxSize(core::PieceType type);

// Cell offsets inside the bounding box for a given rotation state.
[[nodiscard]] const PieceCells& shapeOf(core::PieceType type, core::Rotation rotation);

// Absolute board coordinates of the piece's four cells.
[[nodiscard]] PieceCells cellsOf(const Piece& piece);

// A new piece of `type` at its spawn position (top-centre of the visible area).
[[nodiscard]] Piece spawnPiece(core::PieceType type);

[[nodiscard]] constexpr Piece moved(Piece piece, int dx, int dy) {
    piece.position.x += dx;
    piece.position.y += dy;
    return piece;
}

}  // namespace tetromino::game

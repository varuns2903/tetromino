#include "game/Piece.hpp"

#include "game/Board.hpp"

namespace tetromino::game {

using core::PieceType;
using core::Point;
using core::Rotation;

namespace {

// Spawn-state shapes (SRS orientation: flat side down, pointing up).
struct ShapeDef {
    int box;
    PieceCells spawn;
};

constexpr std::array<ShapeDef, core::kPieceTypeCount> kShapeDefs{{
    /* I */ {4, {{{0, 1}, {1, 1}, {2, 1}, {3, 1}}}},
    /* O */ {2, {{{0, 0}, {1, 0}, {0, 1}, {1, 1}}}},
    /* T */ {3, {{{1, 0}, {0, 1}, {1, 1}, {2, 1}}}},
    /* S */ {3, {{{1, 0}, {2, 0}, {0, 1}, {1, 1}}}},
    /* Z */ {3, {{{0, 0}, {1, 0}, {1, 1}, {2, 1}}}},
    /* J */ {3, {{{0, 0}, {0, 1}, {1, 1}, {2, 1}}}},
    /* L */ {3, {{{2, 0}, {0, 1}, {1, 1}, {2, 1}}}},
}};

// Rotating a cell 90 degrees clockwise inside an n x n box, with y pointing
// down, maps (x, y) -> (n-1-y, x). Applying that 0..3 times to the spawn
// shape yields the four SRS states exactly - SRS is defined as "true"
// rotation about the box centre, before kicks.
constexpr PieceCells rotateClockwise(const PieceCells& cells, int n) {
    PieceCells out{};
    for (std::size_t i = 0; i < cells.size(); ++i) {
        out[i] = Point{n - 1 - cells[i].y, cells[i].x};
    }
    return out;
}

using ShapeTable = std::array<std::array<PieceCells, core::kRotationCount>, core::kPieceTypeCount>;

constexpr ShapeTable buildShapeTable() {
    ShapeTable table{};
    for (std::size_t t = 0; t < core::kPieceTypeCount; ++t) {
        PieceCells cells = kShapeDefs[t].spawn;
        for (std::size_t r = 0; r < core::kRotationCount; ++r) {
            table[t][r] = cells;
            cells = rotateClockwise(cells, kShapeDefs[t].box);
        }
    }
    return table;
}

// Computed at compile time; zero runtime cost.
constexpr ShapeTable kShapes = buildShapeTable();

}  // namespace

int boxSize(PieceType type) { return kShapeDefs[core::indexOf(type)].box; }

const PieceCells& shapeOf(PieceType type, Rotation rotation) {
    return kShapes[core::indexOf(type)][static_cast<std::size_t>(rotation)];
}

PieceCells cellsOf(const Piece& piece) {
    PieceCells cells = shapeOf(piece.type, piece.rotation);
    for (Point& p : cells) {
        p = p + piece.position;
    }
    return cells;
}

Piece spawnPiece(PieceType type) {
    // Centred horizontally (left of centre for odd widths), with the piece's top row on the first visible row. The I
    // piece's cells sit on row 1 of its box, so its box starts one row higher.
    // Integer division gives I -> columns 3-6, O -> 4-5, others -> 3-5.
    const int x = (Board::kWidth - boxSize(type)) / 2;
    const int y = Board::kHiddenRows - (type == PieceType::I ? 1 : 0);
    return Piece{type, Rotation::Spawn, Point{x, y}};
}

}  // namespace tetromino::game

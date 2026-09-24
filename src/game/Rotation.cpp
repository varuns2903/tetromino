#include "game/Rotation.hpp"

#include "game/Collision.hpp"

namespace tetromino::game {

using core::PieceType;
using core::Point;
using core::Rotation;
using core::RotationDirection;

namespace {

// SRS kick data, transcribed from the widely published reference tables,
// where +y means *up*. Our board has +y pointing down, so every y is negated
// when the table is built (see flipY below) - that keeps these rows
// directly comparable with the published reference.
//
// Index: [from state][direction], direction 0 = clockwise, 1 = counter-cw.

using RawTable = std::array<std::array<KickTable, 2>, core::kRotationCount>;

constexpr RawTable kJlstzKicksYUp{{
    // from 0:   0->R                                     0->L
    {{{{{0, 0}, {-1, 0}, {-1, +1}, {0, -2}, {-1, -2}}}, {{{0, 0}, {+1, 0}, {+1, +1}, {0, -2}, {+1, -2}}}}},
    // from R:   R->2                                     R->0
    {{{{{0, 0}, {+1, 0}, {+1, -1}, {0, +2}, {+1, +2}}}, {{{0, 0}, {+1, 0}, {+1, -1}, {0, +2}, {+1, +2}}}}},
    // from 2:   2->L                                     2->R
    {{{{{0, 0}, {+1, 0}, {+1, +1}, {0, -2}, {+1, -2}}}, {{{0, 0}, {-1, 0}, {-1, +1}, {0, -2}, {-1, -2}}}}},
    // from L:   L->0                                     L->2
    {{{{{0, 0}, {-1, 0}, {-1, -1}, {0, +2}, {-1, +2}}}, {{{0, 0}, {-1, 0}, {-1, -1}, {0, +2}, {-1, +2}}}}},
}};

constexpr RawTable kIKicksYUp{{
    // from 0:   0->R                                     0->L
    {{{{{0, 0}, {-2, 0}, {+1, 0}, {-2, -1}, {+1, +2}}}, {{{0, 0}, {-1, 0}, {+2, 0}, {-1, +2}, {+2, -1}}}}},
    // from R:   R->2                                     R->0
    {{{{{0, 0}, {-1, 0}, {+2, 0}, {-1, +2}, {+2, -1}}}, {{{0, 0}, {+2, 0}, {-1, 0}, {+2, +1}, {-1, -2}}}}},
    // from 2:   2->L                                     2->R
    {{{{{0, 0}, {+2, 0}, {-1, 0}, {+2, +1}, {-1, -2}}}, {{{0, 0}, {+1, 0}, {-2, 0}, {+1, -2}, {-2, +1}}}}},
    // from L:   L->0                                     L->2
    {{{{{0, 0}, {+1, 0}, {-2, 0}, {+1, -2}, {-2, +1}}}, {{{0, 0}, {-2, 0}, {+1, 0}, {-2, -1}, {+1, +2}}}}},
}};

constexpr RawTable flipY(RawTable table) {
    for (auto& byDirection : table) {
        for (auto& kicks : byDirection) {
            for (Point& p : kicks) {
                p.y = -p.y;
            }
        }
    }
    return table;
}

constexpr RawTable kJlstzKicks = flipY(kJlstzKicksYUp);
constexpr RawTable kIKicks = flipY(kIKicksYUp);

// The O piece looks identical in every state; it never moves when rotated.
constexpr KickTable kNoKicks{{{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}}};

}  // namespace

const KickTable& kicksFor(PieceType type, Rotation from, RotationDirection direction) {
    const auto f = static_cast<std::size_t>(from);
    const auto d = direction == RotationDirection::Clockwise ? 0u : 1u;
    switch (type) {
    case PieceType::O: return kNoKicks;
    case PieceType::I: return kIKicks[f][d];
    default: return kJlstzKicks[f][d];
    }
}

std::optional<RotationResult> rotateWithKicks(const Board& board, const Piece& piece, RotationDirection direction) {
    Piece rotatedPiece = piece;
    rotatedPiece.rotation = core::rotated(piece.rotation, direction);

    const KickTable& kicks = kicksFor(piece.type, piece.rotation, direction);
    for (std::size_t i = 0; i < kicks.size(); ++i) {
        const Piece candidate = moved(rotatedPiece, kicks[i].x, kicks[i].y);
        if (fits(board, candidate)) {
            return RotationResult{candidate, static_cast<int>(i)};
        }
    }
    return std::nullopt;
}

std::optional<Piece> tryRotate(const Board& board, const Piece& piece, RotationDirection direction) {
    if (const auto result = rotateWithKicks(board, piece, direction)) {
        return result->piece;
    }
    return std::nullopt;
}

bool canRotate(const Board& board, const Piece& piece, RotationDirection direction) {
    return tryRotate(board, piece, direction).has_value();
}

}  // namespace tetromino::game

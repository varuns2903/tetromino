#include "game/Spin.hpp"

#include <array>

namespace tetromino::game {

using core::Point;
using core::Rotation;

SpinKind detectSpin(const Board& board, const Piece& piece, bool lastActionWasRotation, int lastKick) {
    if (piece.type != core::PieceType::T || !lastActionWasRotation) {
        return SpinKind::None;
    }

    // Corners of the 3x3 box around the T's centre (box cell (1, 1)), in the
    // order top-left, top-right, bottom-right, bottom-left.
    const Point origin = piece.position;
    const std::array<bool, 4> blocked{
        board.isBlocked(origin + Point{0, 0}),
        board.isBlocked(origin + Point{2, 0}),
        board.isBlocked(origin + Point{2, 2}),
        board.isBlocked(origin + Point{0, 2}),
    };
    int count = 0;
    for (const bool b : blocked) {
        count += b ? 1 : 0;
    }
    if (count < 3) {
        return SpinKind::None;
    }

    // The two corners on the side the T points towards.
    bool frontA = false;
    bool frontB = false;
    switch (piece.rotation) {
    case Rotation::Spawn: frontA = blocked[0]; frontB = blocked[1]; break;    // up
    case Rotation::Right: frontA = blocked[1]; frontB = blocked[2]; break;    // right
    case Rotation::Reverse: frontA = blocked[2]; frontB = blocked[3]; break;  // down
    case Rotation::Left: frontA = blocked[3]; frontB = blocked[0]; break;     // left
    }
    constexpr int kLastResortKick = 4;
    return (frontA && frontB) || lastKick == kLastResortKick ? SpinKind::Full : SpinKind::Mini;
}

}  // namespace tetromino::game

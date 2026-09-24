#pragma once

// Rotation with wall kicks, using the SRS kick tables that most modern
// falling-block games share.
//
// A rotation is attempted in up to five positions. First the "natural"
// rotation in place (kick 0). If that collides, the piece is nudged by each
// of four offsets ("wall kicks") in order, and the first position that fits
// wins. If none fits, the rotation fails and the piece stays as it was.
//
// The offsets depend on the piece (I has its own table, O never kicks, the
// other five share one) and on which two rotation states are involved.
// Kicks are what let you rotate a piece flush against a wall, or tuck a T
// into a tight slot.

#include <array>
#include <optional>

#include "core/Types.hpp"
#include "game/Board.hpp"
#include "game/Piece.hpp"

namespace tetromino::game {

inline constexpr std::size_t kKickTests = 5;
using KickTable = std::array<core::Point, kKickTests>;

// Offsets to try (in board coordinates: +x right, +y down) when rotating
// `type` from `from` in `direction`. The first entry is always {0, 0}.
[[nodiscard]] const KickTable& kicksFor(core::PieceType type, core::Rotation from,
                                        core::RotationDirection direction);

// The piece after rotating (with kicks), or nullopt if every test collides.
[[nodiscard]] std::optional<Piece> tryRotate(const Board& board, const Piece& piece,
                                             core::RotationDirection direction);

[[nodiscard]] bool canRotate(const Board& board, const Piece& piece, core::RotationDirection direction);

}  // namespace tetromino::game

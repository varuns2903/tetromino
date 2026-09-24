#pragma once

// Spin detection: was a T piece rotated into a tight spot?
//
// The usual "three corner" rule: look at the four cells diagonally adjacent
// to the T's centre. If the piece's last successful action was a rotation and
// at least three of those corners are blocked (by blocks, walls or the floor),
// it's a spin. It's a full spin if both corners on the side the T points
// towards are blocked (or the rotation needed the last-resort kick, which
// only happens when squeezing into a slot); otherwise it's a mini spin.
//
//     T pointing up:   F . F      F = front corners
//                      # # #      B = back corners
//                      B . B

#include "game/Board.hpp"
#include "game/Piece.hpp"
#include "game/Scoring.hpp"

namespace tetromino::game {

// `lastActionWasRotation`: the piece hasn't moved since its last rotation.
// `lastKick`: the kick test index (0-4) that rotation used.
[[nodiscard]] SpinKind detectSpin(const Board& board, const Piece& piece, bool lastActionWasRotation,
                                  int lastKick);

}  // namespace tetromino::game

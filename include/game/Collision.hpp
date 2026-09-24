#pragma once

// Collision detection: the only code that decides whether a piece may occupy
// a position. Movement, rotation (including every wall-kick candidate),
// gravity, hard drop, the ghost piece and spawning all go through fits().
//
// Mapping piece coordinates onto the board is a translation: each of the
// piece's four cell offsets (from the shape table) is added to the piece's
// box position, giving absolute board cells. The piece fits if none of those
// cells is outside the walls/floor or already filled - Board::isBlocked().

#include "game/Board.hpp"
#include "game/Piece.hpp"

namespace tetromino::game {

[[nodiscard]] bool fits(const Board& board, const Piece& piece);

[[nodiscard]] bool canMove(const Board& board, const Piece& piece, int dx, int dy);

// True if the piece is resting on something (it can't move down).
[[nodiscard]] bool isGrounded(const Board& board, const Piece& piece);

// How many rows the piece can fall before landing.
[[nodiscard]] int dropDistance(const Board& board, const Piece& piece);

// The piece moved as far down as it can go (hard-drop target / ghost).
[[nodiscard]] Piece droppedPiece(const Board& board, const Piece& piece);

}  // namespace tetromino::game

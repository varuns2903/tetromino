#include "game/Collision.hpp"

namespace tetromino::game {

bool fits(const Board& board, const Piece& piece) {
    for (const core::Point cell : cellsOf(piece)) {
        if (board.isBlocked(cell)) {
            return false;
        }
    }
    return true;
}

bool canMove(const Board& board, const Piece& piece, int dx, int dy) {
    return fits(board, moved(piece, dx, dy));
}

bool isGrounded(const Board& board, const Piece& piece) { return !canMove(board, piece, 0, 1); }

int dropDistance(const Board& board, const Piece& piece) {
    int distance = 0;
    while (canMove(board, piece, 0, distance + 1)) {
        ++distance;
    }
    return distance;
}

Piece droppedPiece(const Board& board, const Piece& piece) {
    return moved(piece, 0, dropDistance(board, piece));
}

}  // namespace tetromino::game

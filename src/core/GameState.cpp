#include "core/GameState.hpp"

#include "game/Collision.hpp"

namespace tetromino::core {

std::optional<game::Piece> GameState::ghost() const {
    if (!active || !rules.ghostEnabled) {
        return std::nullopt;
    }
    return game::droppedPiece(board, *active);
}

}  // namespace tetromino::core

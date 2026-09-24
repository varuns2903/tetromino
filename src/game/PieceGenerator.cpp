#include "game/PieceGenerator.hpp"

namespace tetromino::game {

PieceGenerator::PieceGenerator(std::uint64_t seed) : random_(seed) { ensureQueued(kMinLookahead); }

core::PieceType PieceGenerator::next() {
    ensureQueued(kMinLookahead + 1);
    const core::PieceType piece = queue_.front();
    queue_.pop_front();
    return piece;
}

core::PieceType PieceGenerator::peek(std::size_t index) {
    ensureQueued(index + 1);
    return queue_[index];
}

void PieceGenerator::ensureQueued(std::size_t count) {
    while (queue_.size() < count) {
        appendBag();
    }
}

void PieceGenerator::appendBag() {
    auto bag = core::kAllPieceTypes;
    random_.shuffle(bag);
    queue_.insert(queue_.end(), bag.begin(), bag.end());
}

}  // namespace tetromino::game

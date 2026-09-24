#pragma once

// The 7-bag randomizer.
//
// Picking each piece uniformly at random allows long droughts (famously, no I
// piece for 30+ pieces) and floods (four S pieces in a row). Modern falling-block
// games instead deal from a "bag": all seven tetrominoes, shuffled, dealt in order,
// then a fresh shuffled bag. Every piece appears exactly once per 7, so the
// longest possible gap between two I pieces is 12.
//
// The generator keeps a queue at least `lookahead` pieces deep so the NEXT
// preview can show upcoming pieces without consuming them.

#include <cstdint>
#include <deque>

#include "core/Types.hpp"
#include "util/Random.hpp"

namespace tetromino::game {

class PieceGenerator {
public:
    static constexpr std::size_t kMinLookahead = 7;

    explicit PieceGenerator(std::uint64_t seed);

    // Take the next piece from the front of the queue.
    core::PieceType next();

    // Look at the i-th upcoming piece without consuming it (0 = next()).
    [[nodiscard]] core::PieceType peek(std::size_t index);

private:
    void ensureQueued(std::size_t count);
    void appendBag();

    util::Random random_;
    std::deque<core::PieceType> queue_;
};

}  // namespace tetromino::game

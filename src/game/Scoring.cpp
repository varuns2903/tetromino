#include "game/Scoring.hpp"

#include <algorithm>

namespace tetromino::game {

std::uint64_t Scoring::lineClear(int lines, int level) const {
    if (lines <= 0) {
        return 0;
    }
    const auto index = static_cast<std::size_t>(std::min(lines, 4));
    return rules_.lineClearBase[index] * static_cast<std::uint64_t>(std::max(level, 1));
}

std::uint64_t Scoring::softDrop(int cells) const {
    return rules_.softDropPerCell * static_cast<std::uint64_t>(std::max(cells, 0));
}

std::uint64_t Scoring::hardDrop(int cells) const {
    return rules_.hardDropPerCell * static_cast<std::uint64_t>(std::max(cells, 0));
}

int Scoring::levelFor(int totalLines, int startLevel) const {
    const int perLevel = std::max(rules_.linesPerLevel, 1);
    return std::max(startLevel, 1) + std::max(totalLines, 0) / perLevel;
}

}  // namespace tetromino::game

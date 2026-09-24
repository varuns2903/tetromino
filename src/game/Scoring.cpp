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

Award Scoring::award(const Placement& placement, int level, ScoringChain& chain) const {
    const auto lvl = static_cast<std::uint64_t>(std::max(level, 1));
    const int lines = std::clamp(placement.lines, 0, 4);
    const auto idx = static_cast<std::size_t>(lines);

    std::uint64_t base = 0;
    switch (placement.spin) {
    case SpinKind::Full: base = rules_.spinBase[std::min<std::size_t>(idx, 3)]; break;
    case SpinKind::Mini: base = rules_.miniSpinBase[std::min<std::size_t>(idx, 3)]; break;
    case SpinKind::None: base = rules_.lineClearBase[idx]; break;
    }

    Award award;
    if (lines == 0) {
        // A spin without lines still scores, but placing a piece without
        // clearing breaks the combo (and leaves back-to-back untouched).
        chain.combo = -1;
        award.points = base * lvl;
        return award;
    }

    award.difficult = lines == 4 || placement.spin != SpinKind::None;
    award.backToBack = award.difficult && chain.backToBack;
    if (award.backToBack) {
        base = base * rules_.backToBackNumerator / rules_.backToBackDenominator;
    }
    chain.backToBack = award.difficult;

    chain.combo += 1;
    award.combo = chain.combo;

    award.points = base * lvl + rules_.comboPerStep * static_cast<std::uint64_t>(chain.combo) * lvl;
    if (placement.perfectClear) {
        award.points += rules_.perfectClearBase[idx] * lvl;
    }
    return award;
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

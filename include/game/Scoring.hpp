#pragma once

// Scoring and level progression.
//
// Kept separate from Game so the rules can be swapped (e.g. NES scoring,
// or combo / spin bonuses) without touching game flow.

#include <array>
#include <cstdint>

namespace tetromino::game {

struct ScoringRules {
    // Points for clearing 0..4 lines at once, multiplied by the level.
    std::array<std::uint64_t, 5> lineClearBase{0, 100, 300, 500, 800};
    std::uint64_t softDropPerCell = 1;
    std::uint64_t hardDropPerCell = 2;
    int linesPerLevel = 10;
};

class Scoring {
public:
    explicit Scoring(ScoringRules rules = {}) : rules_(rules) {}

    // Points for clearing `lines` rows (0..4) at `level`.
    [[nodiscard]] std::uint64_t lineClear(int lines, int level) const;
    [[nodiscard]] std::uint64_t softDrop(int cells) const;
    [[nodiscard]] std::uint64_t hardDrop(int cells) const;

    // Level reached after `totalLines`, starting at `startLevel`:
    // startLevel + totalLines / linesPerLevel.
    [[nodiscard]] int levelFor(int totalLines, int startLevel) const;

    [[nodiscard]] const ScoringRules& rules() const { return rules_; }

private:
    ScoringRules rules_;
};

}  // namespace tetromino::game

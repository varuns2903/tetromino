#pragma once

// Scoring and level progression.
//
// Kept separate from Game so the rules can be swapped (e.g. a different point
// table) without touching game flow. Everything here is pure arithmetic; the
// only state is the "chain" (combo / back-to-back), which the caller owns and
// passes in.
//
// Scoring, per locked piece:
//   base      lines cleared (0-4), or a spin table if the piece was spun in
//   x 1.5     back-to-back: this clear and the previous clear were both
//             "difficult" (four lines, or a spin that clears lines)
//   + combo   50 x combo x level, where combo counts consecutive pieces that
//             each cleared lines (0 for the first one)
//   + bonus   perfect clear: the board is completely empty afterwards
// All point values are multiplied by the level.

#include <array>
#include <cstdint>

namespace tetromino::game {

// How the piece was put into place, for scoring.
enum class SpinKind : std::uint8_t { None, Mini, Full };

struct ScoringRules {
    // Points for clearing 0..4 lines at once, multiplied by the level.
    std::array<std::uint64_t, 5> lineClearBase{0, 100, 300, 500, 800};
    // Spun into place: 0..3 lines (a T can't clear four).
    std::array<std::uint64_t, 4> spinBase{400, 800, 1200, 1600};
    std::array<std::uint64_t, 4> miniSpinBase{100, 200, 400, 400};
    // Bonus when the clear leaves the board empty, by lines cleared.
    std::array<std::uint64_t, 5> perfectClearBase{0, 800, 1200, 1800, 2000};
    std::uint64_t comboPerStep = 50;
    // Applied to the line/spin points (not combo or perfect-clear bonus), as
    // a fraction to keep everything in integers: 3/2 = x1.5.
    std::uint64_t backToBackNumerator = 3;
    std::uint64_t backToBackDenominator = 2;

    std::uint64_t softDropPerCell = 1;
    std::uint64_t hardDropPerCell = 2;
    int linesPerLevel = 10;
};

// Combo / back-to-back state carried from one locked piece to the next.
struct ScoringChain {
    int combo = -1;            // -1: the previous piece cleared nothing
    bool backToBack = false;   // the last line clear was a difficult one
};

// What a locked piece did, as far as scoring is concerned.
struct Placement {
    int lines = 0;             // rows cleared (0-4)
    SpinKind spin = SpinKind::None;
    bool perfectClear = false; // board empty after the clear
};

// The breakdown of one award, for display ("QUAD  B2B  COMBO x3  +2,400").
struct Award {
    std::uint64_t points = 0;
    int combo = 0;             // 0 = no combo bonus this time
    bool backToBack = false;   // the x1.5 applied
    bool difficult = false;    // counts towards back-to-back
};

class Scoring {
public:
    explicit Scoring(ScoringRules rules = {}) : rules_(rules) {}

    // Points for clearing `lines` rows (0..4) at `level`, with no bonuses.
    [[nodiscard]] std::uint64_t lineClear(int lines, int level) const;

    // Full scoring for one locked piece. Updates `chain` for the next piece.
    [[nodiscard]] Award award(const Placement& placement, int level, ScoringChain& chain) const;

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

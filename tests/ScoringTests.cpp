#include "TestFramework.hpp"
#include "core/GameConfig.hpp"
#include "game/Scoring.hpp"

using namespace tetromino;
using game::Scoring;

TEST(single) { CHECK_EQ(Scoring{}.lineClear(1, 1), 100u); }
TEST(double_) { CHECK_EQ(Scoring{}.lineClear(2, 1), 300u); }
TEST(triple) { CHECK_EQ(Scoring{}.lineClear(3, 1), 500u); }
TEST(quad) { CHECK_EQ(Scoring{}.lineClear(4, 1), 800u); }

TEST(no_lines_no_points) { CHECK_EQ(Scoring{}.lineClear(0, 5), 0u); }

TEST(level_multiplier) {
    const Scoring s;
    CHECK_EQ(s.lineClear(1, 3), 300u);
    CHECK_EQ(s.lineClear(2, 5), 1500u);
    CHECK_EQ(s.lineClear(3, 7), 3500u);
    CHECK_EQ(s.lineClear(4, 10), 8000u);
}

TEST(drop_points) {
    const Scoring s;
    CHECK_EQ(s.softDrop(5), 5u);
    CHECK_EQ(s.hardDrop(18), 36u);
    CHECK_EQ(s.softDrop(-3), 0u);
}

TEST(level_from_lines) {
    const Scoring s;
    CHECK_EQ(s.levelFor(0, 1), 1);
    CHECK_EQ(s.levelFor(9, 1), 1);
    CHECK_EQ(s.levelFor(10, 1), 2);
    CHECK_EQ(s.levelFor(25, 1), 3);
    CHECK_EQ(s.levelFor(10, 5), 6);  // counting from a higher start level
}

TEST(rules_are_configurable) {
    game::ScoringRules rules;
    rules.lineClearBase = {0, 40, 100, 300, 1200};  // NES-style values
    rules.linesPerLevel = 5;
    const Scoring s{rules};
    CHECK_EQ(s.lineClear(4, 2), 2400u);
    CHECK_EQ(s.levelFor(5, 1), 2);
}

TEST(gravity_gets_faster_with_level) {
    using namespace std::chrono_literals;
    CHECK(core::standardGravity(1) == std::chrono::nanoseconds{1s});
    for (int level = 1; level < 20; ++level) {
        CHECK(core::standardGravity(level + 1) <= core::standardGravity(level));
    }
    CHECK(core::standardGravity(30) >= std::chrono::nanoseconds{1ms});
}

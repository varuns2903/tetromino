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

// --- award(): combos, back-to-back, spins, perfect clear ---------------------

using game::Placement;
using game::ScoringChain;
using game::SpinKind;

TEST(award_plain_single) {
    ScoringChain chain;
    const auto a = Scoring{}.award({1, SpinKind::None, false}, 1, chain);
    CHECK_EQ(a.points, 100u);
    CHECK_EQ(a.combo, 0);
    CHECK(!a.backToBack);
    CHECK(!a.difficult);
}

TEST(award_combo_grows_and_resets) {
    const Scoring s;
    ScoringChain chain;
    CHECK_EQ(s.award({1, SpinKind::None, false}, 1, chain).points, 100u);       // combo 0
    CHECK_EQ(s.award({1, SpinKind::None, false}, 1, chain).points, 150u);       // combo 1
    CHECK_EQ(s.award({2, SpinKind::None, false}, 1, chain).points, 400u);       // 300 + 2x50
    CHECK_EQ(s.award({0, SpinKind::None, false}, 1, chain).points, 0u);         // breaks it
    const auto a = s.award({1, SpinKind::None, false}, 1, chain);
    CHECK_EQ(a.combo, 0);
    CHECK_EQ(a.points, 100u);
}

TEST(award_combo_scales_with_level) {
    const Scoring s;
    ScoringChain chain;
    (void)s.award({1, SpinKind::None, false}, 3, chain);
    CHECK_EQ(s.award({1, SpinKind::None, false}, 3, chain).points, 450u);  // (100 + 50) x 3
}

TEST(award_back_to_back_quads) {
    const Scoring s;
    ScoringChain chain;
    CHECK_EQ(s.award({4, SpinKind::None, false}, 1, chain).points, 800u);
    (void)s.award({0, SpinKind::None, false}, 1, chain);  // no clear: combo reset, B2B kept
    const auto a = s.award({4, SpinKind::None, false}, 1, chain);
    CHECK(a.backToBack);
    CHECK_EQ(a.points, 1200u);  // 800 x 1.5
}

TEST(award_easy_clear_breaks_back_to_back) {
    const Scoring s;
    ScoringChain chain;
    (void)s.award({4, SpinKind::None, false}, 1, chain);
    (void)s.award({0, SpinKind::None, false}, 1, chain);
    (void)s.award({1, SpinKind::None, false}, 1, chain);  // single: not difficult
    (void)s.award({0, SpinKind::None, false}, 1, chain);
    const auto a = s.award({4, SpinKind::None, false}, 1, chain);
    CHECK(!a.backToBack);
    CHECK_EQ(a.points, 800u);
}

TEST(award_spins) {
    const Scoring s;
    ScoringChain chain;
    CHECK_EQ(s.award({0, SpinKind::Full, false}, 1, chain).points, 400u);
    CHECK_EQ(s.award({1, SpinKind::Full, false}, 1, chain).points, 800u);
    ScoringChain fresh;
    CHECK_EQ(s.award({2, SpinKind::Full, false}, 2, fresh).points, 2400u);  // 1200 x 2
    ScoringChain mini;
    const auto m = s.award({1, SpinKind::Mini, false}, 1, mini);
    CHECK_EQ(m.points, 200u);
    CHECK(m.difficult);  // spins with lines keep back-to-back going
}

TEST(award_spin_then_quad_is_back_to_back) {
    const Scoring s;
    ScoringChain chain;
    (void)s.award({2, SpinKind::Full, false}, 1, chain);
    (void)s.award({0, SpinKind::None, false}, 1, chain);
    const auto a = s.award({4, SpinKind::None, false}, 1, chain);
    CHECK(a.backToBack);
    CHECK_EQ(a.points, 1200u);
}

TEST(award_perfect_clear_bonus) {
    const Scoring s;
    ScoringChain chain;
    CHECK_EQ(s.award({1, SpinKind::None, true}, 1, chain).points, 900u);  // 100 + 800
    ScoringChain fresh;
    CHECK_EQ(s.award({4, SpinKind::None, true}, 2, fresh).points, 5600u);  // (800 + 2000) x 2
}

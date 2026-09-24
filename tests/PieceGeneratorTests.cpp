#include <algorithm>
#include <array>
#include <vector>

#include "TestFramework.hpp"
#include "game/PieceGenerator.hpp"

using namespace tetromino;
using core::PieceType;
using game::PieceGenerator;

namespace {

std::vector<PieceType> take(PieceGenerator& gen, std::size_t n) {
    std::vector<PieceType> out;
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(gen.next());
    }
    return out;
}

}  // namespace

TEST(every_bag_contains_all_seven_exactly_once) {
    for (std::uint64_t seed = 0; seed < 50; ++seed) {
        PieceGenerator gen{seed};
        for (int bag = 0; bag < 20; ++bag) {
            std::array<int, 7> counts{};
            for (int i = 0; i < 7; ++i) {
                ++counts[core::indexOf(gen.next())];
            }
            for (const int c : counts) {
                CHECK_EQ(c, 1);
            }
        }
    }
}

TEST(same_seed_same_sequence) {
    PieceGenerator a{12345};
    PieceGenerator b{12345};
    CHECK(take(a, 100) == take(b, 100));
}

TEST(different_seeds_differ) {
    PieceGenerator a{1};
    PieceGenerator b{2};
    CHECK(take(a, 28) != take(b, 28));
}

TEST(sequence_is_stable_across_platforms) {
    // Pinned output for seed 42. mt19937_64's output is fixed by the C++
    // standard and util::Random does its own bounded sampling (instead of the
    // implementation-defined std distributions), so this must not change
    // between compilers or standard libraries. If it does, --seed replays
    // are broken.
    using enum PieceType;
    const std::vector<PieceType> expected{O, Z, S, J, I, T, L, T, L, S, O, J, I, Z};
    PieceGenerator gen{42};
    CHECK(take(gen, expected.size()) == expected);
}

TEST(peek_does_not_consume) {
    PieceGenerator gen{7};
    const PieceType p0 = gen.peek(0);
    const PieceType p1 = gen.peek(1);
    const PieceType p10 = gen.peek(10);  // beyond the first bag
    CHECK(gen.peek(0) == p0);
    CHECK(gen.next() == p0);
    CHECK(gen.next() == p1);
    for (int i = 2; i < 10; ++i) {
        gen.next();
    }
    CHECK(gen.next() == p10);
}

TEST(no_long_droughts) {
    // With a 7-bag, the gap between two pieces of the same type is at most 12.
    PieceGenerator gen{2024};
    std::array<int, 7> lastSeen{};
    lastSeen.fill(-1);
    for (int i = 0; i < 7000; ++i) {
        const auto idx = core::indexOf(gen.next());
        if (lastSeen[idx] >= 0) {
            CHECK(i - lastSeen[idx] - 1 <= 12);
        }
        lastSeen[idx] = i;
    }
}

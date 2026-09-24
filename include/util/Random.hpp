#pragma once

// Deterministic random numbers.
//
// std::mt19937_64's output sequence is fully specified by the standard, but
// std::uniform_int_distribution and std::shuffle are not: libstdc++ and libc++
// produce different results from the same seed. For reproducible games
// (--seed) and stable tests we do the bounded sampling ourselves.

#include <cstdint>
#include <random>

namespace tetromino::util {

class Random {
public:
    explicit Random(std::uint64_t seed) : engine_(seed) {}

    // Uniform integer in [0, bound). bound must be > 0.
    std::uint64_t below(std::uint64_t bound) {
        // Rejection sampling: discard the top partial range so every result
        // is equally likely (no modulo bias).
        const std::uint64_t limit = UINT64_MAX - (UINT64_MAX % bound);
        std::uint64_t value = 0;
        do {
            value = engine_();
        } while (value >= limit);
        return value % bound;
    }

    // Fisher-Yates shuffle of any random-access range.
    template <typename Range>
    void shuffle(Range& range) {
        const auto n = static_cast<std::uint64_t>(std::size(range));
        for (std::uint64_t i = n; i > 1; --i) {
            const std::uint64_t j = below(i);
            using std::swap;
            swap(range[static_cast<std::size_t>(i - 1)], range[static_cast<std::size_t>(j)]);
        }
    }

    // A seed from the OS entropy source, for normal (non-reproducible) games.
    static std::uint64_t entropySeed() {
        std::random_device rd;
        return (static_cast<std::uint64_t>(rd()) << 32) ^ static_cast<std::uint64_t>(rd());
    }

private:
    std::mt19937_64 engine_;
};

}  // namespace tetromino::util

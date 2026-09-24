#pragma once

// The choices offered on the start screen: board sizes and difficulty levels.
//
// These are game rules, so they live in the engine; front ends only display
// the names and descriptions. A difficulty is a bundle of rule changes applied
// on top of the base GameConfig when a game starts.

#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <string_view>

namespace tetromino::core {

// What you're playing for.
struct GameType {
    std::string_view name;
    std::string_view summary;  // one line for the start screen
    // Timed modes end when this much playing time has passed.
    std::optional<std::chrono::seconds> timeLimit;
};

inline constexpr std::array<GameType, 2> kGameTypes{{
    {"Endless", "play until the stack reaches the top", std::nullopt},
    {"2-Minute", "score as much as you can in 2:00", std::chrono::seconds{120}},
}};

inline constexpr std::size_t kDefaultGameType = 0;  // Endless

struct BoardSize {
    std::string_view name;
    int width;
    int height;  // visible rows
};

inline constexpr std::array<BoardSize, 4> kBoardSizes{{
    {"Small", 8, 16},
    {"Classic", 10, 20},
    {"Wide", 14, 20},
    {"Tall", 10, 24},
}};

inline constexpr std::size_t kDefaultBoardSize = 1;  // Classic

struct Difficulty {
    std::string_view name;
    std::string_view summary;  // one line for the start screen

    // Multiplies the time a piece takes to fall one row: < 1 is faster.
    double gravityScale;
    // Levels added to the starting level (and so to the gravity curve).
    int levelBonus;

    bool holdEnabled;
    int previewCount;  // how many upcoming pieces are shown (0 = none)
    bool ghostEnabled;
    std::chrono::milliseconds lockDelay;
};

inline constexpr std::array<Difficulty, 4> kDifficulties{{
    {"Easy", "slow · hold · 5 next · ghost", 1.5, 0, true, 5, true, std::chrono::milliseconds{700}},
    {"Normal", "hold · 3 next · ghost", 1.0, 0, true, 3, true, std::chrono::milliseconds{500}},
    {"Hard", "fast · no hold · 1 next", 0.6, 2, false, 1, true, std::chrono::milliseconds{400}},
    {"Expert", "very fast · no hold · no next · no ghost", 0.35, 4, false, 0, false,
     std::chrono::milliseconds{300}},
}};

inline constexpr std::size_t kDefaultDifficulty = 1;  // Normal

// Lookup by name for command-line flags; case and hyphens are ignored.
[[nodiscard]] std::optional<std::size_t> findGameType(std::string_view name);
[[nodiscard]] std::optional<std::size_t> findBoardSize(std::string_view name);
[[nodiscard]] std::optional<std::size_t> findDifficulty(std::string_view name);

}  // namespace tetromino::core

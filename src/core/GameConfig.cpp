#include "core/GameConfig.hpp"

#include <algorithm>
#include <cmath>

#include "core/Presets.hpp"

namespace tetromino::core {

Duration standardGravity(int level) {
    const double l = static_cast<double>(std::max(level, 1) - 1);
    const double base = std::max(0.8 - l * 0.007, 0.0);
    const double seconds = std::pow(base, l);
    // Floor at 1 ms: at that point the piece hits the floor on the next frame
    // anyway, and a zero interval would make the gravity loop meaningless.
    const double clamped = std::max(seconds, 0.001);
    return std::chrono::duration_cast<Duration>(std::chrono::duration<double>(clamped));
}

GameConfig configFor(const GameConfig& base, std::size_t boardSize, std::size_t difficulty, std::size_t gameType) {
    const BoardSize& size = kBoardSizes[boardSize < kBoardSizes.size() ? boardSize : kDefaultBoardSize];
    const Difficulty& d = kDifficulties[difficulty < kDifficulties.size() ? difficulty : kDefaultDifficulty];

    GameConfig config = base;
    config.boardWidth = size.width;
    config.boardHeight = size.height;
    config.startLevel = base.startLevel + d.levelBonus;
    config.gravityScale = base.gravityScale * d.gravityScale;
    config.holdEnabled = d.holdEnabled;
    config.previewCount = d.previewCount;
    config.ghostEnabled = d.ghostEnabled;
    config.lockDelay = d.lockDelay;
    const GameType& type = kGameTypes[gameType < kGameTypes.size() ? gameType : kDefaultGameType];
    if (type.timeLimit) {
        config.timeLimit = std::chrono::duration_cast<Duration>(*type.timeLimit);
    } else {
        config.timeLimit.reset();
    }
    return config;
}

}  // namespace tetromino::core

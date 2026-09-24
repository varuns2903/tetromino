#include "core/GameConfig.hpp"

#include <algorithm>
#include <cmath>

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

}  // namespace tetromino::core

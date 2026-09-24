#pragma once

// Vocabulary types shared by the engine and its front ends.
// Nothing in here knows about terminals.

#include <array>
#include <cstddef>
#include <cstdint>

namespace tetromino::core {

// Board coordinates: x grows to the right, y grows *downwards*.
// Row 0 is the top of the (hidden) spawn area.
struct Point {
    int x = 0;
    int y = 0;

    friend constexpr bool operator==(Point, Point) = default;
    friend constexpr Point operator+(Point a, Point b) { return {a.x + b.x, a.y + b.y}; }
};

enum class PieceType : std::uint8_t { I, O, T, S, Z, J, L };

inline constexpr std::size_t kPieceTypeCount = 7;
inline constexpr std::array<PieceType, kPieceTypeCount> kAllPieceTypes{
    PieceType::I, PieceType::O, PieceType::T, PieceType::S,
    PieceType::Z, PieceType::J, PieceType::L,
};

[[nodiscard]] constexpr std::size_t indexOf(PieceType type) { return static_cast<std::size_t>(type); }

// What occupies one board cell. Locked cells remember which piece they came
// from so the front end can colour them.
enum class CellType : std::uint8_t { Empty, I, O, T, S, Z, J, L };

[[nodiscard]] constexpr CellType toCell(PieceType type) {
    return static_cast<CellType>(static_cast<std::uint8_t>(type) + 1);
}

// SRS rotation states, named after the standard notation 0 / R / 2 / L.
enum class Rotation : std::uint8_t { Spawn = 0, Right = 1, Reverse = 2, Left = 3 };

inline constexpr std::size_t kRotationCount = 4;

enum class RotationDirection : std::uint8_t { Clockwise, CounterClockwise };

[[nodiscard]] constexpr Rotation rotated(Rotation r, RotationDirection dir) {
    const auto step = dir == RotationDirection::Clockwise ? 1u : 3u;
    return static_cast<Rotation>((static_cast<unsigned>(r) + step) % 4u);
}

// Everything the player can ask the game to do. Front ends translate their
// own input (keys, gamepad, network...) into these.
enum class Action : std::uint8_t {
    MoveLeft,
    MoveRight,
    SoftDrop,
    HardDrop,
    RotateClockwise,
    RotateCounterClockwise,
    Hold,
    Pause,    // toggles between Playing and Paused
    Start,    // leave the start screen with the selected setup
    Restart,  // new game with the same setup (from game over or pause)
    OpenMenu, // back to the start screen to change the setup
    Quit,

    // Start-screen setup menu.
    MenuUp,
    MenuDown,
    MenuLeft,
    MenuRight,
};

// The top-level state machine. Exactly one of these is active at a time.
enum class GameMode : std::uint8_t {
    StartScreen,
    Playing,
    Paused,
    GameOver,
    Quit,
};

}  // namespace tetromino::core

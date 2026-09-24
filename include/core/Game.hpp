#pragma once

// The game engine.
//
// Game owns the GameState and is the only thing that changes it. It is
// driven entirely from outside through two calls:
//
//   apply(action)  - the player did something
//   update(dt)     - `dt` of wall-clock time passed
//
// It never reads the clock, sleeps, reads input or draws anything. That makes
// it deterministic (same seed + same calls = same game), trivially testable,
// and independent of the front end: the terminal UI is just one caller.

#include <cstdint>

#include "core/GameConfig.hpp"
#include "core/GameState.hpp"
#include "core/Types.hpp"
#include "game/PieceGenerator.hpp"
#include "game/Scoring.hpp"

namespace tetromino::core {

class Game {
public:
    explicit Game(std::uint64_t seed, GameConfig config = {});

    void apply(Action action);
    void update(Duration dt);

    // Start playing on a pre-filled board instead of an empty one (puzzle
    // setups, tests). Uses the base config's rules as-is (no preset), with
    // the board's own dimensions.
    void startWithBoard(const game::Board& board);

    // Pre-select the start-screen menu entries (indices into kBoardSizes /
    // kDifficulties), e.g. from command-line flags.
    void selectSetup(std::size_t boardSize, std::size_t difficulty);

    // The rules of the game in progress (base config + chosen presets).
    [[nodiscard]] const GameConfig& config() const { return config_; }

    [[nodiscard]] const GameState& state() const { return state_; }
    [[nodiscard]] GameMode mode() const { return state_.mode; }

    // Incremented on every change a front end could draw. Lets the UI skip
    // rendering entirely when nothing happened.
    [[nodiscard]] std::uint64_t revision() const { return revision_; }

    // True if update() does anything right now. When false (menus, pause,
    // game over) the front end can sleep until the next key press.
    [[nodiscard]] bool needsUpdates() const { return state_.mode == GameMode::Playing; }

private:
    void startNewGame();
    void openMenu();
    void applyStartScreen(Action action);
    void applyPlaying(Action action);

    // Piece lifecycle.
    void spawnFromQueue();
    bool spawn(PieceType type);
    void lockActivePiece();
    void finishLineClear();
    void refreshPreview();

    // Player moves. Each returns true if the piece actually moved.
    bool tryShift(int dx);
    bool trySoftDrop();
    void hardDrop();
    bool tryRotate(RotationDirection direction);
    void holdPiece();

    // Bookkeeping after the active piece moved or rotated successfully.
    void onPieceMoved();

    void updateGravity(Duration dt);
    void updateLockDelay(Duration dt);

    void touch() { ++revision_; }

    GameConfig base_;    // as passed to the constructor
    GameConfig config_;  // base_ + the presets chosen for the current game
    game::Scoring scoring_;
    game::PieceGenerator generator_;
    GameState state_;

    Duration gravityTimer_{};
    Duration lockTimer_{};
    Duration clearTimer_{};
    int lockResets_ = 0;
    int lowestRow_ = 0;  // deepest y reached by the current piece

    // For spin detection: did the piece last move by rotating, and with
    // which kick test?
    bool lastActionWasRotation_ = false;
    int lastKick_ = 0;
    game::ScoringChain chain_;
    std::uint64_t revision_ = 0;
};

}  // namespace tetromino::core

#include "core/Game.hpp"

#include <algorithm>
#include <chrono>

#include "game/Collision.hpp"
#include "game/Piece.hpp"
#include "game/Rotation.hpp"

namespace tetromino::core {

using game::Board;
using game::Piece;

Game::Game(std::uint64_t seed, GameConfig config)
    : base_(config), config_(config), scoring_(config.scoring), generator_(seed) {
    state_.board = Board{config_.boardWidth, config_.boardHeight};
    refreshPreview();
}

void Game::selectSetup(std::size_t boardSize, std::size_t difficulty) {
    state_.setup.boardSize = std::min(boardSize, kBoardSizes.size() - 1);
    state_.setup.difficulty = std::min(difficulty, kDifficulties.size() - 1);
    touch();
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void Game::apply(Action action) {
    if (action == Action::Quit) {
        state_.mode = GameMode::Quit;
        touch();
        return;
    }

    switch (state_.mode) {
    case GameMode::StartScreen:
        applyStartScreen(action);
        break;
    case GameMode::Playing:
        applyPlaying(action);
        break;
    case GameMode::Paused:
        if (action == Action::Pause) {
            state_.mode = GameMode::Playing;
            touch();
        } else if (action == Action::Restart) {
            startNewGame();
        } else if (action == Action::OpenMenu) {
            openMenu();
        }
        break;
    case GameMode::GameOver:
        if (action == Action::Restart) {
            startNewGame();
        } else if (action == Action::OpenMenu) {
            openMenu();
        }
        break;
    case GameMode::Quit:
        break;
    }
}

void Game::applyStartScreen(Action action) {
    Setup& setup = state_.setup;
    // Left/right step through the options of the focused row, clamped at
    // both ends; up/down move the focus between rows.
    const auto step = [](std::size_t& index, std::size_t count, int delta) {
        if (delta < 0 && index > 0) {
            --index;
        } else if (delta > 0 && index + 1 < count) {
            ++index;
        }
    };
    switch (action) {
    case Action::MenuUp:
    case Action::MenuDown:
        setup.focus = setup.focus == Setup::Field::BoardSize ? Setup::Field::Difficulty : Setup::Field::BoardSize;
        break;
    case Action::MenuLeft:
    case Action::MenuRight: {
        const int delta = action == Action::MenuLeft ? -1 : +1;
        if (setup.focus == Setup::Field::BoardSize) {
            step(setup.boardSize, kBoardSizes.size(), delta);
        } else {
            step(setup.difficulty, kDifficulties.size(), delta);
        }
        break;
    }
    case Action::Start:
        config_ = configFor(base_, setup.boardSize, setup.difficulty);
        startNewGame();
        return;
    default:
        return;
    }
    touch();
}

void Game::openMenu() {
    state_.mode = GameMode::StartScreen;
    state_.active.reset();
    state_.clearingRows.clear();
    touch();
}

void Game::applyPlaying(Action action) {
    if (action == Action::Pause) {
        state_.mode = GameMode::Paused;
        touch();
        return;
    }
    if (!state_.active) {
        return;  // between pieces
    }
    switch (action) {
    case Action::MoveLeft: tryShift(-1); break;
    case Action::MoveRight: tryShift(+1); break;
    case Action::SoftDrop: trySoftDrop(); break;
    case Action::HardDrop: hardDrop(); break;
    case Action::RotateClockwise: tryRotate(RotationDirection::Clockwise); break;
    case Action::RotateCounterClockwise: tryRotate(RotationDirection::CounterClockwise); break;
    case Action::Hold: holdPiece(); break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------

void Game::update(Duration dt) {
    if (state_.mode != GameMode::Playing) {
        return;
    }
    if (state_.isClearingLines()) {
        clearTimer_ += dt;
        const double progress = static_cast<double>(clearTimer_.count()) /
                                static_cast<double>(std::max(config_.lineClearDelay.count(), Duration::rep{1}));
        state_.clearProgress = static_cast<float>(std::min(progress, 1.0));
        touch();
        if (clearTimer_ >= config_.lineClearDelay) {
            finishLineClear();
        }
        return;
    }
    if (!state_.active) {
        return;
    }
    updateGravity(dt);
    if (state_.active) {
        updateLockDelay(dt);
    }
}

void Game::updateGravity(Duration dt) {
    // Gravity is time-based, not frame-based: we accumulate elapsed time and
    // move one row per full interval. At 30 or 144 FPS the piece falls at the
    // same speed; at high levels several rows can fall in one frame.
    const auto scaled = std::chrono::duration_cast<Duration>(
        std::chrono::duration<double, Duration::period>(static_cast<double>(config_.gravity(state_.stats.level).count()) *
                                                        config_.gravityScale));
    const Duration interval = std::max<Duration>(scaled, std::chrono::milliseconds{1});
    gravityTimer_ += dt;
    while (gravityTimer_ >= interval) {
        if (!game::canMove(state_.board, *state_.active, 0, 1)) {
            gravityTimer_ = Duration::zero();  // resting: don't bank time
            break;
        }
        gravityTimer_ -= interval;
        state_.active = game::moved(*state_.active, 0, 1);
        onPieceMoved();
    }
}

void Game::updateLockDelay(Duration dt) {
    if (!game::isGrounded(state_.board, *state_.active)) {
        lockTimer_ = Duration::zero();
        return;
    }
    lockTimer_ += dt;
    if (lockTimer_ >= config_.lockDelay) {
        lockActivePiece();
    }
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void Game::startNewGame() {
    state_.board = Board{config_.boardWidth, config_.boardHeight};
    state_.rules = Rules{config_.holdEnabled, std::clamp(config_.previewCount, 0, static_cast<int>(kPreviewCount)),
                         config_.ghostEnabled};
    state_.active.reset();
    state_.held.reset();
    state_.holdAvailable = true;
    state_.clearingRows.clear();
    state_.clearProgress = 0.0F;
    state_.stats = Stats{};
    state_.stats.level = scoring_.levelFor(0, config_.startLevel);
    state_.mode = GameMode::Playing;
    spawnFromQueue();
    touch();
}

void Game::startWithBoard(const game::Board& board) {
    config_.boardWidth = board.width();
    config_.boardHeight = board.visibleHeight();
    startNewGame();
    state_.board = board;
    // The first piece was spawned onto an empty board; re-check it.
    if (state_.active && !game::fits(state_.board, *state_.active)) {
        state_.active.reset();
        state_.mode = GameMode::GameOver;
    }
}

void Game::refreshPreview() {
    for (std::size_t i = 0; i < state_.preview.size(); ++i) {
        state_.preview[i] = generator_.peek(i);
    }
}

void Game::spawnFromQueue() {
    const PieceType type = generator_.next();
    refreshPreview();
    state_.holdAvailable = true;  // a fresh piece may be held once
    spawn(type);
}

bool Game::spawn(PieceType type) {
    const Piece piece = game::spawnPiece(type, state_.board.width());
    gravityTimer_ = Duration::zero();
    lockTimer_ = Duration::zero();
    lockResets_ = 0;
    lowestRow_ = piece.position.y;
    touch();

    if (!game::fits(state_.board, piece)) {
        // "Block out": the stack reaches the spawn position.
        state_.active.reset();
        state_.mode = GameMode::GameOver;
        return false;
    }
    state_.active = piece;
    return true;
}

void Game::lockActivePiece() {
    const Piece piece = *state_.active;
    bool anyVisible = false;
    for (const Point cell : game::cellsOf(piece)) {
        state_.board.set(cell, toCell(piece.type));
        anyVisible = anyVisible || cell.y >= Board::kHiddenRows;
    }
    state_.active.reset();
    ++state_.stats.pieces;
    touch();

    if (!anyVisible) {
        // "Lock out": the piece locked entirely above the visible field.
        state_.mode = GameMode::GameOver;
        return;
    }

    // Line clears are resolved only here, after the piece has locked -
    // never while it's still falling.
    std::vector<int> full = state_.board.fullRows();
    if (full.empty()) {
        spawnFromQueue();
        return;
    }

    const int cleared = static_cast<int>(full.size());
    Stats& stats = state_.stats;
    stats.score += scoring_.lineClear(cleared, stats.level);  // scored at the level it happened on
    stats.lines += cleared;
    stats.level = scoring_.levelFor(stats.lines, config_.startLevel);
    if (cleared == 4) {
        ++stats.quads;
    }

    // Keep the rows on the board for a moment so the front end can animate
    // them; update() removes them once the delay is over.
    state_.clearingRows = std::move(full);
    state_.clearProgress = 0.0F;
    clearTimer_ = Duration::zero();
    if (config_.lineClearDelay <= Duration::zero()) {
        finishLineClear();
    }
}

void Game::finishLineClear() {
    state_.board.removeRows(state_.clearingRows);
    state_.clearingRows.clear();
    state_.clearProgress = 0.0F;
    touch();
    spawnFromQueue();
}

// ---------------------------------------------------------------------------
// Moves
// ---------------------------------------------------------------------------

void Game::onPieceMoved() {
    const Piece& piece = *state_.active;
    if (piece.position.y > lowestRow_) {
        // Reaching a new lowest row earns a fresh set of lock resets.
        lowestRow_ = piece.position.y;
        lockResets_ = 0;
        lockTimer_ = Duration::zero();
    } else if (lockTimer_ > Duration::zero() && lockResets_ < config_.maxLockResets) {
        ++lockResets_;
        lockTimer_ = Duration::zero();
    }
    touch();
}

bool Game::tryShift(int dx) {
    if (!game::canMove(state_.board, *state_.active, dx, 0)) {
        return false;
    }
    state_.active = game::moved(*state_.active, dx, 0);
    onPieceMoved();
    return true;
}

bool Game::trySoftDrop() {
    if (!game::canMove(state_.board, *state_.active, 0, 1)) {
        return false;
    }
    state_.active = game::moved(*state_.active, 0, 1);
    state_.stats.score += scoring_.softDrop(1);
    gravityTimer_ = Duration::zero();
    onPieceMoved();
    return true;
}

void Game::hardDrop() {
    const int distance = game::dropDistance(state_.board, *state_.active);
    state_.active = game::moved(*state_.active, 0, distance);
    state_.stats.score += scoring_.hardDrop(distance);
    lockActivePiece();
}

bool Game::tryRotate(RotationDirection direction) {
    const auto result = game::tryRotate(state_.board, *state_.active, direction);
    if (!result) {
        return false;
    }
    state_.active = *result;
    onPieceMoved();
    return true;
}

void Game::holdPiece() {
    // One hold per piece: after holding, the next piece (whether it came
    // from the hold slot or the queue) can't be swapped back until it locks.
    if (!state_.rules.holdEnabled || !state_.holdAvailable) {
        return;
    }
    const PieceType current = state_.active->type;
    if (state_.held) {
        const PieceType swapIn = *state_.held;
        state_.held = current;
        spawn(swapIn);
    } else {
        state_.held = current;
        spawnFromQueue();
    }
    state_.holdAvailable = false;
    touch();
}

}  // namespace tetromino::core

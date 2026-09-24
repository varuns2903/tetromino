#include "core/Game.hpp"

#include <algorithm>
#include <chrono>

#include "game/Collision.hpp"
#include "game/Piece.hpp"
#include "game/Rotation.hpp"
#include "game/Spin.hpp"

namespace tetromino::core {

using game::Board;
using game::Piece;

Game::Game(std::uint64_t seed, GameConfig config)
    : base_(config), config_(config), scoring_(config.scoring), generator_(seed) {
    state_.board = Board{config_.boardWidth, config_.boardHeight};
    refreshPreview();
}

void Game::selectSetup(std::size_t boardSize, std::size_t difficulty, std::size_t gameType) {
    state_.setup.boardSize = std::min(boardSize, kBoardSizes.size() - 1);
    state_.setup.difficulty = std::min(difficulty, kDifficulties.size() - 1);
    state_.setup.gameType = std::min(gameType, kGameTypes.size() - 1);
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
    case GameMode::Countdown:
        if (action == Action::Pause) {  // changed their mind: pause again
            state_.mode = GameMode::Paused;
            touch();
        }
        break;
    case GameMode::Paused:
        if (action == Action::Pause) {
            resume();
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
    case Action::MenuDown: {
        // Wrap around between the rows.
        const int count = Setup::kFieldCount;
        const int delta = action == Action::MenuDown ? 1 : count - 1;
        setup.focus = static_cast<Setup::Field>((static_cast<int>(setup.focus) + delta) % count);
        break;
    }
    case Action::MenuLeft:
    case Action::MenuRight: {
        const int delta = action == Action::MenuLeft ? -1 : +1;
        switch (setup.focus) {
        case Setup::Field::GameType: step(setup.gameType, kGameTypes.size(), delta); break;
        case Setup::Field::BoardSize: step(setup.boardSize, kBoardSizes.size(), delta); break;
        case Setup::Field::Difficulty: step(setup.difficulty, kDifficulties.size(), delta); break;
        }
        break;
    }
    case Action::Start:
        config_ = configFor(base_, setup.boardSize, setup.difficulty, setup.gameType);
        startNewGame();
        return;
    default:
        return;
    }
    touch();
}

void Game::resume() {
    if (config_.resumeCountdown <= Duration::zero()) {
        state_.mode = GameMode::Playing;
    } else {
        state_.mode = GameMode::Countdown;
        state_.countdown = config_.resumeCountdown;
    }
    touch();
}

void Game::endGame(EndReason reason) {
    state_.mode = GameMode::GameOver;
    state_.endReason = reason;
    state_.active.reset();
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
    if (state_.mode == GameMode::Countdown) {
        state_.countdown -= dt;
        if (state_.countdown <= Duration::zero()) {
            state_.countdown = Duration::zero();
            state_.mode = GameMode::Playing;
        }
        touch();
        return;
    }
    if (state_.mode != GameMode::Playing) {
        return;
    }

    // The play clock. Front ends show it to a tenth of a second, so only
    // report a change when that digit changes.
    constexpr auto kTick = std::chrono::milliseconds{100};
    const auto ticksBefore = state_.stats.playTime / kTick;
    state_.stats.playTime += dt;
    if (state_.stats.playTime / kTick != ticksBefore) {
        touch();
    }
    if (state_.rules.timeLimit && state_.stats.playTime >= *state_.rules.timeLimit) {
        state_.stats.playTime = *state_.rules.timeLimit;
        state_.clearingRows.clear();
        endGame(EndReason::TimeUp);
        return;
    }

    // Keep feedback ("QUAD +800") ageing so front ends can fade it out.
    if (state_.feedback.id != 0 && state_.feedback.age < kFeedbackDuration) {
        state_.feedback.age += dt;
        touch();
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
        lastActionWasRotation_ = false;
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
                         config_.ghostEnabled, config_.timeLimit};
    state_.countdown = Duration::zero();
    state_.endReason = EndReason::ToppedOut;
    state_.active.reset();
    state_.held.reset();
    state_.holdAvailable = true;
    state_.clearingRows.clear();
    state_.clearProgress = 0.0F;
    state_.stats = Stats{};
    state_.stats.level = scoring_.levelFor(0, config_.startLevel);
    state_.feedback = Feedback{};
    chain_ = game::ScoringChain{};
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
        endGame(EndReason::ToppedOut);
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
    lastActionWasRotation_ = false;
    lastKick_ = 0;
    touch();

    if (!game::fits(state_.board, piece)) {
        // "Block out": the stack reaches the spawn position.
        endGame(EndReason::ToppedOut);
        return false;
    }
    state_.active = piece;
    return true;
}

void Game::lockActivePiece() {
    const Piece piece = *state_.active;
    // Spins are judged against the board as it was before the piece landed.
    const game::SpinKind spin = game::detectSpin(state_.board, piece, lastActionWasRotation_, lastKick_);
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
        endGame(EndReason::ToppedOut);
        return;
    }

    // Line clears are resolved only here, after the piece has locked -
    // never while it's still falling.
    std::vector<int> full = state_.board.fullRows();
    const int cleared = static_cast<int>(full.size());

    bool perfectClear = false;
    if (cleared > 0) {
        game::Board after = state_.board;
        after.removeRows(full);
        perfectClear = after.isEmpty();
    }

    Stats& stats = state_.stats;
    const int levelBefore = stats.level;
    // Scored at the level the clear happened on.
    const game::Award award = scoring_.award({cleared, spin, perfectClear}, stats.level, chain_);
    stats.score += award.points;
    stats.lines += cleared;
    stats.level = scoring_.levelFor(stats.lines, config_.startLevel);
    switch (cleared) {
    case 1: ++stats.singles; break;
    case 2: ++stats.doubles; break;
    case 3: ++stats.triples; break;
    case 4: ++stats.quads; break;
    default: break;
    }
    stats.spins += spin != game::SpinKind::None ? 1 : 0;
    stats.perfectClears += perfectClear ? 1 : 0;
    stats.maxCombo = std::max(stats.maxCombo, award.combo);

    if (cleared > 0 || spin != game::SpinKind::None) {
        Feedback& f = state_.feedback;
        f = Feedback{f.id + 1, cleared, spin, award.combo, award.backToBack, perfectClear, award.points,
                     stats.level > levelBefore, Duration::zero()};
    }

    if (full.empty()) {
        spawnFromQueue();
        return;
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
    lastActionWasRotation_ = false;
    onPieceMoved();
    return true;
}

bool Game::trySoftDrop() {
    if (!game::canMove(state_.board, *state_.active, 0, 1)) {
        return false;
    }
    state_.active = game::moved(*state_.active, 0, 1);
    lastActionWasRotation_ = false;
    state_.stats.score += scoring_.softDrop(1);
    gravityTimer_ = Duration::zero();
    onPieceMoved();
    return true;
}

void Game::hardDrop() {
    const int distance = game::dropDistance(state_.board, *state_.active);
    state_.active = game::moved(*state_.active, 0, distance);
    if (distance > 0) {
        lastActionWasRotation_ = false;  // it fell after the rotation
    }
    state_.stats.score += scoring_.hardDrop(distance);
    lockActivePiece();
}

bool Game::tryRotate(RotationDirection direction) {
    const auto result = game::rotateWithKicks(state_.board, *state_.active, direction);
    if (!result) {
        return false;
    }
    state_.active = result->piece;
    lastActionWasRotation_ = true;
    lastKick_ = result->kick;
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

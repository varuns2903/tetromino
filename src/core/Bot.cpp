#include "core/Bot.hpp"

#include <cstdlib>
#include <limits>
#include <vector>

namespace tetromino::core {

namespace {

// Weights for the classic stacking heuristic (after Pierre Dellacherie's and
// later genetic-algorithm-tuned players): holes are expensive, height and
// bumpiness moderately so, cleared lines are rewarded.
constexpr double kHeightWeight = -0.51;
constexpr double kLinesWeight = 0.76;
constexpr double kHolesWeight = -0.36 * 2.0;
constexpr double kBumpinessWeight = -0.18;

}  // namespace

double evaluateBoard(const game::Board& board, int linesCleared) {
    std::vector<int> heights(static_cast<std::size_t>(board.width()), 0);
    int holes = 0;
    for (int x = 0; x < board.width(); ++x) {
        bool covered = false;
        for (int y = 0; y < board.height(); ++y) {
            if (board.isOccupied({x, y})) {
                if (!covered) {
                    heights[static_cast<std::size_t>(x)] = board.height() - y;
                }
                covered = true;
            } else if (covered) {
                ++holes;
            }
        }
    }
    int aggregate = 0;
    int bumpiness = 0;
    for (std::size_t x = 0; x < heights.size(); ++x) {
        aggregate += heights[x];
        if (x + 1 < heights.size()) {
            bumpiness += std::abs(heights[x] - heights[x + 1]);
        }
    }
    return kHeightWeight * aggregate + kLinesWeight * linesCleared + kHolesWeight * holes +
           kBumpinessWeight * bumpiness;
}

void Bot::plan(const Game& game) {
    const int width = game.state().board.width();
    double bestScore = -std::numeric_limits<double>::infinity();
    int bestRotations = 0;
    int bestShift = 0;

    for (int rotations = 0; rotations < 4; ++rotations) {
        for (int shift = -width; shift <= width; ++shift) {
            Game trial = game;
            for (int i = 0; i < rotations; ++i) {
                trial.apply(Action::RotateClockwise);
            }
            if (!trial.state().active) {
                continue;
            }
            const int startX = trial.state().active->position.x;
            for (int i = 0; i < std::abs(shift); ++i) {
                trial.apply(shift < 0 ? Action::MoveLeft : Action::MoveRight);
            }
            if (!trial.state().active || trial.state().active->position.x != startX + shift) {
                continue;  // blocked: this column isn't reachable
            }
            const int linesBefore = trial.state().stats.lines;
            trial.apply(Action::HardDrop);
            if (trial.mode() == GameMode::GameOver) {
                continue;  // a move that loses is never the best one
            }
            const double score =
                evaluateBoard(trial.state().board, trial.state().stats.lines - linesBefore);
            if (score > bestScore) {
                bestScore = score;
                bestRotations = rotations;
                bestShift = shift;
            }
        }
    }

    plan_.clear();
    for (int i = 0; i < bestRotations; ++i) {
        plan_.push_back(Action::RotateClockwise);
    }
    for (int i = 0; i < std::abs(bestShift); ++i) {
        plan_.push_back(bestShift < 0 ? Action::MoveLeft : Action::MoveRight);
    }
    plan_.push_back(Action::HardDrop);
}

std::optional<Action> Bot::nextAction(const Game& game) {
    if (game.mode() != GameMode::Playing || !game.state().active) {
        plan_.clear();
        return std::nullopt;
    }
    if (plan_.empty()) {
        plan(game);
    }
    const Action next = plan_.front();
    plan_.pop_front();
    return next;
}

}  // namespace tetromino::core

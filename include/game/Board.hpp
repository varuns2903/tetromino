#pragma once

// The playfield: a grid of cells, nothing more.
//
// The board knows which cells are filled and by what, and how to remove full
// rows. It doesn't know about the falling piece, scoring or drawing.
//
// Coordinates: x in [0, width()), y in [0, height()), y grows downwards.
// The top kHiddenRows rows are the spawn buffer: pieces can be rotated into
// it, but it isn't drawn. Rows kHiddenRows .. height()-1 are the visible
// playfield (visibleHeight() rows, 20 on the classic board).

#include <span>
#include <vector>

#include "core/Types.hpp"

namespace tetromino::game {

class Board {
public:
    static constexpr int kHiddenRows = 4;
    static constexpr int kDefaultWidth = 10;
    static constexpr int kDefaultVisibleHeight = 20;
    static constexpr int kDefaultHeight = kDefaultVisibleHeight + kHiddenRows;

    static constexpr int kMinWidth = 4;
    static constexpr int kMinVisibleHeight = 8;

    // The classic 10 x 20 board.
    Board() : Board(kDefaultWidth, kDefaultVisibleHeight) {}
    // A board `width` columns wide with `visibleHeight` visible rows (plus the
    // hidden buffer). Sizes below the minimums are raised to them.
    Board(int width, int visibleHeight);

    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }
    [[nodiscard]] int visibleHeight() const { return height_ - kHiddenRows; }

    [[nodiscard]] bool inBounds(core::Point p) const {
        return p.x >= 0 && p.x < width_ && p.y >= 0 && p.y < height_;
    }

    // Cell contents. Out-of-bounds reads return Empty.
    [[nodiscard]] core::CellType at(core::Point p) const;
    // True if `p` is inside the board and filled.
    [[nodiscard]] bool isOccupied(core::Point p) const;
    // True if a piece cell may *not* be placed at `p`: outside the walls or
    // floor, or already filled. This is the single question collision
    // detection asks.
    [[nodiscard]] bool isBlocked(core::Point p) const;

    // Out-of-bounds writes are ignored.
    void set(core::Point p, core::CellType type);
    void clear();

    [[nodiscard]] bool isRowFull(int y) const;
    [[nodiscard]] bool isRowEmpty(int y) const;
    [[nodiscard]] bool isEmpty() const;

    // Indices of all full rows, top to bottom.
    [[nodiscard]] std::vector<int> fullRows() const;

    // Remove the given rows and shift everything above them down.
    // `rows` must be sorted ascending (as returned by fullRows()).
    void removeRows(std::span<const int> rows);

    // Convenience: remove all full rows and return how many there were.
    int clearFullRows();

private:
    [[nodiscard]] std::size_t indexOf(core::Point p) const {
        return static_cast<std::size_t>(p.y * width_ + p.x);
    }

    int width_;
    int height_;
    std::vector<core::CellType> cells_;
};

}  // namespace tetromino::game

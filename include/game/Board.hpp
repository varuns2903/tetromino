#pragma once

// The playfield: a grid of cells, nothing more.
//
// The board knows which cells are filled and by what, and how to remove full
// rows. It doesn't know about the falling piece, scoring or drawing.
//
// Coordinates: x in [0, kWidth), y in [0, kHeight), y grows downwards.
// The top kHiddenRows rows are the spawn buffer: pieces appear there and can
// be rotated into it, but it isn't drawn. Rows kHiddenRows .. kHeight-1 are
// the 20 visible rows.

#include <array>
#include <span>
#include <vector>

#include "core/Types.hpp"

namespace tetromino::game {

class Board {
public:
    static constexpr int kWidth = 10;
    static constexpr int kVisibleHeight = 20;
    static constexpr int kHiddenRows = 4;
    static constexpr int kHeight = kVisibleHeight + kHiddenRows;

    Board();

    [[nodiscard]] static constexpr bool inBounds(core::Point p) {
        return p.x >= 0 && p.x < kWidth && p.y >= 0 && p.y < kHeight;
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
    [[nodiscard]] static constexpr std::size_t indexOf(core::Point p) {
        return static_cast<std::size_t>(p.y * kWidth + p.x);
    }

    std::array<core::CellType, static_cast<std::size_t>(kWidth * kHeight)> cells_{};
};

}  // namespace tetromino::game

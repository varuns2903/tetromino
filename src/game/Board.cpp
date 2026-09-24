#include "game/Board.hpp"

#include <algorithm>

namespace tetromino::game {

using core::CellType;
using core::Point;

Board::Board() { clear(); }

CellType Board::at(Point p) const { return inBounds(p) ? cells_[indexOf(p)] : CellType::Empty; }

bool Board::isOccupied(Point p) const { return inBounds(p) && cells_[indexOf(p)] != CellType::Empty; }

bool Board::isBlocked(Point p) const { return !inBounds(p) || cells_[indexOf(p)] != CellType::Empty; }

void Board::set(Point p, CellType type) {
    if (inBounds(p)) {
        cells_[indexOf(p)] = type;
    }
}

void Board::clear() { cells_.fill(CellType::Empty); }

bool Board::isRowFull(int y) const {
    for (int x = 0; x < kWidth; ++x) {
        if (!isOccupied({x, y})) {
            return false;
        }
    }
    return y >= 0 && y < kHeight;
}

bool Board::isRowEmpty(int y) const {
    for (int x = 0; x < kWidth; ++x) {
        if (isOccupied({x, y})) {
            return false;
        }
    }
    return true;
}

bool Board::isEmpty() const {
    return std::all_of(cells_.begin(), cells_.end(), [](CellType c) { return c == CellType::Empty; });
}

std::vector<int> Board::fullRows() const {
    std::vector<int> rows;
    for (int y = 0; y < kHeight; ++y) {
        if (isRowFull(y)) {
            rows.push_back(y);
        }
    }
    return rows;
}

void Board::removeRows(std::span<const int> rows) {
    if (rows.empty()) {
        return;
    }
    // Walk from the bottom up, copying every surviving row to the next free
    // destination row. One pass, no matter how many rows are removed or
    // whether they're contiguous.
    int dst = kHeight - 1;
    for (int src = kHeight - 1; src >= 0; --src) {
        if (std::find(rows.begin(), rows.end(), src) != rows.end()) {
            continue;
        }
        if (dst != src) {
            for (int x = 0; x < kWidth; ++x) {
                cells_[indexOf({x, dst})] = cells_[indexOf({x, src})];
            }
        }
        --dst;
    }
    // Whatever is left at the top is new, empty space.
    for (; dst >= 0; --dst) {
        for (int x = 0; x < kWidth; ++x) {
            cells_[indexOf({x, dst})] = CellType::Empty;
        }
    }
}

int Board::clearFullRows() {
    const std::vector<int> rows = fullRows();
    removeRows(rows);
    return static_cast<int>(rows.size());
}

}  // namespace tetromino::game

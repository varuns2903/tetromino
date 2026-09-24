#include <vector>

#include "TestFramework.hpp"
#include "game/Board.hpp"

using namespace tetromino;
using core::CellType;
using game::Board;

namespace {

void fillRow(Board& b, int y, CellType type = CellType::I) {
    for (int x = 0; x < Board::kWidth; ++x) {
        b.set({x, y}, type);
    }
}

}  // namespace

TEST(new_board_is_empty) {
    const Board b;
    CHECK(b.isEmpty());
    CHECK(b.fullRows().empty());
    for (int y = 0; y < Board::kHeight; ++y) {
        CHECK(b.isRowEmpty(y));
    }
}

TEST(standard_dimensions) {
    CHECK_EQ(Board::kWidth, 10);
    CHECK_EQ(Board::kVisibleHeight, 20);
    CHECK_EQ(Board::kHeight, Board::kVisibleHeight + Board::kHiddenRows);
}

TEST(set_and_read_cells) {
    Board b;
    b.set({3, 7}, CellType::T);
    CHECK(b.at({3, 7}) == CellType::T);
    CHECK(b.isOccupied({3, 7}));
    CHECK(!b.isOccupied({4, 7}));
    CHECK(!b.isEmpty());
    b.set({3, 7}, CellType::Empty);
    CHECK(b.isEmpty());
}

TEST(out_of_bounds_reads_are_empty_and_writes_ignored) {
    Board b;
    b.set({-1, 0}, CellType::Z);
    b.set({Board::kWidth, 0}, CellType::Z);
    b.set({0, Board::kHeight}, CellType::Z);
    CHECK(b.isEmpty());
    CHECK(b.at({-1, 5}) == CellType::Empty);
    CHECK(!b.isOccupied({100, 100}));
}

TEST(walls_and_floor_are_blocked) {
    const Board b;
    CHECK(b.isBlocked({-1, 5}));
    CHECK(b.isBlocked({Board::kWidth, 5}));
    CHECK(b.isBlocked({5, Board::kHeight}));
    CHECK(b.isBlocked({5, -1}));
    CHECK(!b.isBlocked({0, 0}));
    CHECK(!b.isBlocked({Board::kWidth - 1, Board::kHeight - 1}));
}

TEST(detects_full_rows) {
    Board b;
    fillRow(b, 23);
    fillRow(b, 20);
    b.set({0, 21}, CellType::O);  // partial row
    CHECK(b.isRowFull(23));
    CHECK(b.isRowFull(20));
    CHECK(!b.isRowFull(21));
    CHECK(b.fullRows() == (std::vector<int>{20, 23}));
}

TEST(clearing_one_row_shifts_rows_above_down) {
    Board b;
    fillRow(b, 23);
    b.set({2, 22}, CellType::S);
    b.set({7, 10}, CellType::L);
    CHECK_EQ(b.clearFullRows(), 1);
    CHECK(b.at({2, 23}) == CellType::S);
    CHECK(b.at({7, 11}) == CellType::L);
    CHECK(b.isRowEmpty(22));
    CHECK(b.isRowEmpty(0));
}

TEST(clearing_non_adjacent_rows) {
    Board b;
    fillRow(b, 23);
    b.set({1, 22}, CellType::J);  // between the cleared rows: falls 1
    fillRow(b, 21);
    b.set({4, 20}, CellType::T);  // falls 2
    CHECK_EQ(b.clearFullRows(), 2);
    CHECK(b.at({1, 23}) == CellType::J);
    CHECK(b.at({4, 22}) == CellType::T);
    CHECK(b.isRowEmpty(21));
    CHECK(b.isRowEmpty(20));
}

TEST(clearing_four_rows) {
    Board b;
    for (int y = 20; y < 24; ++y) {
        fillRow(b, y);
    }
    b.set({9, 19}, CellType::I);
    CHECK_EQ(b.clearFullRows(), 4);
    CHECK(b.at({9, 23}) == CellType::I);
    CHECK(b.isRowEmpty(19));
}

TEST(clearing_preserves_cell_types) {
    Board b;
    fillRow(b, 23);
    const CellType types[] = {CellType::I, CellType::O, CellType::T, CellType::S, CellType::Z,
                              CellType::J, CellType::L, CellType::I, CellType::O, CellType::T};
    for (int x = 0; x < Board::kWidth; ++x) {
        b.set({x, 22}, types[x]);
    }
    b.set({0, 22}, CellType::Empty);  // keep row 22 incomplete
    b.clearFullRows();
    for (int x = 1; x < Board::kWidth; ++x) {
        CHECK(b.at({x, 23}) == types[x]);
    }
}

TEST(clear_resets_board) {
    Board b;
    fillRow(b, 5);
    b.clear();
    CHECK(b.isEmpty());
}

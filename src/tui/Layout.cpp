#include "tui/Layout.hpp"

#include <algorithm>

#include "core/GameState.hpp"
#include "game/Board.hpp"

namespace tetromino::tui {

namespace {

constexpr int kSideWidth = 17;  // side panels, including borders
constexpr int kGap = 1;         // blank column between panels
constexpr int kHoldHeight = 6;
constexpr int kStatsHeight = 8;
constexpr int kKeysMinHeight = 8;
constexpr int kKeysMaxHeight = 10;  // 8 bindings + border
constexpr int kPreviewRows = 3;  // rows per preview slot (2 for the piece + 1 gap)

struct Scale {
    int cellWidth;
    int cellHeight;
};

constexpr Scale kScales[] = {{4, 2}, {2, 1}};  // try the big one first

TerminalSize requiredSize(Scale s) {
    const int boardW = game::Board::kWidth * s.cellWidth + 2;
    const int boardH = game::Board::kVisibleHeight * s.cellHeight + 2;
    const int minSideH = kHoldHeight + kStatsHeight;
    return {kSideWidth + kGap + boardW + kGap + kSideWidth, std::max(boardH, minSideH)};
}

}  // namespace

Layout computeLayout(TerminalSize terminal) {
    Layout layout;
    layout.terminal = terminal;
    layout.minimum = requiredSize(kScales[1]);

    const Scale* chosen = nullptr;
    for (const Scale& s : kScales) {
        const TerminalSize need = requiredSize(s);
        if (terminal.columns >= need.columns && terminal.rows >= need.rows) {
            chosen = &s;
            break;
        }
    }
    if (chosen == nullptr) {
        layout.fits = false;
        return layout;
    }
    layout.fits = true;
    layout.cellWidth = chosen->cellWidth;
    layout.cellHeight = chosen->cellHeight;

    const TerminalSize need = requiredSize(*chosen);
    const int x0 = (terminal.columns - need.columns) / 2;
    const int y0 = (terminal.rows - need.rows) / 2;
    layout.content = {x0, y0, need.columns, need.rows};

    const int boardW = game::Board::kWidth * chosen->cellWidth + 2;
    const int boardH = game::Board::kVisibleHeight * chosen->cellHeight + 2;

    // Left column: HOLD, SCORE, KEYS (if room).
    const Rect left{x0, y0, kSideWidth, need.rows};
    layout.hold = {left.x, left.y, kSideWidth, kHoldHeight};
    layout.stats = {left.x, layout.hold.bottom(), kSideWidth, kStatsHeight};
    const int keysH = std::min(left.bottom() - layout.stats.bottom(), kKeysMaxHeight);
    if (keysH >= kKeysMinHeight) {
        layout.keys = {left.x, layout.stats.bottom(), kSideWidth, keysH};
    }

    // Centre: the board.
    layout.board = {left.right() + kGap, y0, boardW, boardH};
    layout.well = layout.board.inset(1);

    // Right column: NEXT queue sized to what fits, message area below it.
    const int rightX = layout.board.right() + kGap;
    const int maxNextH = need.rows - 4;  // leave a few rows for the message
    const int previews = std::clamp((maxNextH - 3) / kPreviewRows, 1, static_cast<int>(core::kPreviewCount));
    layout.previewCount = previews;
    layout.next = {rightX, y0, kSideWidth, 3 + previews * kPreviewRows};
    layout.message = {rightX, layout.next.bottom(), kSideWidth, need.rows - layout.next.h};

    return layout;
}

}  // namespace tetromino::tui

#include "tui/Layout.hpp"

#include <algorithm>

#include "core/GameState.hpp"

namespace tetromino::tui {

namespace {

constexpr int kGap = 1;  // blank column between panels
constexpr int kMinSideInner = 15;
constexpr int kStatsHeight = 8;
constexpr int kKeysMinHeight = 8;
constexpr int kKeysMaxHeight = 10;  // 8 bindings + border

constexpr int kMinCellPixels = 2;
constexpr int kMaxCellPixels = 8;     // 4x: plenty even on huge screens
constexpr int kMaxPreviewPixels = 4;  // side-panel pieces stop growing at 2x

// Pixel counts to terminal rows (two pixels per row, rounded up).
constexpr int rowsFor(int pixels) { return (pixels + 1) / 2; }

struct Sizes {
    int sideW;
    int holdH;
    int nextH;
    int boardW;
    int boardH;
    TerminalSize need;
};

// Geometry for a given cell size and preview-piece size.
Sizes sizesFor(BoardShape board, int cell, int preview, int previewSlots) {
    Sizes s{};
    // Preview pieces are at most 4 cells wide and 2 tall; leave one column
    // of padding on each side and one row above and below.
    s.sideW = std::max(kMinSideInner, 4 * preview + 2) + 2;
    const int pieceRows = rowsFor(2 * preview);
    s.holdH = pieceRows + 4;
    s.nextH = 3 + std::max(previewSlots, 1) * (pieceRows + 1);
    s.boardW = board.columns * cell + 2;
    s.boardH = rowsFor(board.rows * cell) + 2;
    const int leftH = s.holdH + kStatsHeight;
    const int rightH = s.nextH + 2;  // + a little room for the line-clear callout
    s.need = {s.sideW + kGap + s.boardW + kGap + s.sideW, std::max({s.boardH, leftH, rightH})};
    return s;
}

bool fitsIn(TerminalSize need, TerminalSize terminal) {
    return terminal.columns >= need.columns && terminal.rows >= need.rows;
}

}  // namespace

Layout computeLayout(TerminalSize terminal, BoardShape board, int previews, bool halfBlocks) {
    Layout layout;
    layout.terminal = terminal;
    layout.halfBlocks = halfBlocks;
    const int wanted = std::clamp(previews, 0, static_cast<int>(core::kPreviewCount));
    layout.minimum = sizesFor(board, kMinCellPixels, kMinCellPixels, wanted).need;

    // Largest board first. Character rendering can only do whole multiples
    // (even pixel counts); half blocks can do every size.
    const int step = halfBlocks ? 1 : 2;
    int cell = 0;
    int preview = kMinCellPixels;
    for (int c = kMaxCellPixels; c >= kMinCellPixels; c -= step) {
        // Side-panel pieces follow the board size, but shrink first if that's
        // what it takes to show every preview the difficulty allows.
        for (int p = std::min(c, kMaxPreviewPixels); p >= kMinCellPixels; p -= step) {
            if (fitsIn(sizesFor(board, c, p, wanted).need, terminal)) {
                cell = c;
                preview = p;
                break;
            }
        }
        if (cell != 0) {
            break;
        }
    }
    if (cell == 0) {
        layout.fits = false;
        return layout;
    }

    const Sizes s = sizesFor(board, cell, preview, wanted);
    layout.fits = true;
    layout.cellPixels = cell;
    layout.previewPixels = preview;
    layout.previewCount = wanted;

    const int x0 = (terminal.columns - s.need.columns) / 2;
    const int y0 = (terminal.rows - s.need.rows) / 2;
    layout.content = {x0, y0, s.need.columns, s.need.rows};

    // Left column: HOLD, SCORE, KEYS (if room).
    layout.hold = {x0, y0, s.sideW, s.holdH};
    layout.stats = {x0, layout.hold.bottom(), s.sideW, kStatsHeight};
    const int keysH = std::min(y0 + s.need.rows - layout.stats.bottom(), kKeysMaxHeight);
    if (keysH >= kKeysMinHeight) {
        layout.keys = {x0, layout.stats.bottom(), s.sideW, keysH};
    }

    // Centre: the board.
    layout.board = {x0 + s.sideW + kGap, y0, s.boardW, s.boardH};
    layout.well = layout.board.inset(1);

    // Right column: NEXT queue, message area below it.
    const int rightX = layout.board.right() + kGap;
    layout.next = {rightX, y0, s.sideW, s.nextH};
    layout.message = {rightX, layout.next.bottom(), s.sideW, s.need.rows - s.nextH};

    return layout;
}

}  // namespace tetromino::tui

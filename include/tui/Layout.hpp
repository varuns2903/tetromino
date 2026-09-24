#pragma once

// Screen layout: where each panel goes for a given terminal size.
//
// Pure geometry, recomputed on every resize. The layout is centred in the
// terminal and uses the largest board scale that fits.
//
// Sizes are measured in "pixels": one pixel is one terminal column wide and
// half a terminal row tall, which is roughly square (see PixelCanvas). A board
// cell is `cellPixels` x `cellPixels` pixels: 2 is the smallest (a cell is
// 2 columns x 1 row), 3 is 1.5x, 4 is 2x, and so on. Colour terminals can
// use every size; monochrome ones draw with characters and only even sizes.
//
//   ╭─ HOLD ───────╮ ╭─── TETROMINO ──────╮ ╭─ NEXT ───────╮
//   │              │ │                    │ │              │
//   ╰──────────────╯ │                    │ │              │
//   ╭─ SCORE ──────╮ │       board        │ │   previews   │
//   │              │ │                    │ │              │
//   ╰──────────────╯ │                    │ ╰──────────────╯
//   ╭─ KEYS ───────╮ │                    │
//   │              │ │                    │
//   ╰──────────────╯ ╰────────────────────╯

#include "tui/Terminal.hpp"

namespace tetromino::tui {

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    [[nodiscard]] constexpr int right() const { return x + w; }
    [[nodiscard]] constexpr int bottom() const { return y + h; }
    [[nodiscard]] constexpr Rect inset(int d) const { return {x + d, y + d, w - 2 * d, h - 2 * d}; }
};

struct Layout {
    bool fits = false;
    TerminalSize terminal{};
    TerminalSize minimum{};

    // Size of one board cell / one HOLD-NEXT preview cell, in pixels.
    int cellPixels = 2;
    int previewPixels = 2;
    // Pixel rendering (half blocks) vs character rendering.
    bool halfBlocks = true;

    Rect board;  // including its border
    Rect well;   // the playfield inside the border
    Rect hold;
    Rect stats;
    Rect keys;   // empty (w == 0) if there's no room
    Rect next;
    Rect message;  // free space under NEXT, for "QUAD!" etc.
    int previewCount = 0;  // preview slots that fit (0 if none requested)

    // The whole game area, for centring start-screen content.
    Rect content;
};

struct BoardShape {
    int columns = 10;
    int rows = 20;  // visible rows

    friend constexpr bool operator==(BoardShape, BoardShape) = default;
};

// Layout for a board of `board` cells showing up to `previews` upcoming
// pieces (0 = the NEXT panel shows that previews are off). `halfBlocks`
// allows the in-between (odd) cell sizes.
[[nodiscard]] Layout computeLayout(TerminalSize terminal, BoardShape board = {}, int previews = 5,
                                   bool halfBlocks = true);

}  // namespace tetromino::tui

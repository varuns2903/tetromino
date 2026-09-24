#pragma once

// Screen layout: where each panel goes for a given terminal size.
//
// Pure geometry, recomputed on every resize. The layout is centred in the
// terminal and scales the board up (4x2 characters per cell instead of 2x1)
// when the window is large enough.
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

    // Terminal columns / rows used for one board cell.
    int cellWidth = 2;
    int cellHeight = 1;

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
// pieces (0 = the NEXT panel shows that previews are off).
[[nodiscard]] Layout computeLayout(TerminalSize terminal, BoardShape board = {}, int previews = 5);

}  // namespace tetromino::tui

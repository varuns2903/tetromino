#pragma once

// A tiny bitmap drawn with half-block characters.
//
// A terminal cell is roughly twice as tall as it is wide. The glyph '▀'
// (upper half block) paints the top half of a cell in the foreground colour
// and, because the rest of the cell shows the background colour, the bottom
// half in the background colour. So one terminal cell can show *two*
// independently coloured square-ish "pixels" stacked vertically:
//
//     pixel (x, 2r)     -> foreground of '▀' at column x, row r
//     pixel (x, 2r + 1) -> background of '▀' at column x, row r
//
// That doubles the vertical resolution and lets the board be scaled in half
// steps (1x, 1.5x, 2x, ...) instead of only whole multiples. Colour
// terminals only: it relies on setting both colours of a cell.

#include <vector>

#include "tui/Color.hpp"

namespace tetromino::tui {

class ScreenBuffer;

class PixelCanvas {
public:
    // `width` pixels (= terminal columns) by `height` pixels (= half rows).
    // Reuses the existing allocation. All pixels become transparent.
    void reset(int width, int height);

    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }

    // Color::terminalDefault() means "transparent". Out-of-range is ignored.
    void set(int x, int y, Color color);
    void fill(int x, int y, int w, int h, Color color);

    // Draw onto `buf` with the top-left pixel at terminal (column, row).
    // Transparent pixels show `background` (which may itself be the
    // terminal default, in which case the terminal's own background shows).
    void blit(ScreenBuffer& buf, int column, int row, Color background) const;

    // Transparent (terminal default) outside the canvas.
    [[nodiscard]] Color at(int x, int y) const;

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<Color> pixels_;
};

}  // namespace tetromino::tui

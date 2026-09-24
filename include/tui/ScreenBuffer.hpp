#pragma once

// An in-memory grid of terminal cells, and the diff that turns one grid into
// another on the real terminal.
//
// Why not print straight to stdout each frame? Clearing the screen and
// redrawing everything means the terminal shows a blank (or half-drawn)
// screen for a moment every frame - that's flicker - and it sends thousands
// of bytes per frame when typically only a handful of cells changed (a piece
// moved one column: ~8 cells).
//
// Instead the renderer draws into a *back* buffer. The *front* buffer holds
// what we believe is currently on the terminal. encodeDiff() walks both,
// and for each cell that differs emits just enough to change it: a cursor
// move (skipped when the cursor is already there), an SGR style change
// (skipped when the style is unchanged) and the glyph. Then front = back.
// This is the same idea as React's virtual DOM or curses' "refresh".

#include <string>
#include <string_view>
#include <vector>

#include "tui/Color.hpp"

namespace tetromino::tui {

struct Cell {
    char32_t ch = U' ';
    Style style{};

    friend constexpr bool operator==(const Cell&, const Cell&) = default;
};

class ScreenBuffer {
public:
    ScreenBuffer() = default;
    ScreenBuffer(int width, int height);

    // Resize and clear. Reuses the existing allocation when shrinking.
    void resize(int width, int height);
    void clear(const Style& style = {});

    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }

    [[nodiscard]] bool contains(int x, int y) const {
        return x >= 0 && y >= 0 && x < width_ && y < height_;
    }

    // All drawing is clipped to the buffer: out-of-range writes are ignored,
    // so callers never need bounds checks.
    void set(int x, int y, char32_t ch, const Style& style);
    void fill(int x, int y, int w, int h, char32_t ch, const Style& style);

    // Write UTF-8 text starting at (x, y). Returns the column after the text.
    int text(int x, int y, std::string_view utf8, const Style& style);

    // Change only the style of existing cells (e.g. dim a region).
    void restyle(int x, int y, int w, int h, const Style& style);

    [[nodiscard]] const Cell& at(int x, int y) const;

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<Cell> cells_;
};

// Append to `out` the bytes that transform what's on screen (`front`) into
// `back`. If the sizes differ, or `full` is set, every cell is repainted.
void encodeDiff(const ScreenBuffer& front, const ScreenBuffer& back, ColorMode mode, bool full,
                std::string& out);

// Decode the next UTF-8 code point from `s` starting at `i`, advancing `i`.
// Invalid bytes decode as U+FFFD.
[[nodiscard]] char32_t nextCodepoint(std::string_view s, std::size_t& i);

// Number of code points in a UTF-8 string (all our glyphs are single-width,
// so this is also its width in columns).
[[nodiscard]] int displayWidth(std::string_view utf8);

}  // namespace tetromino::tui

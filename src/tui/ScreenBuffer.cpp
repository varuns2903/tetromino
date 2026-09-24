#include "tui/ScreenBuffer.hpp"

#include <algorithm>

#include "tui/Ansi.hpp"

namespace tetromino::tui {

namespace {

const Cell kOutOfBounds{};

}  // namespace

ScreenBuffer::ScreenBuffer(int width, int height) { resize(width, height); }

void ScreenBuffer::resize(int width, int height) {
    width_ = std::max(0, width);
    height_ = std::max(0, height);
    cells_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), Cell{});
}

void ScreenBuffer::clear(const Style& style) { std::fill(cells_.begin(), cells_.end(), Cell{U' ', style}); }

void ScreenBuffer::set(int x, int y, char32_t ch, const Style& style) {
    if (contains(x, y)) {
        cells_[static_cast<std::size_t>(y * width_ + x)] = Cell{ch, style};
    }
}

void ScreenBuffer::fill(int x, int y, int w, int h, char32_t ch, const Style& style) {
    for (int row = y; row < y + h; ++row) {
        for (int col = x; col < x + w; ++col) {
            set(col, row, ch, style);
        }
    }
}

int ScreenBuffer::text(int x, int y, std::string_view utf8, const Style& style) {
    std::size_t i = 0;
    while (i < utf8.size()) {
        set(x++, y, nextCodepoint(utf8, i), style);
    }
    return x;
}

void ScreenBuffer::restyle(int x, int y, int w, int h, const Style& style) {
    for (int row = y; row < y + h; ++row) {
        for (int col = x; col < x + w; ++col) {
            if (contains(col, row)) {
                cells_[static_cast<std::size_t>(row * width_ + col)].style = style;
            }
        }
    }
}

const Cell& ScreenBuffer::at(int x, int y) const {
    return contains(x, y) ? cells_[static_cast<std::size_t>(y * width_ + x)] : kOutOfBounds;
}

void encodeDiff(const ScreenBuffer& front, const ScreenBuffer& back, ColorMode mode, bool full,
                std::string& out) {
    const bool repaintAll =
        full || front.width() != back.width() || front.height() != back.height();

    // Where the terminal cursor is after the last glyph we emitted, and the
    // style currently active. Unknown at the start of the frame.
    int cursorX = -1;
    int cursorY = -1;
    bool styleKnown = false;
    Style current{};

    if (repaintAll) {
        // Reset attributes first so the clear uses the default background.
        out += ansi::kResetAttributes;
        out += ansi::kClearScreen;
    }

    for (int y = 0; y < back.height(); ++y) {
        for (int x = 0; x < back.width(); ++x) {
            const Cell& cell = back.at(x, y);
            if (!repaintAll && cell == front.at(x, y)) {
                continue;
            }
            // After a clear, default blank cells are already correct.
            if (repaintAll && cell == Cell{}) {
                continue;
            }
            if (x != cursorX || y != cursorY) {
                ansi::appendMoveTo(out, y, x);
            }
            if (!styleKnown || cell.style != current) {
                ansi::appendStyle(out, cell.style, mode);
                current = cell.style;
                styleKnown = true;
            }
            ansi::appendUtf8(out, cell.ch);
            // Printing advances the cursor one column (auto-wrap is off, so
            // it stays on this row even at the right edge).
            cursorX = x + 1;
            cursorY = y;
        }
    }

    if (styleKnown) {
        out += ansi::kResetAttributes;
    }
}

char32_t nextCodepoint(std::string_view s, std::size_t& i) {
    const auto byte = [&](std::size_t k) { return static_cast<unsigned char>(s[k]); };
    const unsigned char b0 = byte(i);
    int extra = 0;
    char32_t cp = 0;
    if (b0 < 0x80) {
        ++i;
        return b0;
    }
    if ((b0 & 0xE0) == 0xC0) {
        extra = 1;
        cp = b0 & 0x1Fu;
    } else if ((b0 & 0xF0) == 0xE0) {
        extra = 2;
        cp = b0 & 0x0Fu;
    } else if ((b0 & 0xF8) == 0xF0) {
        extra = 3;
        cp = b0 & 0x07u;
    } else {
        ++i;
        return U'�';
    }
    if (i + static_cast<std::size_t>(extra) >= s.size()) {  // truncated sequence
        i = s.size();
        return U'�';
    }
    for (int k = 1; k <= extra; ++k) {
        const unsigned char b = byte(i + static_cast<std::size_t>(k));
        if ((b & 0xC0) != 0x80) {
            i += static_cast<std::size_t>(k);
            return U'�';
        }
        cp = (cp << 6) | (b & 0x3Fu);
    }
    i += static_cast<std::size_t>(extra) + 1;
    return cp;
}

int displayWidth(std::string_view utf8) {
    int width = 0;
    std::size_t i = 0;
    while (i < utf8.size()) {
        (void)nextCodepoint(utf8, i);
        ++width;
    }
    return width;
}

}  // namespace tetromino::tui

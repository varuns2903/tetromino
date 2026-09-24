#include "tui/PixelCanvas.hpp"

#include <algorithm>

#include "tui/ScreenBuffer.hpp"

namespace tetromino::tui {

void PixelCanvas::reset(int width, int height) {
    width_ = std::max(0, width);
    height_ = std::max(0, height);
    pixels_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), Color{});
}

void PixelCanvas::set(int x, int y, Color color) {
    if (x >= 0 && y >= 0 && x < width_ && y < height_) {
        pixels_[static_cast<std::size_t>(y * width_ + x)] = color;
    }
}

void PixelCanvas::fill(int x, int y, int w, int h, Color color) {
    for (int py = y; py < y + h; ++py) {
        for (int px = x; px < x + w; ++px) {
            set(px, py, color);
        }
    }
}

Color PixelCanvas::at(int x, int y) const {
    if (x >= 0 && y >= 0 && x < width_ && y < height_) {
        return pixels_[static_cast<std::size_t>(y * width_ + x)];
    }
    return Color{};
}

void PixelCanvas::blit(ScreenBuffer& buf, int column, int row, Color background) const {
    const int rows = (height_ + 1) / 2;
    for (int r = 0; r < rows; ++r) {
        for (int x = 0; x < width_; ++x) {
            Color top = at(x, 2 * r);
            Color bottom = at(x, 2 * r + 1);
            if (top.isDefault()) {
                top = background;
            }
            if (bottom.isDefault()) {
                bottom = background;
            }
            if (top.isDefault() && bottom.isDefault()) {
                continue;  // fully transparent: leave whatever is there
            }
            if (top == bottom) {
                buf.set(column + x, row + r, U' ', Style{{}, top, false, false});
            } else if (top.isDefault()) {
                // Only the bottom half is coloured; the top shows the
                // terminal's own background.
                buf.set(column + x, row + r, U'▄', Style{bottom, {}, false, false});
            } else {
                buf.set(column + x, row + r, U'▀', Style{top, bottom, false, false});
            }
        }
    }
}

}  // namespace tetromino::tui

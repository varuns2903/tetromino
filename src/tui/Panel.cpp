#include "tui/Panel.hpp"

#include "tui/ScreenBuffer.hpp"

namespace tetromino::tui {

namespace {

struct BorderGlyphs {
    char32_t topLeft, topRight, bottomLeft, bottomRight, horizontal, vertical;
};

constexpr BorderGlyphs glyphsFor(BorderStyle style) {
    switch (style) {
    case BorderStyle::Double: return {U'╔', U'╗', U'╚', U'╝', U'═', U'║'};
    case BorderStyle::Heavy: return {U'┏', U'┓', U'┗', U'┛', U'━', U'┃'};
    case BorderStyle::Rounded: break;
    }
    return {U'╭', U'╮', U'╰', U'╯', U'─', U'│'};
}

void drawFrame(ScreenBuffer& buf, const Rect& r, const PanelStyle& style) {
    if (r.w < 2 || r.h < 2) {
        return;
    }
    const BorderGlyphs g = glyphsFor(style.border);
    if (style.fillInterior) {
        buf.fill(r.x + 1, r.y + 1, r.w - 2, r.h - 2, U' ', style.interior);
    }
    buf.set(r.x, r.y, g.topLeft, style.frame);
    buf.set(r.right() - 1, r.y, g.topRight, style.frame);
    buf.set(r.x, r.bottom() - 1, g.bottomLeft, style.frame);
    buf.set(r.right() - 1, r.bottom() - 1, g.bottomRight, style.frame);
    buf.fill(r.x + 1, r.y, r.w - 2, 1, g.horizontal, style.frame);
    buf.fill(r.x + 1, r.bottom() - 1, r.w - 2, 1, g.horizontal, style.frame);
    buf.fill(r.x, r.y + 1, 1, r.h - 2, g.vertical, style.frame);
    buf.fill(r.right() - 1, r.y + 1, 1, r.h - 2, g.vertical, style.frame);
}

void drawTitle(ScreenBuffer& buf, int x, int y, std::string_view title, const PanelStyle& style) {
    buf.set(x, y, U' ', style.frame);
    const int end = buf.text(x + 1, y, title, style.title);
    buf.set(end, y, U' ', style.frame);
}

}  // namespace

void drawPanel(ScreenBuffer& buf, const Rect& rect, std::string_view title, const PanelStyle& style) {
    drawFrame(buf, rect, style);
    if (!title.empty() && rect.w > displayWidth(title) + 4) {
        drawTitle(buf, rect.x + 2, rect.y, title, style);
    }
}

void drawPanelCentredTitle(ScreenBuffer& buf, const Rect& rect, std::string_view title,
                           const PanelStyle& style) {
    drawFrame(buf, rect, style);
    const int width = displayWidth(title) + 2;
    if (!title.empty() && rect.w > width + 2) {
        drawTitle(buf, rect.x + (rect.w - width) / 2, rect.y, title, style);
    }
}

void drawCentred(ScreenBuffer& buf, int x, int width, int y, std::string_view utf8, const Style& style) {
    const int w = displayWidth(utf8);
    buf.text(x + (width - w) / 2, y, utf8, style);
}

}  // namespace tetromino::tui

#pragma once

// Box-drawing helpers: bordered panels with optional titles.
//
// Uses the Unicode box-drawing block (U+2500..). These glyphs are designed to
// join seamlessly between adjacent cells, which is how every TUI draws its
// frames.

#include <string_view>

#include "tui/Color.hpp"
#include "tui/Layout.hpp"

namespace tetromino::tui {

class ScreenBuffer;

enum class BorderStyle { Rounded, Double, Heavy };

struct PanelStyle {
    BorderStyle border = BorderStyle::Rounded;
    Style frame{};
    Style title{};
    // If set, the interior is filled with spaces in this style.
    bool fillInterior = false;
    Style interior{};
};

// Draw a frame around `rect` (the border occupies the rect's outermost cells).
// A non-empty title is embedded in the top edge: ╭─ TITLE ───╮
void drawPanel(ScreenBuffer& buf, const Rect& rect, std::string_view title, const PanelStyle& style);

// Draw a frame with the title centred in the top edge: ╭──── TITLE ────╮
void drawPanelCentredTitle(ScreenBuffer& buf, const Rect& rect, std::string_view title,
                           const PanelStyle& style);

// Write text centred horizontally within [x, x + width).
void drawCentred(ScreenBuffer& buf, int x, int width, int y, std::string_view utf8, const Style& style);

}  // namespace tetromino::tui

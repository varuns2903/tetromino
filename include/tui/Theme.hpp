#pragma once

// Visual theme: which colours and glyphs represent what.
//
// The renderer asks the theme "how do I draw a locked T cell?" and never
// hardcodes colours itself. A monochrome theme exists for NO_COLOR / dumb
// terminals; there, pieces are distinguished by glyph shade instead.

#include <array>

#include "core/Types.hpp"
#include "tui/Color.hpp"

namespace tetromino::tui {

// How one board cell (two terminal columns wide) is drawn.
struct CellGlyph {
    char32_t left = U' ';
    char32_t right = U' ';
    Style style{};
};

class Theme {
public:
    static Theme standard();
    static Theme monochrome();

    [[nodiscard]] bool isMonochrome() const { return monochrome_; }

    [[nodiscard]] Color pieceColor(core::PieceType type) const {
        return pieceColors_[core::indexOf(type)];
    }

    // Colours for pixel (half-block) rendering.
    [[nodiscard]] Color lockedColor(core::PieceType type) const;
    [[nodiscard]] Color ghostColor(core::PieceType type) const;
    // Solid colour that looks like the '░' ghost glyph (for pixel rendering).
    [[nodiscard]] Color ghostFillColor(core::PieceType type) const;
    [[nodiscard]] Color clearingColor(core::PieceType type, float progress) const;
    [[nodiscard]] Color gridDotColor() const { return gridDot_; }

    // Glyphs for character rendering (monochrome terminals).
    [[nodiscard]] CellGlyph emptyCell() const;
    [[nodiscard]] CellGlyph activeCell(core::PieceType type) const;
    [[nodiscard]] CellGlyph lockedCell(core::PieceType type) const;
    [[nodiscard]] CellGlyph ghostCell(core::PieceType type) const;
    // A row being cleared, `progress` 0 -> 1.
    [[nodiscard]] CellGlyph clearingCell(core::PieceType type, float progress) const;

    // Chrome.
    [[nodiscard]] const Style& frame() const { return frame_; }
    [[nodiscard]] const Style& boardFrame() const { return boardFrame_; }
    [[nodiscard]] const Style& heading() const { return heading_; }
    [[nodiscard]] const Style& label() const { return label_; }
    [[nodiscard]] const Style& value() const { return value_; }
    [[nodiscard]] const Style& muted() const { return muted_; }
    [[nodiscard]] const Style& accent() const { return accent_; }
    [[nodiscard]] const Style& overlay() const { return overlay_; }
    [[nodiscard]] const Style& overlayFrame() const { return overlayFrame_; }
    [[nodiscard]] const Style& warning() const { return warning_; }
    [[nodiscard]] Color wellBackground() const { return wellBg_; }

private:
    bool monochrome_ = false;
    std::array<Color, core::kPieceTypeCount> pieceColors_{};
    Color wellBg_{};
    Color gridDot_{};
    Style frame_{};
    Style boardFrame_{};
    Style heading_{};
    Style label_{};
    Style value_{};
    Style muted_{};
    Style accent_{};
    Style overlay_{};
    Style overlayFrame_{};
    Style warning_{};
};

}  // namespace tetromino::tui

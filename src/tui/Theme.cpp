#include "tui/Theme.hpp"

namespace tetromino::tui {

namespace {

constexpr char32_t kFullBlock = U'█';
constexpr char32_t kDarkShade = U'▓';
constexpr char32_t kLightShade = U'░';
constexpr char32_t kMiddleDot = U'·';

}  // namespace

Theme Theme::standard() {
    Theme t;
    t.monochrome_ = false;
    // Tetromino's own palette. It deliberately does *not* reuse the familiar
    // colour-per-piece scheme of other falling-block games (no piece has its
    // "usual" colour), so the game has its own look.
    t.pieceColors_ = {
        Color::rgb(255, 122, 92),   // I coral
        Color::rgb(110, 196, 255),  // O sky
        Color::rgb(255, 196, 84),   // T amber
        Color::rgb(196, 128, 255),  // S violet
        Color::rgb(80, 220, 170),   // Z mint
        Color::rgb(255, 112, 176),  // J rose
        Color::rgb(170, 225, 80),   // L lime
    };
    t.wellBg_ = Color::rgb(16, 17, 24);
    t.gridDot_ = Color::rgb(44, 46, 60);
    t.frame_ = Style{Color::rgb(90, 95, 120), {}, false, false};
    t.boardFrame_ = Style{Color::rgb(140, 150, 190), {}, false, false};
    t.heading_ = Style{Color::rgb(200, 205, 230), {}, true, false};
    t.label_ = Style{Color::rgb(125, 130, 160), {}, false, false};
    t.value_ = Style{Color::rgb(240, 240, 250), {}, true, false};
    t.muted_ = Style{Color::rgb(95, 100, 125), {}, false, false};
    t.accent_ = Style{Color::rgb(80, 220, 170), {}, true, false};
    t.overlay_ = Style{Color::rgb(230, 230, 245), Color::rgb(24, 26, 38), false, false};
    t.overlayFrame_ = Style{Color::rgb(80, 220, 170), Color::rgb(24, 26, 38), true, false};
    t.warning_ = Style{Color::rgb(245, 190, 60), {}, true, false};
    t.danger_ = Style{Color::rgb(240, 80, 80), {}, true, false};
    t.panelBg_ = Color::rgb(28, 28, 32);
    return t;
}

Theme Theme::monochrome() {
    Theme t;
    t.monochrome_ = true;
    t.pieceColors_.fill(Color::terminalDefault());
    t.frame_ = Style{};
    t.boardFrame_ = Style{{}, {}, true, false};
    t.heading_ = Style{{}, {}, true, false};
    t.label_ = Style{{}, {}, false, true};
    t.value_ = Style{{}, {}, true, false};
    t.muted_ = Style{{}, {}, false, true};
    t.accent_ = Style{{}, {}, true, false};
    t.overlay_ = Style{};
    t.overlayFrame_ = Style{{}, {}, true, false};
    t.warning_ = Style{{}, {}, true, false};
    t.danger_ = Style{{}, {}, true, false};
    return t;
}

CellGlyph Theme::emptyCell() const {
    if (monochrome_) {
        return {U' ', kMiddleDot, Style{{}, {}, false, true}};
    }
    // A faint dot grid on a slightly lifted background makes the well
    // readable without competing with the pieces.
    return {U' ', kMiddleDot, Style{gridDot_, wellBg_, false, false}};
}

CellGlyph Theme::activeCell(core::PieceType type) const {
    if (monochrome_) {
        return {kFullBlock, kFullBlock, Style{{}, {}, true, false}};
    }
    const Color c = pieceColor(type);
    return {kFullBlock, kFullBlock, Style{c, wellBg_, false, false}};
}

CellGlyph Theme::lockedCell(core::PieceType type) const {
    if (monochrome_) {
        return {kDarkShade, kDarkShade, Style{}};
    }
    return {kFullBlock, kFullBlock, Style{lockedColor(type), wellBg_, false, false}};
}

CellGlyph Theme::ghostCell(core::PieceType type) const {
    if (monochrome_) {
        return {kLightShade, kLightShade, Style{{}, {}, false, true}};
    }
    return {kLightShade, kLightShade, Style{ghostColor(type), wellBg_, false, false}};
}

CellGlyph Theme::clearingCell(core::PieceType type, float progress) const {
    if (monochrome_) {
        return {kFullBlock, kFullBlock, Style{{}, {}, true, false}};
    }
    return {kFullBlock, kFullBlock, Style{clearingColor(type, progress), wellBg_, false, false}};
}

Color Theme::lockedColor(core::PieceType type) const {
    // Locked pieces a touch darker than the falling one, so the active piece
    // stands out in a busy stack.
    return pieceColor(type).darkened(0.82F);
}

Color Theme::ghostColor(core::PieceType type) const { return pieceColor(type).blended(wellBg_, 0.55F); }

Color Theme::ghostFillColor(core::PieceType type) const {
    // '░' covers roughly a quarter of the cell with the foreground colour, so
    // the ghost glyph reads as the ghost colour mixed 1:3 with the well.
    return ghostColor(type).blended(wellBg_, 0.75F);
}

Color Theme::clearingColor(core::PieceType type, float progress) const {
    // Flash to white, then fade into the well.
    const Color white = Color::rgb(255, 255, 255);
    const Color base = pieceColor(type);
    return progress < 0.35F ? base.blended(white, progress / 0.35F)
                            : white.blended(wellBg_, (progress - 0.35F) / 0.65F);
}

}  // namespace tetromino::tui

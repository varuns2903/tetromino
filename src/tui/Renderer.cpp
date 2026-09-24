#include "tui/Renderer.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <string_view>

#include "core/GameState.hpp"
#include "game/Board.hpp"
#include "game/Piece.hpp"
#include "tui/Ansi.hpp"
#include "tui/Panel.hpp"
#include "tui/Terminal.hpp"

namespace tetromino::tui {

using core::GameMode;
using core::GameState;
using core::PieceType;
using game::Board;

namespace {

constexpr std::string_view kBoardTitle = "TETROMINO";
constexpr int kLinesPerLevel = 10;

// Number formatting into a caller-provided buffer: no heap allocation per frame.
using NumberBuffer = std::array<char, 32>;

std::string_view formatInt(NumberBuffer& buf, std::uint64_t value) {
    const auto [end, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
    return ec == std::errc{} ? std::string_view{buf.data(), static_cast<std::size_t>(end - buf.data())}
                             : std::string_view{"?"};
}

// 1234567 -> "1,234,567"
std::string_view formatThousands(NumberBuffer& buf, std::uint64_t value) {
    NumberBuffer digits{};
    const std::string_view d = formatInt(digits, value);
    std::size_t out = 0;
    for (std::size_t i = 0; i < d.size(); ++i) {
        if (i > 0 && (d.size() - i) % 3 == 0) {
            buf[out++] = ',';
        }
        buf[out++] = d[i];
    }
    return {buf.data(), out};
}

PieceType pieceOf(core::CellType cell) {
    return static_cast<PieceType>(static_cast<std::uint8_t>(cell) - 1);
}

// "LABEL      value" across `width` columns.
void drawLabelValue(ScreenBuffer& buf, int x, int y, int width, std::string_view label,
                    std::string_view value, const Style& labelStyle, const Style& valueStyle) {
    buf.text(x, y, label, labelStyle);
    buf.text(x + width - displayWidth(value), y, value, valueStyle);
}

// --- Start screen logo: 5-row block letters -----------------------------
// Each '#' is one "pixel". Letters have different widths (M and N need more
// than three pixels to be readable).

struct LogoLetter {
    std::array<std::string_view, 5> rows;
};

constexpr std::array<LogoLetter, 9> kLogo{{
    {{"###", ".#.", ".#.", ".#.", ".#."}},              // T
    {{"###", "#..", "##.", "#..", "###"}},              // E
    {{"###", ".#.", ".#.", ".#.", ".#."}},              // T
    {{"##.", "#.#", "##.", "#.#", "#.#"}},              // R
    {{"###", "#.#", "#.#", "#.#", "###"}},              // O
    {{"#...#", "##.##", "#.#.#", "#...#", "#...#"}},    // M
    {{"###", ".#.", ".#.", ".#.", "###"}},              // I
    {{"#..#", "##.#", "#.##", "#..#", "#..#"}},         // N
    {{"###", "#.#", "#.#", "#.#", "###"}},              // O
}};

constexpr int kLogoHeight = 5;

// Width in pixels, including one blank pixel between letters.
constexpr int logoPixelWidth() {
    int width = -1;
    for (const LogoLetter& letter : kLogo) {
        width += static_cast<int>(letter.rows[0].size()) + 1;
    }
    return width;
}

}  // namespace

Renderer::Renderer(Theme theme, ColorMode mode) : theme_(std::move(theme)), mode_(mode) {}

void Renderer::resize(TerminalSize size) {
    layout_ = computeLayout(size);
    back_.resize(size.columns, size.rows);
    fullRepaint_ = true;
}

void Renderer::render(const GameState& state) {
    back_.clear();
    if (!layout_.fits) {
        drawTooSmall();
        return;
    }
    switch (state.mode) {
    case GameMode::StartScreen:
        drawStartScreen(state);
        break;
    case GameMode::Playing:
    case GameMode::Quit:
        drawPlayfield(state);
        break;
    case GameMode::Paused:
        drawPlayfield(state);
        drawPauseOverlay();
        break;
    case GameMode::GameOver:
        drawPlayfield(state);
        drawGameOverOverlay(state);
        break;
    }
}

bool Renderer::present(Terminal& terminal) {
    output_.clear();
    output_ += ansi::kBeginSynchronizedUpdate;
    const std::size_t header = output_.size();
    encodeDiff(front_, back_, mode_, fullRepaint_, output_);
    fullRepaint_ = false;
    front_ = back_;  // same size after the first frame: copy-assign reuses storage

    if (output_.size() == header) {
        return true;  // nothing changed; don't touch the terminal at all
    }
    output_ += ansi::kEndSynchronizedUpdate;
    return terminal.write(output_);
}

// ---------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------

void Renderer::drawTooSmall() {
    const int w = back_.width();
    const int h = back_.height();
    NumberBuffer a{};
    NumberBuffer b{};
    const int top = std::max(0, h / 2 - 3);
    drawCentred(back_, 0, w, top, "Terminal too small.", theme_.warning());
    drawCentred(back_, 0, w, top + 2, "Please resize to at least:", theme_.label());

    // "56 × 22"
    const std::string_view cols = formatInt(a, static_cast<std::uint64_t>(layout_.minimum.columns));
    const std::string_view rows = formatInt(b, static_cast<std::uint64_t>(layout_.minimum.rows));
    const int needW = displayWidth(cols) + 3 + displayWidth(rows);
    int x = (w - needW) / 2;
    x = back_.text(x, top + 3, cols, theme_.value());
    x = back_.text(x, top + 3, " × ", theme_.label());
    back_.text(x, top + 3, rows, theme_.value());

    NumberBuffer c{};
    NumberBuffer d{};
    const std::string_view curC = formatInt(c, static_cast<std::uint64_t>(layout_.terminal.columns));
    const std::string_view curR = formatInt(d, static_cast<std::uint64_t>(layout_.terminal.rows));
    const int curW = 9 + displayWidth(curC) + 3 + displayWidth(curR) + 1;
    x = (w - curW) / 2;
    x = back_.text(x, top + 5, "(current ", theme_.muted());
    x = back_.text(x, top + 5, curC, theme_.muted());
    x = back_.text(x, top + 5, " × ", theme_.muted());
    x = back_.text(x, top + 5, curR, theme_.muted());
    back_.text(x, top + 5, ")", theme_.muted());
}

void Renderer::drawStartScreen(const GameState& /*state*/) {
    const int w = back_.width();
    constexpr int kBoxW = 36;
    constexpr int kBoxH = 8;
    constexpr int kTotalH = kLogoHeight + 2 + 1 + 2 + kBoxH + 2 + 1;
    int y = std::max(0, (back_.height() - kTotalH) / 2);

    // Logo. Pixels are two columns wide
    // (roughly square) when there's room, one column on narrow terminals.
    const int pixelW = logoPixelWidth() * 2 <= w - 2 ? 2 : 1;
    int x = (w - logoPixelWidth() * pixelW) / 2;
    for (const LogoLetter& letter : kLogo) {
        const int letterW = static_cast<int>(letter.rows[0].size());
        for (int row = 0; row < kLogoHeight; ++row) {
            // One vertical gradient across the whole word (mint -> violet).
            const float t = static_cast<float>(row) / static_cast<float>(kLogoHeight - 1);
            Style style{};
            if (!theme_.isMonochrome()) {
                style.fg = Color::rgb(80, 220, 170).blended(Color::rgb(196, 128, 255), t);
            }
            const std::string_view line = letter.rows[static_cast<std::size_t>(row)];
            for (int col = 0; col < letterW; ++col) {
                if (line[static_cast<std::size_t>(col)] == '#') {
                    back_.set(x + col * pixelW, y + row, U'█', style);
                    back_.set(x + col * pixelW + pixelW - 1, y + row, U'█', style);
                }
            }
        }
        x += (letterW + 1) * pixelW;
    }
    y += kLogoHeight + 2;

    drawCentred(back_, 0, w, y, "T E R M I N A L   E D I T I O N", theme_.label());
    y += 3;

    const Rect box{(w - kBoxW) / 2, y, kBoxW, kBoxH};
    drawPanel(back_, box, "", PanelStyle{BorderStyle::Double, theme_.overlayFrame(), theme_.heading(), true, theme_.overlay()});
    const int ix = box.x + 9;
    Style key = theme_.accent();
    key.bg = theme_.overlay().bg;
    back_.text(ix, box.y + 2, "ENTER", key);
    back_.text(ix + 8, box.y + 2, "Start game", theme_.overlay());
    back_.text(ix, box.y + 3, "Q", key);
    back_.text(ix + 8, box.y + 3, "Quit", theme_.overlay());
    Style hint = theme_.muted();
    hint.bg = theme_.overlay().bg;
    drawCentred(back_, box.x, box.w, box.y + 5, "hold · ghost · wall kicks", hint);
    y += kBoxH + 2;

    drawCentred(back_, 0, w, y, "←→ move   ↑ rotate   ↓ soft   space drop   c hold   p pause",
                theme_.muted());
}

void Renderer::drawPlayfield(const GameState& state) {
    const bool paused = state.mode == GameMode::Paused;
    drawBoard(state, paused);
    drawHold(state);
    drawStats(state);
    drawKeys();
    drawNext(state);
    drawMessage(state);
}

// ---------------------------------------------------------------------------
// Board
// ---------------------------------------------------------------------------

void Renderer::drawBoardCell(int col, int row, const CellGlyph& glyph) {
    const int cw = layout_.cellWidth;
    const int ch = layout_.cellHeight;
    const int x0 = layout_.well.x + col * cw;
    const int y0 = layout_.well.y + row * ch;
    for (int dy = 0; dy < ch; ++dy) {
        for (int dx = 0; dx < cw; ++dx) {
            // The right-hand glyph only goes in the last column of the first
            // row; that's where the empty-cell grid dot sits at any scale.
            const bool rightSlot = dx == cw - 1 && dy == 0;
            back_.set(x0 + dx, y0 + dy, rightSlot ? glyph.right : glyph.left, glyph.style);
        }
    }
}

void Renderer::drawBoard(const GameState& state, bool hideContents) {
    drawPanelCentredTitle(back_, layout_.board, kBoardTitle,
                          PanelStyle{BorderStyle::Rounded, theme_.boardFrame(), theme_.accent(), false, {}});

    const CellGlyph empty = theme_.emptyCell();
    const bool gameOver = state.mode == GameMode::GameOver;

    for (int row = 0; row < Board::kVisibleHeight; ++row) {
        const int y = row + Board::kHiddenRows;
        const bool clearing =
            std::find(state.clearingRows.begin(), state.clearingRows.end(), y) != state.clearingRows.end();
        for (int col = 0; col < Board::kWidth; ++col) {
            const core::CellType cell = state.board.at({col, y});
            if (hideContents || cell == core::CellType::Empty) {
                drawBoardCell(col, row, empty);
                continue;
            }
            if (clearing) {
                // Flash, then wipe outwards from the centre of the row.
                const float centreDistance = std::abs(static_cast<float>(col) - 4.5F);
                const bool wiped = centreDistance < state.clearProgress * 6.0F - 1.0F;
                drawBoardCell(col, row, wiped ? empty : theme_.clearingCell(pieceOf(cell), state.clearProgress));
                continue;
            }
            CellGlyph g = theme_.lockedCell(pieceOf(cell));
            if (gameOver && !theme_.isMonochrome()) {
                g.style.fg = g.style.fg.blended(theme_.wellBackground(), 0.6F);
            }
            drawBoardCell(col, row, g);
        }
    }

    if (hideContents || !state.active) {
        return;
    }

    const auto drawPiece = [this](const game::Piece& piece, const CellGlyph& glyph) {
        for (const core::Point p : game::cellsOf(piece)) {
            const int row = p.y - Board::kHiddenRows;
            if (row >= 0 && row < Board::kVisibleHeight) {
                drawBoardCell(p.x, row, glyph);
            }
        }
    };

    if (const auto ghost = state.ghost(); ghost && *ghost != *state.active) {
        drawPiece(*ghost, theme_.ghostCell(ghost->type));
    }
    drawPiece(*state.active, theme_.activeCell(state.active->type));
}

// ---------------------------------------------------------------------------
// Side panels
// ---------------------------------------------------------------------------

void Renderer::drawMiniPiece(PieceType type, int x, int y, int width, bool dimmed) {
    const game::PieceCells& shape = game::shapeOf(type, core::Rotation::Spawn);
    int minX = 4;
    int maxX = 0;
    int minY = 4;
    int maxY = 0;
    for (const core::Point p : shape) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }
    const int pieceW = (maxX - minX + 1) * 2;
    const int startX = x + (width - pieceW) / 2;
    const int startY = y + (2 - (maxY - minY + 1)) / 2;

    CellGlyph g = theme_.activeCell(type);
    if (dimmed) {
        g = theme_.ghostCell(type);
    }
    g.style.bg = {};  // side panels use the terminal background
    for (const core::Point p : shape) {
        const int cx = startX + (p.x - minX) * 2;
        const int cy = startY + (p.y - minY);
        back_.set(cx, cy, g.left, g.style);
        back_.set(cx + 1, cy, g.right, g.style);
    }
}

void Renderer::drawHold(const GameState& state) {
    const Rect& r = layout_.hold;
    drawPanel(back_, r, "HOLD", PanelStyle{BorderStyle::Rounded, theme_.frame(), theme_.heading(), false, {}});
    if (state.held && state.mode != GameMode::Paused) {
        drawMiniPiece(*state.held, r.x + 1, r.y + 2, r.w - 2, !state.holdAvailable);
    }
}

void Renderer::drawStats(const GameState& state) {
    const Rect& r = layout_.stats;
    drawPanel(back_, r, "SCORE", PanelStyle{BorderStyle::Rounded, theme_.frame(), theme_.heading(), false, {}});
    const int x = r.x + 2;
    const int w = r.w - 4;

    NumberBuffer buf{};
    const std::string_view score = formatThousands(buf, state.stats.score);
    back_.text(x + w - displayWidth(score), r.y + 1, score, theme_.value());

    drawLabelValue(back_, x, r.y + 3, w, "LEVEL", formatInt(buf, static_cast<std::uint64_t>(state.stats.level)),
                   theme_.label(), theme_.value());
    drawLabelValue(back_, x, r.y + 4, w, "LINES", formatInt(buf, static_cast<std::uint64_t>(state.stats.lines)),
                   theme_.label(), theme_.value());

    // Progress towards the next level.
    const int filled = (state.stats.lines % kLinesPerLevel) * w / kLinesPerLevel;
    Style on = theme_.accent();
    on.bold = false;
    for (int i = 0; i < w; ++i) {
        back_.set(x + i, r.y + 6, i < filled ? U'━' : U'─', i < filled ? on : theme_.muted());
    }
}

void Renderer::drawKeys() {
    const Rect& r = layout_.keys;
    if (r.w == 0) {
        return;
    }
    drawPanel(back_, r, "KEYS", PanelStyle{BorderStyle::Rounded, theme_.frame(), theme_.heading(), false, {}});

    struct Binding {
        std::string_view key;
        std::string_view what;
    };
    static constexpr std::array<Binding, 8> kFull{{
        {"← →", "move"},
        {"↑ x", "rotate"},
        {"z", "rotate ↺"},
        {"↓", "soft drop"},
        {"spc", "hard drop"},
        {"c", "hold"},
        {"p", "pause"},
        {"q", "quit"},
    }};
    static constexpr std::array<Binding, 6> kCompact{{
        {"← →", "move"},
        {"↑ x", "rotate"},
        {"↓", "soft drop"},
        {"spc", "hard drop"},
        {"c", "hold"},
        {"p", "pause"},
    }};

    const int rows = r.h - 2;
    const auto draw = [&](const auto& list) {
        int y = r.y + 1;
        for (const Binding& b : list) {
            if (y >= r.bottom() - 1) {
                break;
            }
            back_.text(r.x + 2, y, b.key, theme_.value());
            back_.text(r.x + 6, y, b.what, theme_.label());
            ++y;
        }
    };
    if (rows >= static_cast<int>(kFull.size())) {
        draw(kFull);
    } else {
        draw(kCompact);
    }
}

void Renderer::drawNext(const GameState& state) {
    const Rect& r = layout_.next;
    drawPanel(back_, r, "NEXT", PanelStyle{BorderStyle::Rounded, theme_.frame(), theme_.heading(), false, {}});
    if (state.mode == GameMode::Paused) {
        return;
    }
    for (int i = 0; i < layout_.previewCount; ++i) {
        drawMiniPiece(state.preview[static_cast<std::size_t>(i)], r.x + 1, r.y + 2 + i * 3, r.w - 2, false);
    }
}

void Renderer::drawMessage(const GameState& state) {
    if (!state.isClearingLines() || layout_.message.h < 2) {
        return;
    }
    static constexpr std::array<std::string_view, 5> kNames{"", "SINGLE", "DOUBLE", "TRIPLE", "QUAD!"};
    const std::size_t n = std::min(state.clearingRows.size(), kNames.size() - 1);
    const Rect& r = layout_.message;
    Style s = n == 4 ? theme_.warning() : theme_.accent();
    drawCentred(back_, r.x, r.w, r.y + 1, kNames[n], s);
}

// ---------------------------------------------------------------------------
// Overlays
// ---------------------------------------------------------------------------

Rect Renderer::drawOverlayBox(int innerHeight, std::string_view title) {
    const Rect& well = layout_.well;
    const int w = std::min(well.w, 28);
    const int h = innerHeight + 2;
    const Rect box{well.x + (well.w - w) / 2, well.y + (well.h - h) / 2, w, h};
    drawPanelCentredTitle(back_, box, title,
                          PanelStyle{BorderStyle::Double, theme_.overlayFrame(), theme_.overlayFrame(), true, theme_.overlay()});
    return box.inset(1);
}

void Renderer::drawPauseOverlay() {
    const Rect in = drawOverlayBox(7, "");
    Style title = theme_.overlayFrame();
    drawCentred(back_, in.x, in.w, in.y + 1, "PAUSED", title);

    Style key = theme_.accent();
    key.bg = theme_.overlay().bg;
    const int x = in.x + (in.w - 12) / 2;
    back_.text(x, in.y + 3, "P", key);
    back_.text(x + 3, in.y + 3, "resume", theme_.overlay());
    back_.text(x, in.y + 4, "R", key);
    back_.text(x + 3, in.y + 4, "restart", theme_.overlay());
    back_.text(x, in.y + 5, "Q", key);
    back_.text(x + 3, in.y + 5, "quit", theme_.overlay());
}

void Renderer::drawGameOverOverlay(const GameState& state) {
    const Rect in = drawOverlayBox(10, "");
    Style title = theme_.overlayFrame();
    title.fg = theme_.isMonochrome() ? Color{} : Color::rgb(240, 80, 80);
    drawCentred(back_, in.x, in.w, in.y + 1, "GAME OVER", title);

    Style label = theme_.overlay();
    Style value = theme_.overlay();
    value.bold = true;
    const int x = in.x + 2;
    const int w = in.w - 4;
    NumberBuffer buf{};
    drawLabelValue(back_, x, in.y + 3, w, "Score", formatThousands(buf, state.stats.score), label, value);
    drawLabelValue(back_, x, in.y + 4, w, "Lines", formatInt(buf, static_cast<std::uint64_t>(state.stats.lines)),
                   label, value);
    drawLabelValue(back_, x, in.y + 5, w, "Level", formatInt(buf, static_cast<std::uint64_t>(state.stats.level)),
                   label, value);

    Style key = theme_.accent();
    key.bg = theme_.overlay().bg;
    const int kx = in.x + (in.w - 12) / 2;
    back_.text(kx, in.y + 7, "R", key);
    back_.text(kx + 3, in.y + 7, "restart", theme_.overlay());
    back_.text(kx, in.y + 8, "Q", key);
    back_.text(kx + 3, in.y + 8, "quit", theme_.overlay());
}

}  // namespace tetromino::tui

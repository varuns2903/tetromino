#include "tui/Renderer.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <format>
#include <string_view>

#include "core/GameState.hpp"
#include "core/Presets.hpp"
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

// std::format into a fixed buffer (truncating if too long): no allocation.
template <std::size_t N, typename... Args>
std::string_view formatInto(std::array<char, N>& buf, std::format_string<Args...> fmt, Args&&... args) {
    const auto result = std::format_to_n(buf.data(), static_cast<std::ptrdiff_t>(N), fmt, std::forward<Args>(args)...);
    return {buf.data(), static_cast<std::size_t>(std::min(result.out - buf.data(), static_cast<std::ptrdiff_t>(N)))};
}

// 83.4 s -> "1:23" (or "1:23.4" with tenths).
std::string_view formatClock(NumberBuffer& buf, core::Duration d, bool tenths) {
    const auto totalTenths = std::chrono::duration_cast<std::chrono::milliseconds>(d).count() / 100;
    const long long minutes = totalTenths / 600;
    const long long seconds = (totalTenths / 10) % 60;
    const long long tenth = totalTenths % 10;
    return tenths ? formatInto(buf, "{}:{:02}.{}", minutes, seconds, tenth) : formatInto(buf, "{}:{:02}", minutes, seconds);
}

// 3x5 block digits for the resume countdown.
constexpr std::array<std::array<std::string_view, 5>, 10> kDigits{{
    {"###", "#.#", "#.#", "#.#", "###"}, {".#.", "##.", ".#.", ".#.", "###"}, {"###", "..#", "###", "#..", "###"},
    {"###", "..#", "###", "..#", "###"}, {"#.#", "#.#", "###", "..#", "..#"}, {"###", "#..", "###", "..#", "###"},
    {"###", "#..", "###", "#.#", "###"}, {"###", "..#", "..#", "..#", "..#"}, {"###", "#.#", "###", "#.#", "###"},
    {"###", "#.#", "###", "..#", "###"},
}};

// True if the stack reaches into the top few visible rows.
bool inDanger(const GameState& state) {
    const int dangerRows = std::max(4, state.board.visibleHeight() * 3 / 10);
    for (int y = Board::kHiddenRows; y < Board::kHiddenRows + dangerRows; ++y) {
        if (!state.board.isRowEmpty(y)) {
            return true;
        }
    }
    return false;
}

std::string_view clearName(const core::Feedback& f) {
    using game::SpinKind;
    static constexpr std::array<std::string_view, 5> kPlain{"", "SINGLE", "DOUBLE", "TRIPLE", "QUAD!"};
    static constexpr std::array<std::string_view, 4> kSpin{"SPIN", "SPIN SINGLE", "SPIN DOUBLE", "SPIN TRIPLE"};
    static constexpr std::array<std::string_view, 4> kMini{"MINI SPIN", "MINI SINGLE", "MINI DOUBLE", "MINI DOUBLE"};
    const auto n = static_cast<std::size_t>(std::clamp(f.lines, 0, 4));
    switch (f.spin) {
    case SpinKind::Full: return kSpin[std::min<std::size_t>(n, 3)];
    case SpinKind::Mini: return kMini[std::min<std::size_t>(n, 3)];
    case SpinKind::None: break;
    }
    return kPlain[n];
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
    {{"###", "#.#", "##.", "#.#", "#.#"}},              // R
    {{"###", "#.#", "#.#", "#.#", "###"}},              // O
    {{"#...#", "##.##", "#.#.#", "#...#", "#...#"}},    // M
    {{"###", ".#.", ".#.", ".#.", "###"}},              // I
    {{"#..#", "##.#", "#.##", "#..#", "#..#"}},         // N
    {{"###", "#.#", "#.#", "#.#", "###"}},              // O
}};

constexpr int kLogoHeight = 5;

// Start screen geometry.
constexpr int kMenuWidth = 46;
constexpr int kMenuHeight = 13;
constexpr TerminalSize kStartScreenMinimum{kMenuWidth + 4, kLogoHeight + 3 + 1 + kMenuHeight};

// Width in pixels, including one blank pixel between letters.
constexpr int logoPixelWidth() {
    int width = -1;
    for (const LogoLetter& letter : kLogo) {
        width += static_cast<int>(letter.rows[0].size()) + 1;
    }
    return width;
}

}  // namespace

Renderer::Renderer(Theme theme, ColorMode mode)
    : theme_(std::move(theme)), mode_(mode), halfBlocks_(mode != ColorMode::Monochrome && !theme_.isMonochrome()) {}

void Renderer::resize(TerminalSize size) {
    terminal_ = size;
    layoutValid_ = false;
    back_.resize(size.columns, size.rows);
    fullRepaint_ = true;
}

const Layout& Renderer::layoutFor(const GameState& state) {
    const BoardShape shape{state.board.width(), state.board.visibleHeight()};
    const int previews = state.rules.previewCount;
    if (!layoutValid_ || shape != layoutShape_ || previews != layoutPreviews_) {
        layout_ = computeLayout(terminal_, shape, previews, halfBlocks_);
        layoutValid_ = true;
        layoutShape_ = shape;
        layoutPreviews_ = previews;
        fullRepaint_ = true;  // everything moved
    }
    return layout_;
}

void Renderer::render(const GameState& state, const HudInfo& hud) {
    hud_ = hud;
    back_.clear();
    if (state.mode == GameMode::StartScreen) {
        if (terminal_.columns < kStartScreenMinimum.columns || terminal_.rows < kStartScreenMinimum.rows) {
            drawTooSmall(kStartScreenMinimum);
        } else {
            drawStartScreen(state);
        }
        return;
    }
    if (!layoutFor(state).fits) {
        drawTooSmall(layout_.minimum);
        return;
    }
    switch (state.mode) {
    case GameMode::StartScreen:
        break;
    case GameMode::Playing:
    case GameMode::Quit:
        drawPlayfield(state);
        break;
    case GameMode::Paused:
        drawPlayfield(state);
        drawPauseOverlay();
        break;
    case GameMode::Countdown:
        drawPlayfield(state);
        drawCountdownOverlay(state);
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

void Renderer::drawTooSmall(TerminalSize minimum) {
    const int w = back_.width();
    const int h = back_.height();
    NumberBuffer a{};
    NumberBuffer b{};
    const int top = std::max(0, h / 2 - 3);
    drawCentred(back_, 0, w, top, "Terminal too small.", theme_.warning());
    drawCentred(back_, 0, w, top + 2, "Please resize to at least:", theme_.label());

    // "56 × 22"
    const std::string_view cols = formatInt(a, static_cast<std::uint64_t>(minimum.columns));
    const std::string_view rows = formatInt(b, static_cast<std::uint64_t>(minimum.rows));
    const int needW = displayWidth(cols) + 3 + displayWidth(rows);
    int x = (w - needW) / 2;
    x = back_.text(x, top + 3, cols, theme_.value());
    x = back_.text(x, top + 3, " × ", theme_.label());
    back_.text(x, top + 3, rows, theme_.value());

    NumberBuffer c{};
    NumberBuffer d{};
    const std::string_view curC = formatInt(c, static_cast<std::uint64_t>(terminal_.columns));
    const std::string_view curR = formatInt(d, static_cast<std::uint64_t>(terminal_.rows));
    const int curW = 9 + displayWidth(curC) + 3 + displayWidth(curR) + 1;
    x = (w - curW) / 2;
    x = back_.text(x, top + 5, "(current ", theme_.muted());
    x = back_.text(x, top + 5, curC, theme_.muted());
    x = back_.text(x, top + 5, " × ", theme_.muted());
    x = back_.text(x, top + 5, curR, theme_.muted());
    back_.text(x, top + 5, ")", theme_.muted());
}

void Renderer::drawStartScreen(const GameState& state) {
    const int w = back_.width();
    constexpr int kTotalH = kLogoHeight + 1 + 1 + 1 + kMenuHeight;
    int y = std::max(0, (back_.height() - kTotalH) / 2);

    // Logo. Pixels are two columns wide (roughly square) when there's room,
    // one column on narrow terminals.
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
    y += kLogoHeight + 1;

    drawCentred(back_, 0, w, y, "T E R M I N A L   E D I T I O N", theme_.label());
    y += 2;

    drawSetupMenu(state, y);
}

void Renderer::drawSetupMenu(const GameState& state, int y) {
    const core::Setup& setup = state.setup;
    const core::GameType& type = core::kGameTypes[setup.gameType];
    const core::BoardSize& size = core::kBoardSizes[setup.boardSize];
    const core::Difficulty& difficulty = core::kDifficulties[setup.difficulty];

    //  0 ╔════════════════════════════════════════════╗
    //  2 ║ › MODE         ◂  Endless              ▸   ║
    //  3 ║      play until the stack reaches the top  ║
    //  5 ║   BOARD        ◂  Classic  10 × 20     ▸   ║
    //  6 ║   DIFFICULTY   ◂  Normal               ▸   ║
    //  7 ║          hold · 3 next · ghost             ║
    //  9 ║           BEST  12,450                     ║
    // 11 ║  ↑↓ choose  ←→ change  ENTER start  Q quit ║
    const Rect box{(back_.width() - kMenuWidth) / 2, y, kMenuWidth, kMenuHeight};
    drawPanel(back_, box, "", PanelStyle{BorderStyle::Double, theme_.overlayFrame(), theme_.heading(), true, theme_.overlay()});

    const Color bg = theme_.overlay().bg;
    Style normal = theme_.overlay();
    Style muted = theme_.muted();
    muted.bg = bg;
    Style accent = theme_.accent();
    accent.bg = bg;
    Style value = theme_.overlay();
    value.bold = true;

    constexpr int kLabelX = 4;
    constexpr int kArrowLeftX = 17;
    constexpr int kValueX = 20;
    constexpr int kArrowRightX = 41;

    // One selectable row: "› LABEL      ◂  value  ▸"
    const auto drawRow = [&](int row, core::Setup::Field field, std::string_view label, std::size_t index,
                             std::size_t count) {
        const bool focused = setup.focus == field;
        const int ry = box.y + row;
        if (focused) {
            back_.text(box.x + 2, ry, "›", accent);
        }
        back_.text(box.x + kLabelX, ry, label, focused ? accent : normal);
        back_.text(box.x + kArrowLeftX, ry, "◂", index > 0 && focused ? accent : muted);
        back_.text(box.x + kArrowRightX, ry, "▸", index + 1 < count && focused ? accent : muted);
    };

    drawRow(2, core::Setup::Field::GameType, "MODE", setup.gameType, core::kGameTypes.size());
    back_.text(box.x + kValueX, box.y + 2, type.name, value);
    drawCentred(back_, box.x, box.w, box.y + 3, type.summary, muted);

    // Board size: "Classic  10 × 20"
    drawRow(5, core::Setup::Field::BoardSize, "BOARD", setup.boardSize, core::kBoardSizes.size());
    NumberBuffer a{};
    NumberBuffer b{};
    int x = back_.text(box.x + kValueX, box.y + 5, size.name, value);
    x = back_.text(x + 2, box.y + 5, formatInt(a, static_cast<std::uint64_t>(size.width)), muted);
    x = back_.text(x, box.y + 5, " × ", muted);
    back_.text(x, box.y + 5, formatInt(b, static_cast<std::uint64_t>(size.height)), muted);

    drawRow(6, core::Setup::Field::Difficulty, "DIFFICULTY", setup.difficulty, core::kDifficulties.size());
    back_.text(box.x + kValueX, box.y + 6, difficulty.name, value);
    drawCentred(back_, box.x, box.w, box.y + 7, difficulty.summary, muted);

    // Status line: a size warning takes priority over the best score.
    const Layout preview =
        computeLayout(terminal_, BoardShape{size.width, size.height}, difficulty.previewCount, halfBlocks_);
    const int sy = box.y + 9;
    if (!preview.fits) {
        Style warn = theme_.warning();
        warn.bg = bg;
        NumberBuffer c{};
        NumberBuffer d{};
        x = back_.text(box.x + kLabelX, sy, "needs a ", warn);
        x = back_.text(x, sy, formatInt(c, static_cast<std::uint64_t>(preview.minimum.columns)), warn);
        x = back_.text(x, sy, " × ", warn);
        x = back_.text(x, sy, formatInt(d, static_cast<std::uint64_t>(preview.minimum.rows)), warn);
        back_.text(x, sy, " terminal", warn);
    } else if (hud_.best) {
        NumberBuffer c{};
        const std::string_view best = formatThousands(c, *hud_.best);
        const int w = 6 + displayWidth(best);
        x = back_.text(box.x + (box.w - w) / 2, sy, "BEST  ", muted);
        back_.text(x, sy, best, accent);
    } else {
        drawCentred(back_, box.x, box.w, sy, "no best score yet", muted);
    }

    // Footer: key hints.
    int fx = box.x + 3;
    const int fy = box.y + 11;
    fx = back_.text(fx, fy, "↑↓", accent);
    fx = back_.text(fx + 1, fy, "choose", muted);
    fx = back_.text(fx + 2, fy, "←→", accent);
    fx = back_.text(fx + 1, fy, "change", muted);
    fx = back_.text(fx + 2, fy, "ENTER", accent);
    fx = back_.text(fx + 1, fy, "start", muted);
    fx = back_.text(fx + 2, fy, "Q", accent);
    back_.text(fx + 1, fy, "quit", muted);
}

void Renderer::drawPlayfield(const GameState& state) {
    const bool paused = state.mode == GameMode::Paused;
    drawBoard(state, paused);
    drawHold(state);
    drawStats(state);
    drawKeys(state);
    drawNext(state);
    drawMessage(state);
}

// ---------------------------------------------------------------------------
// Board
// ---------------------------------------------------------------------------

void Renderer::drawBoardCell(int col, int row, const CellGlyph& glyph) {
    const int cw = layout_.cellPixels;
    const int ch = layout_.cellPixels / 2;
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
    // Title: what's being played ("2-MINUTE · HARD"); the demo is labelled.
    boardTitle_.clear();
    if (hud_.attract) {
        boardTitle_ = "DEMO";
    } else {
        for (const char c : core::kGameTypes[state.setup.gameType].name) {
            boardTitle_ += static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
        }
        boardTitle_ += " · ";
        for (const char c : core::kDifficulties[state.setup.difficulty].name) {
            boardTitle_ += static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
        }
    }

    Style frame = theme_.boardFrame();
    Style title = theme_.accent();
    const core::Feedback& f = state.feedback;
    const bool levelUpFlash = f.levelUp && f.age < std::chrono::milliseconds{900} &&
                              (std::chrono::duration_cast<std::chrono::milliseconds>(f.age).count() / 150) % 2 == 0;
    if (levelUpFlash) {
        frame = theme_.warning();
        title = theme_.warning();
    } else if (state.mode == GameMode::Playing && inDanger(state)) {
        frame = theme_.danger();
        title = theme_.danger();
    }
    drawPanelCentredTitle(back_, layout_.board, boardTitle_, PanelStyle{BorderStyle::Rounded, frame, title, false, {}});
    if (layout_.halfBlocks) {
        drawBoardPixels(state, hideContents);
    } else {
        drawBoardGlyphs(state, hideContents);
    }
}

void Renderer::drawBoardPixels(const GameState& state, bool hideContents) {
    const int k = layout_.cellPixels;
    const int columns = state.board.width();
    const int rows = state.board.visibleHeight();
    const float centreColumn = static_cast<float>(columns - 1) / 2.0F;
    const bool gameOver = state.mode == GameMode::GameOver;
    canvas_.reset(columns * k, rows * k);


    for (int row = 0; row < rows; ++row) {
        const int y = row + Board::kHiddenRows;
        const bool clearing =
            std::find(state.clearingRows.begin(), state.clearingRows.end(), y) != state.clearingRows.end();
        for (int col = 0; col < columns; ++col) {
            const core::CellType cell = state.board.at({col, y});
            const int px = col * k;
            const int py = row * k;
            if (hideContents || cell == core::CellType::Empty) {
                continue;  // left transparent; gets a grid dot below
            }
            if (clearing) {
                const float centreDistance = std::abs(static_cast<float>(col) - centreColumn);
                if (centreDistance < state.clearProgress * (centreColumn + 1.5F) - 1.0F) {
                    continue;  // wiped
                }
                canvas_.fill(px, py, k, k, theme_.clearingColor(pieceOf(cell), state.clearProgress));
                continue;
            }
            Color c = theme_.lockedColor(pieceOf(cell));
            if (gameOver) {
                c = c.blended(theme_.wellBackground(), 0.6F);
            }
            canvas_.fill(px, py, k, k, c);
        }
    }

    if (!hideContents && state.active) {
        const auto visible = [rows](core::Point p) {
            const int row = p.y - Board::kHiddenRows;
            return row >= 0 && row < rows;
        };

        if (const auto ghost = state.ghost(); ghost && *ghost != *state.active) {
            const Color g = theme_.ghostFillColor(ghost->type);
            for (const core::Point p : game::cellsOf(*ghost)) {
                if (visible(p)) {
                    canvas_.fill(p.x * k, (p.y - Board::kHiddenRows) * k, k, k, g);
                }
            }
        }

        const Color active = theme_.pieceColor(state.active->type);
        for (const core::Point p : game::cellsOf(*state.active)) {
            if (visible(p)) {
                canvas_.fill(p.x * k, (p.y - Board::kHiddenRows) * k, k, k, active);
            }
        }
    }

    canvas_.blit(back_, layout_.well.x, layout_.well.y, theme_.wellBackground());

    // Grid dots: the same '·' glyph as character rendering, in the last
    // column of the first terminal row that lies fully inside each empty cell.
    const Style dotStyle{theme_.gridDotColor(), theme_.wellBackground(), false, false};
    for (int row = 0; row < rows; ++row) {
        const int textRow = (row * k + 1) / 2;  // first full terminal row of the cell
        for (int col = 0; col < columns; ++col) {
            const int px = col * k + k - 1;
            if (canvas_.at(px, 2 * textRow).isDefault() && canvas_.at(px, 2 * textRow + 1).isDefault()) {
                back_.set(layout_.well.x + px, layout_.well.y + textRow, U'·', dotStyle);
            }
        }
    }
}

void Renderer::drawBoardGlyphs(const GameState& state, bool hideContents) {
    const CellGlyph empty = theme_.emptyCell();
    const bool gameOver = state.mode == GameMode::GameOver;

    const int columns = state.board.width();
    const int rows = state.board.visibleHeight();
    const float centreColumn = static_cast<float>(columns - 1) / 2.0F;

    for (int row = 0; row < rows; ++row) {
        const int y = row + Board::kHiddenRows;
        const bool clearing =
            std::find(state.clearingRows.begin(), state.clearingRows.end(), y) != state.clearingRows.end();
        for (int col = 0; col < columns; ++col) {
            const core::CellType cell = state.board.at({col, y});
            if (hideContents || cell == core::CellType::Empty) {
                drawBoardCell(col, row, empty);
                continue;
            }
            if (clearing) {
                // Flash, then wipe outwards from the centre of the row.
                const float centreDistance = std::abs(static_cast<float>(col) - centreColumn);
                const bool wiped = centreDistance < state.clearProgress * (centreColumn + 1.5F) - 1.0F;
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

    const auto drawPiece = [this, rows](const game::Piece& piece, const CellGlyph& glyph) {
        for (const core::Point p : game::cellsOf(piece)) {
            const int row = p.y - Board::kHiddenRows;
            if (row >= 0 && row < rows) {
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
    if (layout_.halfBlocks) {
        // Pixel rendering at the layout's preview size, centred in a
        // `width` x (2 cells) area.
        const int k = layout_.previewPixels;
        const int cellsW = maxX - minX + 1;
        const int cellsH = maxY - minY + 1;
        canvas_.reset(cellsW * k, 2 * k);
        const int offsetY = (2 - cellsH) * k / 2;
        const Color c = dimmed ? theme_.ghostColor(type) : theme_.pieceColor(type);
        for (const core::Point p : shape) {
            canvas_.fill((p.x - minX) * k, offsetY + (p.y - minY) * k, k, k, c);
        }
        canvas_.blit(back_, x + (width - cellsW * k) / 2, y, Color{});
        return;
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
    if (!state.rules.holdEnabled) {
        drawCentred(back_, r.x, r.w, r.y + 2, "off", theme_.muted());
        return;
    }
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
    if (hud_.best) {
        NumberBuffer bestBuf{};
        drawLabelValue(back_, x, r.y + 2, w, "BEST", formatThousands(bestBuf, *hud_.best), theme_.muted(),
                       theme_.muted());
    }

    drawLabelValue(back_, x, r.y + 3, w, "LEVEL", formatInt(buf, static_cast<std::uint64_t>(state.stats.level)),
                   theme_.label(), theme_.value());
    drawLabelValue(back_, x, r.y + 4, w, "LINES", formatInt(buf, static_cast<std::uint64_t>(state.stats.lines)),
                   theme_.label(), theme_.value());
    // Elapsed time, or time left in a timed mode (amber for the last 10 s).
    NumberBuffer clock{};
    if (state.rules.timeLimit) {
        const core::Duration left = state.timeLeft();
        const bool urgent = left < std::chrono::seconds{10};
        drawLabelValue(back_, x, r.y + 5, w, "LEFT", formatClock(clock, left, true), theme_.label(),
                       urgent ? theme_.warning() : theme_.value());
    } else {
        drawLabelValue(back_, x, r.y + 5, w, "TIME", formatClock(clock, state.stats.playTime, false), theme_.label(),
                       theme_.value());
    }

    // Progress towards the next level.
    const int filled = (state.stats.lines % kLinesPerLevel) * w / kLinesPerLevel;
    Style on = theme_.accent();
    on.bold = false;
    for (int i = 0; i < w; ++i) {
        back_.set(x + i, r.y + 6, i < filled ? U'━' : U'─', i < filled ? on : theme_.muted());
    }
}

void Renderer::drawKeys(const GameState& state) {
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
            if (b.what == "hold" && !state.rules.holdEnabled) {
                continue;
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
    if (layout_.previewCount == 0) {
        drawCentred(back_, r.x, r.w, r.y + 2, "off", theme_.muted());
        return;
    }
    for (int i = 0; i < layout_.previewCount; ++i) {
        const int slotRows = (2 * layout_.previewPixels + 1) / 2 + 1;
        drawMiniPiece(state.preview[static_cast<std::size_t>(i)], r.x + 1, r.y + 2 + i * slotRows, r.w - 2, false);
    }
}

void Renderer::drawMessage(const GameState& state) {
    const core::Feedback& f = state.feedback;
    const Rect& r = layout_.message;
    if (f.id == 0 || f.age >= core::kFeedbackDuration || r.h < 2 || state.mode != GameMode::Playing) {
        return;
    }
    // Fade out over the last 40% of the display time.
    const float t = static_cast<float>(f.age.count()) / static_cast<float>(core::kFeedbackDuration.count());
    const float fade = std::clamp((t - 0.6F) / 0.4F, 0.0F, 1.0F);
    const auto faded = [&](Style s) {
        if (!theme_.isMonochrome()) {
            s.fg = s.fg.blended(theme_.panelBackground(), fade);
        }
        return s;
    };
    const Style headline = faded(f.lines == 4 || f.spin != game::SpinKind::None ? theme_.warning() : theme_.accent());
    const Style detail = faded(theme_.value());
    const Style bonus = faded(theme_.accent());

    int y = r.y + 1;
    const int last = r.bottom() - 1;
    const auto line = [&](std::string_view text, const Style& style) {
        if (y <= last && !text.empty()) {
            drawCentred(back_, r.x, r.w, y++, text, style);
        }
    };

    // Most important first: small terminals only have room for a few lines.
    line(clearName(f), headline);
    if (f.points > 0) {
        NumberBuffer n{};
        std::array<char, 40> text{};
        line(formatInto(text, "+{}", formatThousands(n, f.points)), detail);
    }
    if (f.perfectClear) {
        line("PERFECT CLEAR!", headline);
    }
    if (f.backToBack || f.combo > 0) {
        std::array<char, 40> text{};
        if (f.backToBack && f.combo > 0) {
            line(formatInto(text, "B2B · COMBO ×{}", f.combo), bonus);
        } else if (f.backToBack) {
            line("BACK-TO-BACK", bonus);
        } else {
            line(formatInto(text, "COMBO ×{}", f.combo), bonus);
        }
    }
    if (f.levelUp) {
        std::array<char, 32> text{};
        line(formatInto(text, "LEVEL {}", state.stats.level), headline);
    }
}

Rect Renderer::drawOverlayBox(int innerHeight, std::string_view title) {
    // As wide as the well, but at least 22 columns so the text fits even on
    // the small board (the box then overlaps the board frame, like a dialog).
    const Rect& well = layout_.well;
    const int w = std::clamp(well.w, 22, 28);
    const int h = innerHeight + 2;
    const Rect box{well.x + (well.w - w) / 2, well.y + (well.h - h) / 2, w, h};
    drawPanelCentredTitle(back_, box, title,
                          PanelStyle{BorderStyle::Double, theme_.overlayFrame(), theme_.overlayFrame(), true, theme_.overlay()});
    return box.inset(1);
}

void Renderer::drawKeyHint(int x, int y, std::string_view key, std::string_view action) {
    Style keyStyle = theme_.accent();
    keyStyle.bg = theme_.overlay().bg;
    back_.text(x, y, key, keyStyle);
    back_.text(x + 3, y, action, theme_.overlay());
}

void Renderer::drawCountdownOverlay(const GameState& state) {
    // A big block digit over the board (the stack stays visible), counting
    // down to the resume.
    const auto seconds = std::clamp<long long>(std::chrono::ceil<std::chrono::seconds>(state.countdown).count(), 1, 9);
    const Rect in = drawOverlayBox(8, "");
    const auto& digit = kDigits[static_cast<std::size_t>(seconds)];
    const int x0 = in.x + (in.w - 6) / 2;
    Style ink = theme_.overlayFrame();
    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 3; ++col) {
            if (digit[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] == '#') {
                back_.set(x0 + col * 2, in.y + 1 + row, U'█', ink);
                back_.set(x0 + col * 2 + 1, in.y + 1 + row, U'█', ink);
            }
        }
    }
    Style hint = theme_.muted();
    hint.bg = theme_.overlay().bg;
    drawCentred(back_, in.x, in.w, in.y + 7, "get ready", hint);
}

void Renderer::drawPauseOverlay() {
    const Rect in = drawOverlayBox(8, "");
    drawCentred(back_, in.x, in.w, in.y + 1, "PAUSED", theme_.overlayFrame());

    const int x = in.x + (in.w - 12) / 2;
    drawKeyHint(x, in.y + 3, "P", "resume");
    drawKeyHint(x, in.y + 4, "R", "restart");
    drawKeyHint(x, in.y + 5, "M", "menu");
    drawKeyHint(x, in.y + 6, "Q", "quit");
}

void Renderer::drawGameOverOverlay(const GameState& state) {
    const core::Stats& st = state.stats;
    const Rect in = drawOverlayBox(16, "");
    Style title = theme_.overlayFrame();
    title.fg = theme_.isMonochrome() ? Color{} : theme_.danger().fg;
    drawCentred(back_, in.x, in.w, in.y + 1, state.endReason == core::EndReason::TimeUp ? "TIME UP" : "GAME OVER",
                title);
    if (hud_.newBest) {
        Style best = theme_.warning();
        best.bg = theme_.overlay().bg;
        drawCentred(back_, in.x, in.w, in.y + 2, "NEW BEST!", best);
    }

    Style label = theme_.overlay();
    Style value = theme_.overlay();
    value.bold = true;
    Style minor = theme_.muted();
    minor.bg = theme_.overlay().bg;
    const int x = in.x + 2;
    const int w = in.w - 4;
    NumberBuffer buf{};
    drawLabelValue(back_, x, in.y + 3, w, "Score", formatThousands(buf, st.score), label, value);
    drawLabelValue(back_, x, in.y + 4, w, "Lines", formatInt(buf, static_cast<std::uint64_t>(st.lines)), label, value);
    drawLabelValue(back_, x, in.y + 5, w, "Level", formatInt(buf, static_cast<std::uint64_t>(st.level)), label, value);
    drawLabelValue(back_, x, in.y + 6, w, "Time", formatClock(buf, st.playTime, false), label, value);

    // Pieces per second.
    const double secs = std::chrono::duration<double>(st.playTime).count();
    std::array<char, 16> pps{};
    drawLabelValue(back_, x, in.y + 8, w, "Pieces/s",
                   formatInto(pps, "{:.2f}", secs > 0.5 ? static_cast<double>(st.pieces) / secs : 0.0), minor, label);
    drawLabelValue(back_, x, in.y + 9, w, "Quads", formatInt(buf, static_cast<std::uint64_t>(st.quads)), minor, label);
    drawLabelValue(back_, x, in.y + 10, w, "Spins", formatInt(buf, static_cast<std::uint64_t>(st.spins)), minor, label);
    drawLabelValue(back_, x, in.y + 11, w, "Best combo", formatInt(buf, static_cast<std::uint64_t>(st.maxCombo)),
                   minor, label);

    const int kx = in.x + (in.w - 12) / 2;
    drawKeyHint(kx, in.y + 13, "R", "restart");
    drawKeyHint(kx, in.y + 14, "M", "menu");
    drawKeyHint(kx, in.y + 15, "Q", "quit");
}

}  // namespace tetromino::tui

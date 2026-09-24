// Screen buffer, diff encoder, layout and renderer - all without a terminal.

#include <string>

#include "TestFramework.hpp"
#include "core/GameState.hpp"
#include "game/Piece.hpp"
#include "tui/Layout.hpp"
#include "tui/Renderer.hpp"
#include "tui/ScreenBuffer.hpp"

using namespace tetromino;
using tui::Cell;
using tui::ColorMode;
using tui::ScreenBuffer;
using tui::Style;

namespace {

std::string rowText(const ScreenBuffer& buf, int y) {
    std::string s;
    for (int x = 0; x < buf.width(); ++x) {
        const char32_t ch = buf.at(x, y).ch;
        s += ch < 0x80 ? static_cast<char>(ch) : '#';
    }
    return s;
}

bool contains(const std::string& haystack, std::string_view needle) {
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

TEST(text_and_clipping) {
    ScreenBuffer buf{10, 2};
    const int end = buf.text(7, 0, "hello", Style{});
    CHECK_EQ(end, 12);
    CHECK_EQ(rowText(buf, 0), std::string{"       hel"});
    buf.set(-1, 0, U'x', Style{});  // ignored
    buf.set(0, 5, U'x', Style{});   // ignored
    CHECK(buf.at(-1, 0) == Cell{});
}

TEST(utf8_decoding) {
    const std::string_view s = "a│█€😀";
    std::size_t i = 0;
    CHECK(tui::nextCodepoint(s, i) == U'a');
    CHECK(tui::nextCodepoint(s, i) == U'│');
    CHECK(tui::nextCodepoint(s, i) == U'█');
    CHECK(tui::nextCodepoint(s, i) == U'€');
    CHECK(tui::nextCodepoint(s, i) == U'😀');
    CHECK_EQ(i, s.size());
    CHECK_EQ(tui::displayWidth("╭─ HOLD ─╮"), 10);
}

TEST(identical_frames_produce_no_output) {
    ScreenBuffer a{20, 5};
    a.text(2, 2, "same", Style{});
    ScreenBuffer b = a;
    std::string out;
    tui::encodeDiff(a, b, ColorMode::TrueColor, false, out);
    CHECK(out.empty());
}

TEST(diff_only_emits_changed_cells) {
    ScreenBuffer front{40, 10};
    front.text(0, 0, "abcdefghij", Style{});
    ScreenBuffer back = front;
    back.set(5, 0, U'X', Style{});
    std::string out;
    tui::encodeDiff(front, back, ColorMode::TrueColor, false, out);
    // One cursor move to row 1 col 6, one style, one glyph, one reset.
    CHECK(contains(out, "\x1b[1;6H"));
    CHECK(contains(out, "X"));
    CHECK(!contains(out, "abc"));
    CHECK(out.size() < 30);
}

TEST(diff_skips_cursor_moves_for_adjacent_cells) {
    ScreenBuffer front{40, 3};
    ScreenBuffer back = front;
    back.text(3, 1, "run", Style{});
    std::string out;
    tui::encodeDiff(front, back, ColorMode::TrueColor, false, out);
    CHECK(contains(out, "run"));  // written contiguously, no moves between
}

TEST(resize_forces_full_repaint) {
    ScreenBuffer front{10, 3};
    ScreenBuffer back{12, 3};
    back.text(0, 0, "x", Style{});
    std::string out;
    tui::encodeDiff(front, back, ColorMode::TrueColor, false, out);
    CHECK(contains(out, "\x1b[2J"));
}

TEST(color_modes_emit_expected_sgr) {
    ScreenBuffer front{4, 1};
    ScreenBuffer back{4, 1};
    back.set(0, 0, U'#', Style{tui::Color::rgb(255, 0, 0), {}, false, false});
    std::string tc;
    std::string c256;
    std::string mono;
    tui::encodeDiff(front, back, ColorMode::TrueColor, false, tc);
    tui::encodeDiff(front, back, ColorMode::Ansi256, false, c256);
    tui::encodeDiff(front, back, ColorMode::Monochrome, false, mono);
    CHECK(contains(tc, "38;2;255;0;0"));
    CHECK(contains(c256, "38;5;196"));
    CHECK(!contains(mono, "38;"));
}

TEST(layout_minimum_and_scaling) {
    const tui::Layout small = tui::computeLayout({40, 15});
    CHECK(!small.fits);
    const tui::Layout normal = tui::computeLayout(small.minimum);
    CHECK(normal.fits);
    CHECK_EQ(normal.cellWidth, 2);
    CHECK_EQ(normal.well.w, 20);
    CHECK_EQ(normal.well.h, 20);
    const tui::Layout big = tui::computeLayout({200, 60});
    CHECK(big.fits);
    CHECK_EQ(big.cellWidth, 4);
    CHECK_EQ(big.cellHeight, 2);
    // Panels never overlap the board.
    CHECK(big.hold.right() <= big.board.x);
    CHECK(big.next.x >= big.board.right());
}

TEST(renderer_draws_board_pieces_and_stats) {
    tui::Renderer r{tui::Theme::standard(), ColorMode::TrueColor};
    r.resize({80, 24});
    core::GameState s;
    s.mode = core::GameMode::Playing;
    s.active = game::spawnPiece(core::PieceType::O, game::Board::kDefaultWidth);
    s.stats.score = 1234567;
    r.render(s);
    const ScreenBuffer& f = r.frame();
    const tui::Layout& l = r.layout();
    // The O piece: columns 4-5 -> terminal columns well.x + 8 .. + 11, first row.
    CHECK(f.at(l.well.x + 8, l.well.y).ch == U'█');
    CHECK(f.at(l.well.x + 11, l.well.y).ch == U'█');
    // Its ghost on the bottom row.
    CHECK(f.at(l.well.x + 8, l.well.bottom() - 1).ch == U'░');
    bool foundScore = false;
    for (int y = 0; y < f.height(); ++y) {
        foundScore = foundScore || contains(rowText(f, y), "1,234,567");
    }
    CHECK(foundScore);
}

TEST(renderer_shows_too_small_message) {
    tui::Renderer r{tui::Theme::standard(), ColorMode::TrueColor};
    r.resize({40, 12});
    r.render(core::GameState{});
    bool found = false;
    for (int y = 0; y < 12; ++y) {
        found = found || contains(rowText(r.frame(), y), "Terminal too small.");
    }
    CHECK(found);
}

TEST(renderer_animates_line_clear) {
    tui::Renderer r{tui::Theme::standard(), ColorMode::TrueColor};
    r.resize({80, 24});
    core::GameState s;
    s.mode = core::GameMode::Playing;
    const int y = game::Board::kDefaultHeight - 1;
    for (int x = 0; x < game::Board::kDefaultWidth; ++x) {
        s.board.set({x, y}, core::CellType::T);
    }
    s.clearingRows = {y};
    const tui::Layout& l = r.layoutFor(s);
    const int row = l.well.bottom() - 1;

    s.clearProgress = 0.2F;  // flashing: all cells still drawn, brighter than normal
    r.render(s);
    const tui::Color flash = r.frame().at(l.well.x, row).style.fg;
    CHECK(r.frame().at(l.well.x + 9, row).ch == U'█');

    s.clearProgress = 0.9F;  // wiping: centre cells gone
    r.render(s);
    CHECK(r.frame().at(l.well.x + 9, row).ch != U'█');

    s.clearingRows.clear();  // normal locked cell for comparison
    r.render(s);
    CHECK(!(r.frame().at(l.well.x, row).style.fg == flash));
}

TEST(layout_depends_on_board_size) {
    const tui::Layout classic = tui::computeLayout({200, 60}, {10, 20}, 5);
    const tui::Layout wide = tui::computeLayout({200, 60}, {14, 20}, 5);
    CHECK(wide.well.w > classic.well.w);
    CHECK(tui::computeLayout({58, 22}, {10, 20}, 5).fits);
    CHECK(!tui::computeLayout({58, 22}, {14, 20}, 5).fits);
    CHECK_EQ(tui::computeLayout({58, 22}, {14, 20}, 5).minimum.columns, 66);
    CHECK_EQ(tui::computeLayout({80, 24}, {10, 20}, 0).previewCount, 0);
    CHECK_EQ(tui::computeLayout({80, 24}, {10, 20}, 1).previewCount, 1);
}

TEST(renderer_hides_disabled_features) {
    tui::Renderer r{tui::Theme::standard(), ColorMode::TrueColor};
    r.resize({80, 24});
    core::GameState s;
    s.mode = core::GameMode::Playing;
    s.rules = core::Rules{false, 0, false};
    s.active = game::spawnPiece(core::PieceType::O, 10);
    r.render(s);
    const tui::Layout& l = r.layout();
    const std::string holdRow = rowText(r.frame(), l.hold.y + 2);
    const std::string nextRow = rowText(r.frame(), l.next.y + 2);
    CHECK(contains(holdRow, "off"));
    CHECK(contains(nextRow, "off"));
    // No ghost on the bottom row.
    CHECK(r.frame().at(l.well.x + 8, l.well.bottom() - 1).ch != U'░');
}

TEST(start_screen_shows_setup_menu) {
    tui::Renderer r{tui::Theme::standard(), ColorMode::TrueColor};
    r.resize({80, 24});
    core::GameState s;  // start screen, Classic / Normal
    r.render(s);
    std::string all;
    for (int y = 0; y < 24; ++y) {
        all += rowText(r.frame(), y) + "\n";
    }
    CHECK(contains(all, "BOARD"));
    CHECK(contains(all, "Classic"));
    CHECK(contains(all, "DIFFICULTY"));
    CHECK(contains(all, "Normal"));
    CHECK(!contains(all, "needs a"));  // classic fits 80x24

    s.setup.boardSize = *core::findBoardSize("tall");  // needs 26 rows
    r.render(s);
    all.clear();
    for (int y = 0; y < 24; ++y) {
        all += rowText(r.frame(), y) + "\n";
    }
    CHECK(contains(all, "needs a"));
}

// Screen buffer, diff encoder, layout and renderer - all without a terminal.

#include <string>

#include "TestFramework.hpp"
#include "core/GameState.hpp"
#include "core/Presets.hpp"
#include "game/Piece.hpp"
#include "tui/Layout.hpp"
#include "tui/PixelCanvas.hpp"
#include "tui/Renderer.hpp"
#include "tui/Terminal.hpp"
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

// Colour of the top pixel of a cell drawn by PixelCanvas.
tui::Color topPixel(const Cell& c) {
    if (c.ch == U'▀') {
        return c.style.fg;
    }
    return c.style.bg;  // ' ' (both halves = bg) or '▄' (top = bg)
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
    CHECK_EQ(normal.cellPixels, 2);
    CHECK_EQ(normal.well.w, 20);
    CHECK_EQ(normal.well.h, 20);
    const tui::Layout big = tui::computeLayout({200, 60});
    CHECK(big.fits);
    CHECK(big.cellPixels > 2);
    CHECK_EQ(big.well.w, 10 * big.cellPixels);
    CHECK_EQ(big.well.h, (20 * big.cellPixels + 1) / 2);
    CHECK(big.board.bottom() <= 60);
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
    const tui::Theme theme = tui::Theme::standard();
    CHECK_EQ(l.cellPixels, 2);
    // The O piece: columns 4-5 -> terminal columns well.x + 8 .. + 11, first row.
    CHECK(topPixel(f.at(l.well.x + 8, l.well.y)) == theme.pieceColor(core::PieceType::O));
    CHECK(topPixel(f.at(l.well.x + 11, l.well.y)) == theme.pieceColor(core::PieceType::O));
    // Its ghost on the bottom row: tinted, not plain well background.
    const tui::Color ghost = topPixel(f.at(l.well.x + 8, l.well.bottom() - 1));
    CHECK(!(ghost == theme.wellBackground()));
    CHECK(!(ghost == theme.pieceColor(core::PieceType::O)));
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

    const tui::Theme theme = tui::Theme::standard();

    s.clearProgress = 0.2F;  // flashing: all cells still drawn, brighter than normal
    r.render(s);
    const tui::Color flash = topPixel(r.frame().at(l.well.x, row));
    CHECK(flash == theme.clearingColor(core::PieceType::T, 0.2F));
    CHECK(topPixel(r.frame().at(l.well.x + 8, row)) == flash);

    s.clearProgress = 0.9F;  // wiping: centre cells gone
    r.render(s);
    const tui::Color centre = topPixel(r.frame().at(l.well.x + 8, row));
    CHECK(centre == theme.wellBackground());

    s.clearingRows.clear();  // normal locked cell for comparison
    r.render(s);
    CHECK(topPixel(r.frame().at(l.well.x, row)) == theme.lockedColor(core::PieceType::T));
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
    s.rules = core::Rules{false, 0, false, std::nullopt};
    s.active = game::spawnPiece(core::PieceType::O, 10);
    r.render(s);
    const tui::Layout& l = r.layout();
    const std::string holdRow = rowText(r.frame(), l.hold.y + 2);
    const std::string nextRow = rowText(r.frame(), l.next.y + 2);
    CHECK(contains(holdRow, "off"));
    CHECK(contains(nextRow, "off"));
    // No ghost on the bottom row.
    CHECK(topPixel(r.frame().at(l.well.x + 8, l.well.bottom() - 1)) == tui::Theme::standard().wellBackground());
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

TEST(half_blocks_allow_in_between_sizes) {
    // Regression: a 218x49 terminal (full-screen window) with the Tall
    // board. 2x needs 50 rows; before half-block rendering this fell all the
    // way back to 1x. Now it gets 1.5x.
    const tui::Layout tall = tui::computeLayout({218, 49}, {10, 24}, 3, true);
    CHECK(tall.fits);
    CHECK_EQ(tall.cellPixels, 3);
    const tui::Layout tallMono = tui::computeLayout({218, 49}, {10, 24}, 3, false);
    CHECK_EQ(tallMono.cellPixels, 2);  // characters: whole multiples only
    // Every board uses the largest size that fits.
    for (const auto& size : core::kBoardSizes) {
        const tui::Layout l = tui::computeLayout({218, 49}, {size.width, size.height}, 3, true);
        CHECK(l.fits);
        CHECK(l.board.bottom() <= 49);
        CHECK(l.cellPixels >= 3);
    }
}

TEST(easy_keeps_all_five_previews_by_shrinking_side_pieces) {
    const tui::Layout l = tui::computeLayout({218, 49}, {10, 20}, 5, true);
    CHECK_EQ(l.previewCount, 5);
    CHECK(l.next.bottom() <= 49);
}

TEST(pixel_canvas_blits_half_blocks) {
    tui::PixelCanvas canvas;
    canvas.reset(2, 3);
    const tui::Color red = tui::Color::rgb(255, 0, 0);
    const tui::Color blue = tui::Color::rgb(0, 0, 255);
    canvas.set(0, 0, red);
    canvas.set(0, 1, blue);  // column 0: red over blue -> one '▀'
    canvas.set(1, 0, red);
    canvas.set(1, 1, red);   // column 1: red over red -> space with red bg
    canvas.set(0, 2, blue);  // row 1 of the terminal: only the top half used
    ScreenBuffer buf{4, 4};
    canvas.blit(buf, 1, 1, tui::Color{});
    CHECK(buf.at(1, 1).ch == U'▀');
    CHECK(buf.at(1, 1).style.fg == red);
    CHECK(buf.at(1, 1).style.bg == blue);
    CHECK(buf.at(2, 1).ch == U' ');
    CHECK(buf.at(2, 1).style.bg == red);
    CHECK(buf.at(1, 2).ch == U'▀');
    CHECK(buf.at(1, 2).style.fg == blue);
    CHECK(buf.at(2, 2) == Cell{});  // fully transparent: untouched
}

// --- Feedback, countdown, game over, danger, menu ---------------------------

namespace {

std::string screenText(const ScreenBuffer& f) {
    std::string all;
    for (int y = 0; y < f.height(); ++y) {
        // Keep non-ASCII glyphs recognisable enough for "×".
        for (int x = 0; x < f.width(); ++x) {
            const char32_t ch = f.at(x, y).ch;
            all += ch < 0x80 ? static_cast<char>(ch) : (ch == U'×' ? 'x' : '#');
        }
        all += '\n';
    }
    return all;
}

core::GameState playingState() {
    core::GameState s;
    s.mode = core::GameMode::Playing;
    s.active = game::spawnPiece(core::PieceType::T, 10);
    return s;
}

}  // namespace

TEST(feedback_shows_clear_bonus_and_points) {
    tui::Renderer r{tui::Theme::standard(), ColorMode::TrueColor};
    r.resize({80, 24});
    core::GameState s = playingState();
    s.feedback = core::Feedback{1, 4, game::SpinKind::None, 2, true, false, 2400, false, core::Duration::zero()};
    r.render(s);
    const std::string all = screenText(r.frame());
    CHECK(contains(all, "QUAD!"));
    CHECK(contains(all, "+2,400"));
    CHECK(contains(all, "B2B # COMBO x2"));  // '·' shows as '#'

    s.feedback.age = core::kFeedbackDuration;  // expired
    r.render(s);
    CHECK(!contains(screenText(r.frame()), "QUAD!"));
}

TEST(feedback_names_spins) {
    tui::Renderer r{tui::Theme::standard(), ColorMode::TrueColor};
    r.resize({120, 40});  // room for every line
    core::GameState s = playingState();
    s.feedback = core::Feedback{1, 2, game::SpinKind::Full, 0, false, true, 1200, true, core::Duration::zero()};
    s.stats.level = 3;
    r.render(s);
    const std::string all = screenText(r.frame());
    CHECK(contains(all, "SPIN DOUBLE"));
    CHECK(contains(all, "PERFECT CLEAR!"));
    CHECK(contains(all, "LEVEL 3"));
}

TEST(countdown_draws_a_big_digit) {
    tui::Renderer r{tui::Theme::standard(), ColorMode::TrueColor};
    r.resize({80, 24});
    core::GameState s = playingState();
    s.mode = core::GameMode::Countdown;
    s.countdown = std::chrono::milliseconds{2300};  // shows "3"
    r.render(s);
    CHECK(contains(screenText(r.frame()), "get ready"));
}

TEST(game_over_shows_reason_best_and_stats) {
    tui::Renderer r{tui::Theme::standard(), ColorMode::TrueColor};
    r.resize({80, 24});
    core::GameState s = playingState();
    s.mode = core::GameMode::GameOver;
    s.endReason = core::EndReason::TimeUp;
    s.stats.score = 12450;
    s.stats.pieces = 120;
    s.stats.playTime = std::chrono::seconds{120};
    r.render(s, tui::HudInfo{12450, true, false});
    const std::string all = screenText(r.frame());
    CHECK(contains(all, "TIME UP"));
    CHECK(contains(all, "NEW BEST!"));
    CHECK(contains(all, "12,450"));
    CHECK(contains(all, "2:00"));
    CHECK(contains(all, "1.00"));  // pieces per second

    s.endReason = core::EndReason::ToppedOut;
    r.render(s, tui::HudInfo{20000, false, false});
    const std::string topped = screenText(r.frame());
    CHECK(contains(topped, "GAME OVER"));
    CHECK(!contains(topped, "NEW BEST!"));
}

TEST(board_title_names_mode_and_difficulty) {
    tui::Renderer r{tui::Theme::standard(), ColorMode::TrueColor};
    r.resize({80, 24});
    core::GameState s = playingState();
    s.setup.gameType = 1;
    s.setup.difficulty = 2;
    r.render(s);
    CHECK(contains(screenText(r.frame()), "2-MINUTE # HARD"));  // '·' shows as '#'
    r.render(s, tui::HudInfo{std::nullopt, false, true});
    CHECK(contains(screenText(r.frame()), " DEMO "));
}

TEST(timed_mode_shows_time_left) {
    tui::Renderer r{tui::Theme::standard(), ColorMode::TrueColor};
    r.resize({80, 24});
    core::GameState s = playingState();
    s.rules.timeLimit = std::chrono::seconds{120};
    s.stats.playTime = std::chrono::milliseconds{115500};
    r.render(s);
    const std::string all = screenText(r.frame());
    CHECK(contains(all, "LEFT"));
    CHECK(contains(all, "0:04.5"));
}

TEST(frame_turns_red_when_the_stack_is_high) {
    tui::Renderer r{tui::Theme::standard(), ColorMode::TrueColor};
    r.resize({80, 24});
    core::GameState s = playingState();
    r.render(s);
    const tui::Layout l = r.layout();
    const tui::Color calm = r.frame().at(l.board.x, l.board.y + 5).style.fg;
    s.board.set({0, game::Board::kHiddenRows + 2}, core::CellType::Z);
    r.render(s);
    const tui::Color alarmed = r.frame().at(l.board.x, l.board.y + 5).style.fg;
    CHECK(!(calm == alarmed));
    CHECK(alarmed == tui::Theme::standard().danger().fg);
}

TEST(menu_shows_mode_and_best) {
    tui::Renderer r{tui::Theme::standard(), ColorMode::TrueColor};
    r.resize({80, 24});
    core::GameState s;
    r.render(s, tui::HudInfo{4321, false, false});
    const std::string all = screenText(r.frame());
    CHECK(contains(all, "MODE"));
    CHECK(contains(all, "Endless"));
    CHECK(contains(all, "BEST  4,321"));
    r.render(s);
    CHECK(contains(screenText(r.frame()), "no best score yet"));
}

TEST(feedback_keeps_the_important_lines_when_space_is_short) {
    tui::Renderer r{tui::Theme::standard(), ColorMode::TrueColor};
    r.resize({58, 22});  // minimum size: only a few rows under NEXT
    core::GameState s = playingState();
    s.feedback = core::Feedback{1, 4, game::SpinKind::None, 3, true, true, 9000, true, core::Duration::zero()};
    r.render(s);
    const std::string all = screenText(r.frame());
    CHECK(contains(all, "QUAD!"));
    CHECK(contains(all, "+9,000"));
}

// --- Background detection ---------------------------------------------------

TEST(parses_background_replies) {
    const auto dark = tui::parseBackgroundReply("\x1b]11;rgb:1e1e/1e1e/1e1e\x1b\\");
    CHECK(dark.has_value());
    CHECK(*dark == (tui::Rgb{30, 30, 30}));
    CHECK(!tui::isLight(*dark));

    // BEL terminator, two-digit components, mixed with other replies.
    const auto light = tui::parseBackgroundReply("\x1b[?62;22c\x1b]11;rgb:ff/fa/f0\x07");
    CHECK(light.has_value());
    CHECK(*light == (tui::Rgb{255, 250, 240}));
    CHECK(tui::isLight(*light));

    CHECK(!tui::parseBackgroundReply("").has_value());
    CHECK(!tui::parseBackgroundReply("\x1b[?1;2c").has_value());         // only DA1
    CHECK(!tui::parseBackgroundReply("\x1b]11;rgb:ffff/ffff\x07").has_value());  // truncated
    CHECK(!tui::parseBackgroundReply("\x1b]11;rgb:zz/00/00\x07").has_value());
}

TEST(reads_colorfgbg) {
    CHECK(!tui::lightBackgroundFromColorFgBg(nullptr).has_value());
    CHECK(tui::lightBackgroundFromColorFgBg("15;0") == false);
    CHECK(tui::lightBackgroundFromColorFgBg("0;15") == true);
    CHECK(tui::lightBackgroundFromColorFgBg("0;default;7") == true);
    CHECK(!tui::lightBackgroundFromColorFgBg("garbage").has_value());
}

TEST(light_theme_has_light_well_and_dark_text) {
    const tui::Theme t = tui::Theme::light();
    CHECK(tui::isLight(t.wellBackground().value()));
    CHECK(!tui::isLight(t.value().fg.value()));
    CHECK(!tui::isLight(t.overlay().fg.value()));
    CHECK(tui::isLight(t.overlay().bg.value()));
}

TEST(parses_probe_replies) {
    const auto kitty = tui::parseCapabilities("\x1b]11;rgb:0000/0000/0000\x1b\\\x1b[?0u\x1b[?62;22c");
    CHECK(kitty.keyReleaseEvents);
    CHECK(kitty.background.has_value());
    const auto plain = tui::parseCapabilities("\x1b[?1;2c");
    CHECK(!plain.keyReleaseEvents);
    CHECK(!plain.background.has_value());
    CHECK(!tui::parseCapabilities("").keyReleaseEvents);
}

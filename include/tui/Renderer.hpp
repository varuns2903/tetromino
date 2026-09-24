#pragma once

// Turns a GameState into terminal output.
//
// This is the only component that knows both about the game (it reads
// GameState) and about the screen (it draws into a ScreenBuffer). It never
// modifies game state, and the engine never calls it.
//
// Per frame:
//   render(state)  -> draw the whole UI into the back buffer (cheap, in memory)
//   present(term)  -> diff back vs front, write only the changes, swap
//
// A future ncurses (or GUI, or web) front end would replace this class and
// Terminal, and reuse everything under core/ and game/ unchanged.

#include <string>

#include "tui/Color.hpp"
#include "tui/Layout.hpp"
#include "tui/ScreenBuffer.hpp"
#include "tui/Theme.hpp"

namespace tetromino::core {
struct GameState;
}

namespace tetromino::tui {

class Terminal;

class Renderer {
public:
    Renderer(Theme theme, ColorMode mode);

    // Recompute layout and force a full repaint on the next present().
    void resize(TerminalSize size);
    // Force a full repaint (e.g. after the terminal was suspended).
    void invalidate() { fullRepaint_ = true; }

    void render(const core::GameState& state);
    // Returns false if writing to the terminal failed.
    bool present(Terminal& terminal);

    [[nodiscard]] const Layout& layout() const { return layout_; }
    // For tests / debugging: the most recently rendered frame.
    [[nodiscard]] const ScreenBuffer& frame() const { return back_; }

private:
    void drawTooSmall();
    void drawStartScreen(const core::GameState& state);
    void drawPlayfield(const core::GameState& state);
    void drawBoard(const core::GameState& state, bool hideContents);
    void drawHold(const core::GameState& state);
    void drawStats(const core::GameState& state);
    void drawKeys();
    void drawNext(const core::GameState& state);
    void drawMessage(const core::GameState& state);
    void drawPauseOverlay();
    void drawGameOverOverlay(const core::GameState& state);

    // Draw one board cell at board-relative (col, row) of the visible area.
    void drawBoardCell(int col, int row, const CellGlyph& glyph);
    // Draw a piece shape (spawn orientation) centred in an area.
    void drawMiniPiece(core::PieceType type, int x, int y, int width, bool dimmed);
    // A centred box inside the well, returns its interior rect.
    Rect drawOverlayBox(int innerHeight, std::string_view title);

    Theme theme_;
    ColorMode mode_;
    Layout layout_;
    ScreenBuffer front_;
    ScreenBuffer back_;
    std::string output_;  // reused every frame to avoid reallocating
    bool fullRepaint_ = true;
};

}  // namespace tetromino::tui

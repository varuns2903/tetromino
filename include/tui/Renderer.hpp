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

#include <cstdint>
#include <optional>
#include <string>

#include "tui/Color.hpp"
#include "tui/Layout.hpp"
#include "tui/PixelCanvas.hpp"
#include "tui/ScreenBuffer.hpp"
#include "tui/Theme.hpp"

namespace tetromino::core {
struct GameState;
}

namespace tetromino::tui {

class Terminal;

// What the front end knows that isn't part of the game state: high scores
// (kept by the application) and whether the game on screen is a demo.
struct HudInfo {
    std::optional<std::uint64_t> best;  // best score for the selected setup
    bool newBest = false;               // game over: this game set it
    bool attract = false;               // a demo game played by the bot

    friend bool operator==(const HudInfo&, const HudInfo&) = default;
};

class Renderer {
public:
    Renderer(Theme theme, ColorMode mode);

    // New terminal size: layout is recomputed and the next present()
    // repaints everything.
    void resize(TerminalSize size);
    // Force a full repaint (e.g. after the terminal was suspended).
    void invalidate() { fullRepaint_ = true; }

    void render(const core::GameState& state, const HudInfo& hud = {});
    // Returns false if writing to the terminal failed.
    bool present(Terminal& terminal);

    // The playfield layout for this state's board size and rules (computed
    // on demand and cached).
    const Layout& layoutFor(const core::GameState& state);
    // The layout used by the most recent render().
    [[nodiscard]] const Layout& layout() const { return layout_; }
    // For tests / debugging: the most recently rendered frame.
    [[nodiscard]] const ScreenBuffer& frame() const { return back_; }

private:
    void drawTooSmall(TerminalSize minimum);
    void drawStartScreen(const core::GameState& state);
    void drawSetupMenu(const core::GameState& state, int y);
    void drawPlayfield(const core::GameState& state);
    void drawBoard(const core::GameState& state, bool hideContents);
    void drawBoardPixels(const core::GameState& state, bool hideContents);
    void drawBoardGlyphs(const core::GameState& state, bool hideContents);

    void drawHold(const core::GameState& state);
    void drawStats(const core::GameState& state);
    void drawKeys(const core::GameState& state);
    void drawNext(const core::GameState& state);
    void drawMessage(const core::GameState& state);
    void drawPauseOverlay();
    void drawCountdownOverlay(const core::GameState& state);
    void drawKeyHint(int x, int y, std::string_view key, std::string_view action);
    void drawGameOverOverlay(const core::GameState& state);

    // Draw one board cell at board-relative (col, row) of the visible area.
    void drawBoardCell(int col, int row, const CellGlyph& glyph);
    // Draw a piece shape (spawn orientation) centred in an area.
    void drawMiniPiece(core::PieceType type, int x, int y, int width, bool dimmed);
    // A centred box inside the well, returns its interior rect.
    Rect drawOverlayBox(int innerHeight, std::string_view title);

    Theme theme_;
    ColorMode mode_;
    HudInfo hud_;             // for the frame being rendered
    std::string boardTitle_;  // reused every frame
    TerminalSize terminal_{};
    Layout layout_;
    bool layoutValid_ = false;
    BoardShape layoutShape_{};
    int layoutPreviews_ = -1;
    bool halfBlocks_;  // pixel rendering available (colour terminal)
    PixelCanvas canvas_;  // reused every frame
    ScreenBuffer front_;
    ScreenBuffer back_;
    std::string output_;  // reused every frame to avoid reallocating
    bool fullRepaint_ = true;
};

}  // namespace tetromino::tui

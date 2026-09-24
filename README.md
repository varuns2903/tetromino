# Tetromino

[![CI](https://github.com/varuns2903/tetromino/actions/workflows/ci.yml/badge.svg)](https://github.com/varuns2903/tetromino/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A falling-block puzzle game for the Linux terminal, written in C++20 with
**no TUI framework**: just
`termios`, `poll()`, ANSI escape sequences and the standard library.

It's a playable game, and it's also a worked example of how terminal apps like
`lazygit`, `btop`, `k9s` or `vim` work under the hood: raw keyboard input,
escape-sequence rendering, double-buffered screen diffing, resize handling,
and putting the terminal back the way it was when the program exits.

```
           ╭─ HOLD ────────╮ ╭──── TETROMINO ─────╮ ╭─ NEXT ────────╮
           │               │ │ · · · ·██ · · · · ·│ │               │
           │   ████████    │ │ · · ·██████ · · · ·│ │        ██     │
           │               │ │ · · · · · · · · · ·│ │    ██████     │
           │               │ │ · · · · · · · · · ·│ │               │
           ╰───────────────╯ │ · · · · · · · · · ·│ │     ████      │
           ╭─ SCORE ───────╮ │ · · · · · · · · · ·│ │     ████      │
           │        12,450 │ │ · · · · · · · · · ·│ │               │
           │               │ │ · · · · · · · · · ·│ │      ████     │
           │ LEVEL       2 │ │ · · · · · · · · · ·│ │    ████       │
           │ LINES      17 │ │ · · · · · · · · · ·│ │               │
           │               │ │ · · · · · · · · · ·│ │    ████       │
           │ ━━━━━━━━━──── │ │ · · · · · · · · · ·│ │      ████     │
           ╰───────────────╯ │ · · · · · · · · · ·│ │               │
           ╭─ KEYS ────────╮ │ · · · · · · · · · ·│ │    ██         │
           │ ← → move      │ │ · · · ·░░ · · · · ·│ │    ██████     │
           │ ↑ x rotate    │ │ · · ·░░░░░░ · · · ·│ │               │
           │ ↓   soft drop │ │██████████████ ·████│ ╰───────────────╯
           │ spc hard drop │ │████████████ ·██████│
           │ c   hold      │ │██████████ ·████████│
           │ p   pause     │ │████████ ·██████████│
           ╰───────────────╯ ╰────────────────────╯
```

## Features

- **Setup menu on the start screen**: pick a board size and a difficulty
  before each game (see [Board sizes and difficulty](#board-sizes-and-difficulty))

- Modern falling-block rules: 10×20 field with a hidden spawn buffer, all seven
  tetrominoes, **rotation with wall and floor kicks** (SRS kick tables), **7-bag randomizer**,
  hold (once per piece), a 5-piece next queue, ghost piece, lock delay with
  move-reset (limited to 15 resets), soft and hard drop
- Standard scoring (100/300/500/800 × level, +1 per soft-dropped row, +2 per
  hard-dropped row). The level goes up every 10 lines and gravity follows the
  common modern speed curve
- Line-clear animation (a flash, then a wipe out from the centre) with a
  SINGLE / DOUBLE / TRIPLE / QUAD! callout
- Start menu, pause, game over, restart and back-to-menu, all run by one explicit state machine
- Responsive layout: the game is centred and the board is drawn at the
  largest size that fits (1×, 1.5×, 2×, 2.5×… using half-block pixels), with
  side-panel pieces that scale along, and shows a "terminal too small" screen (and auto-pauses) when
  the window is too small
- 24-bit colour with automatic fallback to 256, 16 or no colours
  (`NO_COLOR` is honoured)
- Diff-based rendering inside synchronized-update brackets, so there's no flicker
  and only changed cells are written. Moving a piece one column costs about 380
  bytes, compared with about 7 KB for a full repaint
- 0% CPU while idle: the loop sleeps in `poll()`, not in a busy loop
- The terminal is restored on every exit path: `q`, Ctrl-C, `SIGTERM`, `SIGHUP`,
  crashes (`SIGSEGV`/`SIGABRT`) and Ctrl-Z suspend (restored while stopped,
  re-entered on `fg`)
- Auto-pause when the window loses focus
- Reproducible games with `--seed`
- 119 unit tests with no dependencies, plus a pty-based terminal-restoration check

## Controls

| Key                | Action                       |
|--------------------|------------------------------|
| `←` `→` / `A` `D`  | Move                         |
| `↓` / `S`          | Soft drop                    |
| `↑` / `W` / `X`    | Rotate clockwise             |
| `Z`                | Rotate counter-clockwise     |
| `Space`            | Hard drop                    |
| `C`                | Hold                         |
| `P` / `Esc`        | Pause / resume (Esc quits from the start and game-over screens) |
| `R`                | Restart (paused or game over)|
| `M`                | Back to the setup menu (paused or game over) |
| `↑` `↓` / `W` `S`  | Menu: choose board / difficulty row |
| `←` `→` / `A` `D`  | Menu: change the selected option |
| `Enter`            | Start (start screen)         |
| `Q` / `Ctrl-C`     | Quit                         |
| `Ctrl-Z`           | Suspend to the shell (`fg` to resume) |

## Building

Requirements: Linux, CMake ≥ 3.20, and a C++20 compiler with `<format>` (GCC ≥ 13 or a recent Clang). Tested with GCC 16.2 and Clang 22.1.

```bash
cmake -S . -B build              # defaults to a Release build
cmake --build build
./build/tetromino
```

Other build types and options:

```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build-asan  -DCMAKE_BUILD_TYPE=Debug -DTETROMINO_SANITIZE=ON   # ASan + UBSan
cmake --build build --config Release                                      # multi-config generators
cmake -S . -B build -DTETROMINO_WARNINGS_AS_ERRORS=OFF                        # if a newer compiler adds warnings
```

The build uses `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
-Wold-style-cast` and several more warnings, with `-Werror` turned on. It builds
without warnings on GCC 16 and Clang.

### Tests

```bash
ctest --test-dir build --output-on-failure    # unit tests (engine + TUI helpers)
python3 scripts/pty_check.py build/tetromino # terminal restoration on a real pty
```

`pty_check.py` runs the binary on a pseudo-terminal, drives it with keys and
signals, and checks that the tty's termios settings afterwards are
byte-for-byte what they were before. It covers normal quit, Escape, Ctrl-C,
arrow keys, resizes, `SIGTERM`, `SIGHUP`, and the crash paths `SIGSEGV` and
`SIGABRT`.

## Running

```
tetromino [--size NAME] [--difficulty NAME] [--level N] [--seed N] [--color truecolor|256|16|mono] [--mono] [--fps N] [--debug]
```

- `--size` and `--difficulty` pre-select the start-menu entries
  (`small|classic|wide|tall`, `easy|normal|hard|expert`). You still confirm
  with Enter.
- `--seed N` gives the same piece sequence on every machine. The randomizer
  doesn't use the implementation-defined `std::uniform_int_distribution` or
  `std::shuffle`.
- `--debug` (or `TETROMINO_DEBUG=1`) writes a log to
  `$XDG_STATE_HOME/tetromino/tetromino.log`, which is usually
  `~/.local/state/tetromino/tetromino.log`. Nothing is ever printed to stdout
  while the game is running, because stdout belongs to the TUI.

## Terminal compatibility

Tested headlessly in tmux and on raw ptys. The game only uses widely supported
sequences, so it should work in Kitty, Foot, Alacritty, WezTerm, GNOME Terminal,
Konsole, xterm, tmux and screen.

- **Minimum size**: the start menu needs 50×20; the game depends on the
  board: 54×18 (Small; 54×20 on Easy for the 5 previews), 58×22 (Classic),
  66×22 (Wide), 58×26 (Tall). The start menu warns when the chosen board
  won't fit. Bigger terminals get a bigger board, in half-size steps.
- **Colour**: detected from `COLORTERM` and `TERM`. Override it with `--color`.
- **Fonts**: the terminal needs a UTF-8 locale and a font with box-drawing
  (`╭─╮`) and block (`█░`) glyphs, which almost every monospace font has.
- **Synchronized output** (DEC mode 2026) and **focus events** (mode 1004) are
  used where supported and ignored elsewhere.

## Architecture

```
tetromino/
├── include/ src/
│   ├── core/    Game (engine + state machine), GameState (snapshot), GameConfig, Presets, Types
│   ├── game/    Board, Piece, PieceGenerator, Collision, Rotation, Scoring
│   ├── tui/     Terminal, Input, Ansi, Color, Theme, ScreenBuffer, PixelCanvas, Panel, Layout, Renderer
│   ├── util/    Logger, Random
│   └── app/     Application (the main loop), Options (CLI)
├── tests/       one executable per area + a ~60-line test harness
└── scripts/     pty_check.py
```

These are the layers, and the build enforces the arrows:

```
                ┌─────────────────────────────────────────┐
                │ app/        Application: the main loop  │
                └───────┬───────────────────────┬─────────┘
                        │                       │
        ┌───────────────▼──────────┐   ┌────────▼────────────────────────┐
        │ tui/  (libtetromino_tui)    │   │ core/ + game/  (libtetromino_engine)│
        │ Terminal  Input          │──▶│ Game ──▶ GameState               │
        │ Renderer  ScreenBuffer   │   │ Board Piece Rotation Collision   │
        │ Ansi Color Theme Layout  │   │ PieceGenerator Scoring           │
        └──────────────────────────┘   └──────────────────────────────────┘
             knows about terminals          knows nothing about terminals
```

- **`tetromino_engine` has no terminal code.** It holds no escape sequences, no
  I/O, no clock and no sleeping. The game moves forward only through
  `Game::apply(Action)` and `Game::update(Duration)`, so it is deterministic and
  easy to test. The engine tests link against this library alone.
- **`GameState` is the whole contract** between the engine and any front end.
  The renderer gets a `const GameState&` and nothing else.
- **`Terminal` is the only place with `termios`, `ioctl` and signal code.**
  **`Ansi.hpp` is the only place with escape sequences.**
- **`Theme` owns every colour and glyph.** The renderer asks the theme how to
  draw "a locked T cell" or "a ghost cell", which keeps monochrome mode to a
  single alternative theme.

Replacing `termios` + ANSI with ncurses (or SDL, or a web socket) would mean
writing a new `Renderer` and input adapter. `core/` and `game/` wouldn't change:

```
                 Game Engine
                      │
                      ▼
                  GameState
          ┌───────────┴───────────┐
          ▼                       ▼
     ANSI Renderer          ncurses Renderer (future)
          │                       │
          ▼                       ▼
       Terminal                Terminal
```

Where this differs from the suggested layout:

- `Ansi.hpp` and `Theme.hpp` are split out of `Color.hpp`, so escape sequences
  and the choice of palette each live in exactly one place.
- `app/` exists so the loop that connects the terminal to the engine lives in
  neither of them.
- Tests use a tiny built-in harness (`tests/TestFramework.hpp`) instead of
  GoogleTest or Catch2, to keep the project free of dependencies.

## How the TUI works

```
 Keyboard
    │  bytes: 'a', or ESC [ D for ←
    ▼
 termios raw mode ─────────── Terminal.cpp: no line buffering, no echo, no signals
    │  read() never blocks (VMIN=0); poll() sleeps until input or a timeout
    ▼
 Input decoder ────────────── Input.cpp: ESC [ D → Key::Left, lone ESC → Key::Escape (after 25 ms)
    │
    ▼
 Key bindings ─────────────── Input.cpp actionFor(): Key::Left in Playing → Action::MoveLeft
    │
    ▼
 Game::apply / Game::update ─ core/Game.cpp: rules, timers, state machine
    │
    ▼
 GameState ────────────────── a read-only snapshot
    │
    ▼
 Renderer ─────────────────── draws the whole UI into the back ScreenBuffer (in memory)
    │
    ▼
 ScreenBuffer diff ────────── compares back with front and emits only the changed cells
    │  ESC[12;40H  ESC[0;38;2;175;80;235m  ██
    ▼
 Terminal emulator ────────── parses the bytes and paints pixels
```

### Raw mode (`src/tui/Terminal.cpp`)

By default a tty is in *canonical* mode. The kernel buffers a whole line,
handles backspace, echoes keys and only passes input to the program when you
press Enter. It also turns Ctrl-C into `SIGINT`. The game switches off `ICANON`
(line buffering), `ECHO`, `ISIG` (so Ctrl-C and Ctrl-Z arrive as bytes 0x03 and
0x1a), `IXON` (so Ctrl-S doesn't freeze output) and `OPOST`. It sets
`VMIN=0/VTIME=0` so that `read()` never blocks.

`O_NONBLOCK` is deliberately left off. On a tty, stdin and stdout usually share
one open file description, so setting it on stdin quietly makes stdout
non-blocking as well, and large frames would then fail with `EAGAIN`.

Restoring the terminal is handled in layers:

1. The `Terminal` destructor covers normal exits and exceptions.
2. Signal handlers set a flag on `SIGINT`/`SIGTERM`/`SIGHUP`, so the main loop
   exits normally and the destructor runs.
3. Fatal signals (`SIGSEGV`, `SIGABRT` and so on) run an async-signal-safe
   emergency restore, a `tcsetattr` plus one `write`, and then re-raise the
   signal so the exit status stays accurate.
4. `SIGTSTP`/`SIGCONT` restore the terminal before stopping and re-enter game
   mode on `fg`.

### ANSI escape sequences (`include/tui/Ansi.hpp`)

A terminal emulator is a state machine that reads a byte stream. Most bytes are
printed; `ESC [` starts a command:

| Sequence | Meaning |
|---|---|
| `ESC[row;colH` | move the cursor (1-based) |
| `ESC[2J` | clear the screen |
| `ESC[0;1;38;2;r;g;bm` | SGR: reset, bold, 24-bit foreground colour |
| `ESC[?25l` / `h` | hide / show the cursor |
| `ESC[?1049h` / `l` | enter / leave the **alternate screen**, a second buffer with no scrollback. Leaving it brings your shell history back untouched |
| `ESC[?7l` | turn off auto-wrap, so writing the bottom-right cell can't scroll the screen |
| `ESC[?2026h` … `l` | synchronized update: the terminal paints the frame all at once |

### Screen buffering (`include/tui/ScreenBuffer.hpp`)

Clearing the screen and printing everything each frame causes flicker, because
the terminal shows the blank screen for a moment. It is also wasteful: moving a
piece changes about 16 cells out of 1,700. The renderer draws into a back buffer
of `Cell{char32_t, Style}` instead. `encodeDiff()` compares that with the front
buffer, which is a copy of what is on screen. For each changed cell it emits a
cursor move (skipped when the cursor is already there), a style change (skipped
when the style hasn't changed) and the glyph. Then `front = back`. If nothing
changed, nothing is written. React's virtual DOM and curses' `refresh()` work
the same way.

### Half-block pixels (`include/tui/PixelCanvas.hpp`)

A terminal cell is about twice as tall as it is wide, so `▀` (upper half
block) with a foreground *and* a background colour shows two square-ish
pixels stacked in one cell. The board is drawn into a small bitmap at that
resolution and then converted to `▀`/`▄`/space cells. Because a board cell
can then be any whole number of these pixels (2 = 1×, 3 = 1.5×, 4 = 2×, …),
the layout picks the largest size that fits instead of jumping between 1× and
2×. Monochrome terminals can't set two colours per cell, so they keep
character rendering (`██`, `░░`, `·`) at whole multiples.

### The game loop (`src/app/Application.cpp`)

```
render (only if Game::revision() changed) → poll(stdin, time until the next frame)
→ decode keys → game.apply(action) → handle resize/suspend → game.update(elapsed)
```

Input, update and render are kept apart so each runs at its own pace. Keys take
effect as soon as `poll()` wakes up. The simulation advances by however much
wall-clock time has passed, and rendering is skipped when nothing visible has
changed. In menus and while paused there's no frame deadline, so the process
stays asleep in `poll()` until a key or signal arrives.

### Elapsed-time gravity (`Game::updateGravity`)

Gravity is measured in **time per row**: 1 s at level 1, about 64 ms at level
10. The game adds up elapsed time and moves one row for each full interval, so
the fall speed is the same at 30 FPS or 144 FPS. At high levels a piece can fall
several rows in a single frame. The unit test
`gravity_is_independent_of_update_granularity` checks this.

### Collision detection (`include/game/Collision.hpp`)

Each piece is `(type, rotation, box position)`. The shape table gives four cell
offsets for each type and rotation, and adding the box position to each offset
gives the absolute board cells. `fits()` checks that none of those cells is
outside the walls or floor or already filled. Movement, every wall-kick test,
gravity, hard drop, the ghost piece and spawning all call `fits()`; nothing else
checks for collisions.

## Board sizes and difficulty

The start screen is a small setup menu. `↑`/`↓` pick a row, `←`/`→` change
it, and `Enter` starts. `M` from the pause or game-over screen returns to it.

| Board   | Size (columns × rows) |
|---------|-----------------------|
| Small   | 8 × 16  |
| Classic | 10 × 20 |
| Wide    | 14 × 20 |
| Tall    | 10 × 24 |

| Difficulty | Fall speed | Start level | Hold | Next pieces shown | Ghost | Lock delay |
|------------|------------|-------------|------|-------------------|-------|------------|
| Easy       | ×1.5 (slower) | +0 | ✓ | 5 | ✓ | 700 ms |
| Normal     | standard    | +0 | ✓ | 3 | ✓ | 500 ms |
| Hard       | ×0.6 (faster) | +2 | ✗ | 1 | ✓ | 400 ms |
| Expert     | ×0.35 (much faster) | +4 | ✗ | 0 | ✗ | 300 ms |

A higher start level also means more points per line, since line clears are
multiplied by the level. The presets live in `include/core/Presets.hpp`: a
difficulty is data (gravity multiplier, level bonus, hold / preview / ghost
switches, lock delay), and `core::configFor()` applies it to the base
`GameConfig` when a game starts. The front end reads what's enabled from
`GameState::rules` and shows `off` in the HOLD and NEXT panels when a feature
is disabled.

## Game mechanics

- **Board**: the chosen width and visible height (10 × 20 by default), with 4
  hidden rows above. Pieces spawn on the top visible row, centred (on the
  classic board: I at columns 3–6, O at 4–5, the rest at 3–5).
- **Rotation**: SRS kick tables. The four states are generated at compile time by rotating
  the spawn shape inside its 3×3 (or 4×4 for I) box. The kick tables follow the
  widely published reference, with y flipped because the board's y axis points down.
  The O piece never kicks.
- **Lock delay**: 500 ms. Each successful move or rotation while grounded
  restarts the timer, up to 15 times. Reaching a new lowest row resets that
  count.
- **Line clear**: rows are found only after a piece locks. They stay on the
  board for 280 ms for the animation and then collapse.
- **Game over**: a new piece can't spawn (block out), or a piece locks entirely
  inside the hidden rows (lock out).
- **Levels**: `level = start + lines / 10`. Gravity is
  `(0.8 − (level−1)·0.007)^(level−1)` seconds per row, multiplied by the
  difficulty's fall-speed factor.

Every rule lives in `GameConfig` / `ScoringRules`: lock delay, reset limit,
clear delay, gravity curve, points and lines per level.

## Roadmap / future improvements

- [ ] Spin-move detection and combo scoring (the hook is `Scoring`)
- [ ] DAS/ARR (auto-repeat tuned by the game instead of the OS). This needs
      key-release events, for example from the kitty keyboard protocol (`CSI > 1 u`)
- [ ] High-score table in `$XDG_STATE_HOME`
- [ ] 180° rotation, a configurable keymap, and themes loaded from a file
- [ ] An ncurses renderer next to the ANSI one, to show that the front end can
      be swapped
- [ ] More modes: endless, a 40-line race, and a 2-minute score attack
- [ ] Replays: `--seed` plus a recorded action/time log is enough to replay a game exactly

## License

MIT. See [LICENSE](LICENSE).

Tetromino is an independent, non-commercial open-source project. It isn't
affiliated with, sponsored or endorsed by The Tetris Company or any other game
publisher, and it uses none of their names, logos, artwork, music or colour
schemes. "Tetromino" is the generic mathematical term for a shape made of four
squares. Tetris® is a registered trademark of The Tetris Company, LLC.

# Changelog

All notable changes to this project. Versions follow
[Semantic Versioning](https://semver.org/). Each release's GitHub notes are
taken from its section here.

## [1.1.0] - 2026-09-24

### Added
- Modes: **Endless** and **2-Minute** score attack, chosen in a new MODE
  row of the setup menu (`--mode`)
- Scoring depth: combos, back-to-back bonus, spins (full and mini), and
  perfect-clear bonus
- On-screen feedback for clears (QUAD!, SPIN DOUBLE, B2B, COMBO, PERFECT
  CLEAR, points earned), a level-up flash, and a red board frame when the
  stack gets close to the top
- 3-2-1 countdown when resuming from pause
- High scores per mode, board and difficulty, saved in
  `$XDG_STATE_HOME/tetromino/highscores`; best shown in the menu and while
  playing; NEW BEST! and statistics (time, pieces per second, quads, spins,
  best combo) on the game-over screen
- Light theme, chosen automatically from the terminal's background colour
  (`--theme auto|dark|light`)
- Held-key movement timed by the game on terminals that report key releases
  (kitty keyboard protocol); `--das`, `--arr`, `--legacy-keys`
- Attract mode: a built-in bot plays a demo when the menu is idle for 15 s
  (`--no-demo`)
- Arch Linux packages (`tetromino`, `tetromino-bin`) in `packaging/aur`
- Homebrew formula: `brew install varuns2903/tap/tetromino`

### Changed
- The score panel shows elapsed time (or time left in 2-Minute mode); the
  board title shows the mode and difficulty
- Release binaries are static-PIE with full RELRO (address-space layout
  randomisation and read-only relocations), still with no dependencies

## [1.0.1] - 2026-09-24

Packaging release; gameplay is unchanged from 1.0.0.

### Added
- `.deb` (Debian, Ubuntu) and `.rpm` (Fedora, openSUSE) packages
- arm64 (aarch64) builds: static binary, `.deb` and `.rpm`
- Man page: `man tetromino` (section 6)
- `cmake --install` installs the binary, man page, README and license
- Release workflow: pushing a version tag builds, tests and publishes all
  artifacts with checksums
- Demo GIF in the README

## [1.0.0] - 2026-09-24

First public release.

### Added
- Setup menu: four board sizes (Small, Classic, Wide, Tall) and four
  difficulties (Easy, Normal, Hard, Expert) that change fall speed, start
  level, hold, number of next pieces, ghost piece and lock delay
- Wall-kick rotation, 7-bag randomizer, hold, next queue, ghost piece, lock
  delay, line-clear animation, scoring and levels
- Board drawn at the largest size that fits the terminal, in half steps
- 24-bit colour with 256 / 16 / monochrome fallback; `NO_COLOR` support
- Terminal restored on every exit path, including signals and crashes;
  Ctrl-Z suspend
- Static x86_64 Linux binary

[1.1.0]: https://github.com/varuns2903/tetromino/releases/tag/v1.1.0
[1.0.1]: https://github.com/varuns2903/tetromino/releases/tag/v1.0.1
[1.0.0]: https://github.com/varuns2903/tetromino/releases/tag/v1.0.0

# Demo recording for the README. The launch line uses a throwaway
# XDG_STATE_HOME so recording never touches your real high-score file.
# Don't run vhs on this directly; use
#
#   scripts/make-demo.sh
#
# which records the frames with VHS and assembles assets/demo.gif with ffmpeg
# (VHS 0.12's built-in GIF step fails with ffmpeg 9).
#
# The piece sequence is fixed by --seed 37. The moves below were planned
# against the game engine (Classic board, Normal difficulty): ten pieces, three
# holds, and the tenth piece clears four lines at once, emptying the board.
# Changing the seed, board, difficulty or game rules invalidates the moves.


Set Shell "bash"
Set FontFamily "Adwaita Mono"
Set FontSize 16
Set Width 1200
Set Height 820
Set Padding 16
Set Framerate 30
Set Theme { "name": "Tetromino", "background": "#1e1e1e", "foreground": "#dcdce1", "cursor": "#1e1e1e", "black": "#1e1e1e", "red": "#f04141", "green": "#46d75a", "yellow": "#f5d700", "blue": "#376ef5", "magenta": "#af50eb", "cyan": "#00d7eb", "white": "#dcdce1", "brightBlack": "#5f6480", "brightRed": "#ff6b6b", "brightGreen": "#6ee77e", "brightYellow": "#ffe45c", "brightBlue": "#5c8cff", "brightMagenta": "#c77dff", "brightCyan": "#5ce7f5", "brightWhite": "#ffffff" }

# Launch without showing the command
Hide
Type "XDG_STATE_HOME=$(mktemp -d) ./build/tetromino --seed 37 --theme dark --color truecolor --no-demo"
Enter
Sleep 900ms
Show

# Setup menu: browse each row, then start with Endless / Classic / Normal
Sleep 1.5s
Right@500ms
Left@400ms
Down@400ms
Right@500ms 2
Left@350ms 2
Down@400ms
Right@500ms 2
Left@350ms 2
Sleep 600ms
Enter
Sleep 900ms

# Piece 1: rotate x2, left 4, drop
Up@180ms 2
Sleep 120ms
Left@110ms 4
Sleep 250ms
Space
Sleep 500ms

# Piece 2: hold, right 3, drop
Type "c"
Sleep 450ms
Right@110ms 3
Sleep 250ms
Space
Sleep 500ms

# Piece 3: left 1, drop
Left
Sleep 250ms
Space
Sleep 500ms

# Piece 4: rotate x2, right 1, drop
Up@180ms 2
Sleep 120ms
Right
Sleep 250ms
Space
Sleep 500ms

# Piece 5: rotate x1, right 3, drop
Up
Sleep 120ms
Right@110ms 3
Sleep 250ms
Space
Sleep 500ms

# Piece 6: hold, right 2, drop
Type "c"
Sleep 450ms
Right@110ms 2
Sleep 250ms
Space
Sleep 500ms

# Piece 7: rotate x2, left 2, drop
Up@180ms 2
Sleep 120ms
Left@110ms 2
Sleep 250ms
Space
Sleep 500ms

# Piece 8: rotate x2, drop
Up@180ms 2
Sleep 120ms
Sleep 250ms
Space
Sleep 500ms

# Piece 9: rotate x2, left 3, drop
Up@180ms 2
Sleep 120ms
Left@110ms 3
Sleep 250ms
Space
Sleep 500ms

# Piece 10: hold, rotate x1, right 4, drop -> QUAD
Type "c"
Sleep 450ms
Up
Sleep 120ms
Right@110ms 4
Sleep 250ms
Space
Sleep 2s

# Pause screen, then quit
Type "p"
Sleep 2s
Type "q"
Sleep 300ms

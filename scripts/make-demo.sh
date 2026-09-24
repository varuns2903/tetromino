#!/usr/bin/env bash
# Record assets/demo.gif (the README demo) from assets/demo.tape.
#
# Needs: vhs (and ttyd), ffmpeg, gifsicle. Run from anywhere in the repo.
#
# VHS 0.12 records fine but its own GIF step fails silently with ffmpeg 9, so
# VHS only writes the frames and ffmpeg assembles the GIF here.
set -euo pipefail
cd "$(dirname "$0")/.."

cmake --build build

# VHS moves its frame directory into place with a rename, so the target must
# not exist yet and must be on the same filesystem as $TMPDIR.
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
frames="$work/frames"
{ printf 'Output "%s/"\n' "$frames"; cat assets/demo.tape; } > "$work/demo.tape"
vhs --quiet "$work/demo.tape"

# Frames are the terminal text layer without padding: add it back, build a
# palette tuned for flat terminal colours, and encode.
ffmpeg -y -loglevel error -framerate 30 -i "$frames/frame-text-%05d.png" \
    -vf "pad=iw+32:ih+32:16:16:color=0x1e1e1e,split[a][b];[a]palettegen=max_colors=128:stats_mode=diff[p];[b][p]paletteuse=dither=none:diff_mode=rectangle" \
    assets/demo.gif
gifsicle -O3 --lossy=60 -o assets/demo.gif assets/demo.gif

ls -lh assets/demo.gif

#!/usr/bin/env bash
# Build the release artifacts for the current machine's architecture into dist/:
#
#   tetromino-<version>-linux-<arch>.tar.gz   static binary + man page + docs
#   tetromino_<version>_<debarch>.deb         Debian / Ubuntu package
#   tetromino-<version>-1.<arch>.rpm          Fedora / openSUSE package
#
# The binary is linked fully statically, so it runs on any Linux distribution
# with the same CPU architecture, whatever its glibc version.
#
# Needs: cmake, ninja, a C++20 compiler (CXX, default g++), python3 (for the
# terminal checks), dpkg-dev and rpm (for the packages). Used by the release
# workflow; also works locally.
set -euo pipefail
cd "$(dirname "$0")/.."

build=build-release
rm -rf "$build" dist
mkdir -p dist

cmake -S . -B "$build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER="${CXX:-g++}" \
    -DTETROMINO_STATIC=ON
cmake --build "$build"

# Never ship something that doesn't pass its own checks.
ctest --test-dir "$build" --output-on-failure
python3 scripts/pty_check.py "$build/tetromino"

version="$("$build/tetromino" --version | awk '{print $2}')"
arch="$(uname -m)"

# Tarball: a directory that can be run in place.
stage="dist/tetromino-$version-linux-$arch"
mkdir -p "$stage"
cp "$build/tetromino" "$stage/"
strip "$stage/tetromino"
cp docs/tetromino.6 README.md LICENSE "$stage/"
tar -C dist -czf "$stage.tar.gz" "$(basename "$stage")"
rm -rf "$stage"

# .deb and .rpm via CPack (installs under /usr).
(cd "$build" && cpack -G "DEB;RPM" -B "$PWD/../dist/cpack" >/dev/null)
mv dist/cpack/*.deb dist/cpack/*.rpm dist/
rm -rf dist/cpack

(cd dist && sha256sum -- *.tar.gz *.deb *.rpm > "SHA256SUMS-$arch.txt")
ls -l dist

#!/usr/bin/env bash
# build_x11.sh — Build Frogger with X11/OpenGL backend
# Usage: ./build_x11.sh [debug|release]
#
# Requires: clang, libx11-dev, libxkbcommon-dev, mesa (GL/GLX)
#   sudo apt install clang libx11-dev libxkbcommon-dev libgl-dev

set -e

mkdir -p build

MODE="${1:-debug}"
OUT="build/x11"
CFLAGS="-Wall -Wextra -Wpedantic -std=c99 -DASSETS_DIR=\"assets\""

if [ "$MODE" = "release" ]; then
    CFLAGS="$CFLAGS -O2"
else
    CFLAGS="$CFLAGS -g"
fi

echo "Building $OUT ($MODE)..."
clang $CFLAGS -o "$OUT" src/frogger.c src/main_x11.c \
-lX11 -lxkbcommon -lGL -lGLX

echo "Done! Run with: ./$OUT"
echo "(Run from the course/ directory so 'assets/' is found)"

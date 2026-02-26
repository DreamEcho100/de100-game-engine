#!/usr/bin/env bash
# build_x11.sh — Build Frogger with X11 backend
# Usage: ./build_x11.sh [debug|release]
#
# Requires: gcc, libx11-dev
#   sudo apt install libx11-dev

set -e   # exit on first error

mkdir -p build

MODE="${1:-debug}"
OUT="build/frogger_x11"
CFLAGS="-Wall -Wextra -std=c11 -DASSETS_DIR=\"assets\""

if [ "$MODE" = "release" ]; then
    CFLAGS="$CFLAGS -O2"
else
    CFLAGS="$CFLAGS -g"
fi

echo "Building $OUT ($MODE)..."
gcc $CFLAGS -o "$OUT" src/frogger.c src/main_x11.c -lX11

echo "Done. Run with: ./$OUT"
echo "(Make sure to run from the course/ directory so 'assets/' is found)"

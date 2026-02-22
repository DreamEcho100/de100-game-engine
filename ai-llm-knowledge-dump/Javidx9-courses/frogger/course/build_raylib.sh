#!/usr/bin/env bash
# build_raylib.sh — Build Frogger with Raylib backend
# Usage: ./build_raylib.sh [debug|release]
#
# Requires: gcc, raylib
#   sudo apt install libraylib-dev
#   OR install from source: https://github.com/raysan5/raylib

set -e

mkdir -p build

MODE="${1:-debug}"
OUT="build/frogger_raylib"
CFLAGS="-Wall -Wextra -std=c11 -DASSETS_DIR=\"assets\""

if [ "$MODE" = "release" ]; then
    CFLAGS="$CFLAGS -O2"
else
    CFLAGS="$CFLAGS -g"
fi

echo "Building $OUT ($MODE)..."
gcc $CFLAGS -o "$OUT" src/frogger.c src/main_raylib.c -lraylib -lm

echo "Done. Run with: ./$OUT"
echo "(Make sure to run from the course/ directory so 'assets/' is found)"

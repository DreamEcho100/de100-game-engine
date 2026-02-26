#!/usr/bin/env bash
# build_raylib.sh — Build Frogger with Raylib backend
# Usage: ./build_raylib.sh [debug|release]
#
# Requires: clang, raylib
#   sudo apt install clang libraylib-dev

set -e

mkdir -p build

MODE="${1:-debug}"
OUT="build/raylib"
CFLAGS="-Wall -Wextra -Wpedantic -std=c99 -DASSETS_DIR=\"assets\""

if [ "$MODE" = "release" ]; then
    CFLAGS="$CFLAGS -O2"
else
    CFLAGS="$CFLAGS -g"
fi

echo "Building $OUT ($MODE)..."
clang $CFLAGS -o "$OUT" src/frogger.c src/main_raylib.c \
-lraylib -lm

echo "Done! Run with: ./$OUT"
echo "(Run from the course/ directory so 'assets/' is found)"

#!/usr/bin/env bash
# build_raylib.sh — Build Sugar, Sugar with the Raylib backend
# Usage:  ./build_raylib.sh        (release)
#         ./build_raylib.sh debug  (debug + sanitizers)
#
# Requirements:  Raylib 5.x installed system-wide
#   Ubuntu/Debian: sudo apt install libraylib-dev
#   Or build from source: https://github.com/raysan5/raylib

set -e

mkdir -p build

SRC_DIR="src"
OUTPUT="build/raylib"
SOURCES="$SRC_DIR/main_raylib.c $SRC_DIR/game.c $SRC_DIR/levels.c"
LIBS="-lraylib -lm -lpthread -ldl"

if [ "$1" = "debug" ]; then
    echo "Building DEBUG (Raylib)..."
    clang -Wall -Wextra \
    -O0 -g \
    -fsanitize=address,undefined \
    -o "${OUTPUT}_dbg" \
    $SOURCES $LIBS
    echo "Done: ./${OUTPUT}_dbg"
else
    echo "Building RELEASE (Raylib)..."
    clang -Wall -Wextra \
    -O2 \
    -o "$OUTPUT" \
    $SOURCES $LIBS
    echo "Done: ./$OUTPUT"
    echo "Run: ./$OUTPUT"
fi

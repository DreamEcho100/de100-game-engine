#!/bin/bash
# build_raylib.sh — Build the Raylib version of Tetris
#
# Requirements:
#   sudo apt install libraylib-dev   (Ubuntu 22.04+)
#   OR build Raylib from source: https://github.com/raysan5/raylib
#
# Usage:
#   chmod +x build_raylib.sh
#   ./build_raylib.sh
#   ./tetris_raylib

set -e  # exit immediately on any error

mkdir -p build

BINARY="tetris_raylib"
SOURCES="src/tetris.c src/main_raylib.c"
FLAGS="-Wall -Wextra -g"
LIBS="-lraylib -lm -lpthread -ldl"

echo "Building $BINARY..."
gcc $FLAGS -o ./build/$BINARY $SOURCES $LIBS
echo "Done! Run with: ./build/$BINARY"

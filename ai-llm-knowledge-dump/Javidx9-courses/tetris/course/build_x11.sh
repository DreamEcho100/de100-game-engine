#!/bin/bash
# build_x11.sh — Build the X11 version of Tetris
#
# Requirements:
#   sudo apt install libx11-dev   (on Ubuntu/Debian)
#
# Usage:
#   chmod +x build_x11.sh
#   ./build_x11.sh
#   ./tetris_x11

set -e  # exit immediately on any error

mkdir -p build

BINARY="tetris_x11"
SOURCES="src/tetris.c src/main_x11.c"
FLAGS="-Wall -Wextra -g"
LIBS="-lX11"

echo "Building $BINARY..."
gcc $FLAGS -o ./build/$BINARY $SOURCES $LIBS
echo "Done! Run with: ./build/$BINARY"

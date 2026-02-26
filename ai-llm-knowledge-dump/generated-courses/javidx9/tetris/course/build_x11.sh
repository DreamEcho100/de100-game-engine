#!/bin/bash
# build_x11.sh — Build the X11 version of Tetris
#
# Requirements:
#   sudo apt install libx11-dev libgl-dev libxkbcommon-dev   (on Ubuntu/Debian)
#
# Usage:
#   chmod +x build_x11.sh
#   ./build_x11.sh
#   ./build/x11

set -e  # exit immediately on any error

mkdir -p build

BINARY="x11"
SOURCES="src/tetris.c src/main_x11.c"
FLAGS="-Wall -Wextra -g"
LIBS="-lX11 -lxkbcommon -lGL -lGLX"

echo "Building $BINARY..."
clang $FLAGS -o ./build/$BINARY $SOURCES $LIBS
echo "Done! Run with: ./build/$BINARY"

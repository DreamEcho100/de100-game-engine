#!/bin/bash
# build_x11.sh — Build the X11/GLX version of Snake
#
# Prerequisites:
#   sudo apt install libx11-dev libxkbcommon-dev
#   (OpenGL/GLX is usually provided by your GPU driver / mesa)
#
# Usage:
#   chmod +x build_x11.sh
#   ./build_x11.sh && ./build/x11

set -e
mkdir -p build
echo "Building x11..."
clang src/snake.c src/main_x11.c \
-o build/x11 \
-Wall -Wextra -O2 \
-lX11 -lxkbcommon -lGL -lGLX
echo "Done! Run with: ./build/x11"

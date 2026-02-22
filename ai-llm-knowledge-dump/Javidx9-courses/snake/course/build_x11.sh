#!/bin/bash
# build_x11.sh — Build the X11 version of Snake
#
# Prerequisites:
#   sudo apt install libx11-dev
#
# Usage:
#   chmod +x build_x11.sh
#   ./build_x11.sh && ./snake_x11

set -e
mkdir -p build
gcc -Wall -Wextra -g -o build/snake_x11 src/snake.c src/main_x11.c -lX11
echo "Done! Run: ./build/snake_x11"

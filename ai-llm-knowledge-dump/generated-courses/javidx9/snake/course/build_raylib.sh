#!/bin/bash
# build_raylib.sh — Build the Raylib version of Snake
#
# Prerequisites:
#   sudo apt install libraylib-dev   (Ubuntu 22.04+)
#   OR build from source: https://github.com/raysan5/raylib
#
# Usage:
#   chmod +x build_raylib.sh
#   ./build_raylib.sh && ./build/raylib

set -e
mkdir -p build
echo "Building raylib..."
clang src/snake.c src/main_raylib.c \
-o build/raylib \
-Wall -Wextra -O2 \
-lraylib -lm
echo "Done! Run with: ./build/raylib"

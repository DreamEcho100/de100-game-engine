#!/bin/bash
# build_raylib.sh — Build the Raylib version of Snake
#
# Prerequisites:
#   sudo apt install libraylib-dev   (Ubuntu 22.04+)
#   OR build from source: https://github.com/raysan5/raylib
#
# Usage:
#   chmod +x build_raylib.sh
#   ./build_raylib.sh && ./snake_raylib

set -e
mkdir -p build
gcc -Wall -Wextra -g -o build/snake_raylib src/snake.c src/main_raylib.c -lraylib -lm
echo "Done! Run: ./build/snake_raylib"

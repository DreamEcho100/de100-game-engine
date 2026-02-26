#!/bin/bash
set -e
mkdir -p build
clang src/main_raylib.c src/tetris.c -o build/raylib -lraylib -lm -lpthread -ldl
echo "Build OK -> ./build/raylib"

# ./build_raylib.sh && ./build/tetris_raylib;

#!/bin/bash
set -e
mkdir -p build
clang src/main_raylib.c -o build/tetris_raylib -lraylib -lm -lpthread -ldl
echo "Build OK -> ./build/tetris_raylib"

# ./build_raylib.sh && ./build/tetris_raylib;

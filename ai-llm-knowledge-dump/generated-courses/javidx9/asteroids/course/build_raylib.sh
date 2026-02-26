#!/usr/bin/env bash
# build_raylib.sh — Build asteroids with Raylib backend
set -e

mkdir -p build
echo "Building build/raylib..."

clang -std=c99 -Wall -Wextra -Wno-unused-parameter \
-g -O0 \
src/asteroids.c src/main_raylib.c \
-lraylib -lm \
-o build/raylib

echo "Done! Run with: ./build/raylib"

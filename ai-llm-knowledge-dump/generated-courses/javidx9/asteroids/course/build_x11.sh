#!/usr/bin/env bash
# build_x11.sh — Build asteroids with X11/OpenGL backend
set -e

mkdir -p build
echo "Building build/x11..."

clang -std=c99 -Wall -Wextra -Wno-unused-parameter \
-g -O0 \
src/asteroids.c src/main_x11.c \
-lX11 -lxkbcommon -lGL -lGLX -lm \
-o build/x11

echo "Done! Run with: ./build/x11"

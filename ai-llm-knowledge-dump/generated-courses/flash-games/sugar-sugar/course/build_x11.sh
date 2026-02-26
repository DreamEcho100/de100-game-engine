#!/usr/bin/env bash
# build_x11.sh — Build Sugar, Sugar with the X11 backend
# Usage:  ./build_x11.sh        (release)
#         ./build_x11.sh debug  (debug + sanitizers)

set -e  # exit on first error

mkdir -p build

SRC_DIR="src"
OUTPUT="build/x11"
SOURCES="$SRC_DIR/main_x11.c $SRC_DIR/game.c $SRC_DIR/levels.c"
LIBS="-lX11 -lm"

if [ "$1" = "debug" ]; then
    echo "Building DEBUG..."
    clang -Wall -Wextra \
    -O0 -g \f
    -fsanitize=address,undefined \
    -o "${OUTPUT}_dbg" \
    $SOURCES $LIBS
    echo "Done: ./${OUTPUT}_dbg"
else
    echo "Building RELEASE..."
    clang -Wall -Wextra \
    -O2 \
    -o "$OUTPUT" \
    $SOURCES $LIBS
    echo "Done: ./$OUTPUT"
    echo "Run: ./$OUTPUT"
fi

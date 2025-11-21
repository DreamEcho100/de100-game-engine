#!/bin/bash

set -e

mkdir -p build

BACKEND=$1

if [ -z "$BACKEND" ]; then
    BACKEND="raylib"
fi

echo "Building with backend: $BACKEND"

FLAGS="-Isrc -Wall -Wextra -std=c11"

SRC="src/main.c src/game/game.c"

if [ "$BACKEND" = "x11" ]; then
    FLAGS="$FLAGS -DUSE_X11 -lX11"
    SRC="$SRC src/platform/x11_backend.c"
elif [ "$BACKEND" = "x11-shm" ]; then
    FLAGS="$FLAGS -DUSE_X11 -lX11 -lXext"
    SRC="$SRC src/platform/x11_shm_backend.c"
elif [ "$BACKEND" = "x11-glx" ]; then
    FLAGS="$FLAGS -DUSE_X11 -lX11 -lGL"
    SRC="$SRC src/platform/x11_glx_backend.c"
elif [ "$BACKEND" = "raylib" ]; then
    FLAGS="$FLAGS -DUSE_RAYLIB -lraylib -lm -ldl -lpthread"
    SRC="$SRC src/platform/raylib_backend.c"
else
    echo "Unknown backend: $BACKEND"
    echo "Available backends: x11, x11-shm, x11-glx, raylib"
    exit 1
fi

clang $SRC -o build/game $FLAGS


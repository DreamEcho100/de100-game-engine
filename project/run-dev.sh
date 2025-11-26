#!/usr/bin/env bash

BACKEND=$1

if [ -z "$BACKEND" ]; then
    BACKEND="x11"
fi

echo "Running with backend: $BACKEND"

mkdir -p build

./build.sh "$BACKEND" && ./build/game

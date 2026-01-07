#!/bin/bash
set -e
mkdir -p build

BACKEND=$1
if [ -z "$BACKEND" ]; then
    BACKEND="x11"
fi

echo "Building with backend: $BACKEND"

# ═══════════════════════════════════════════════════════════════
# Debug build flags (NO OPTIMIZATION during development!)
# ═══════════════════════════════════════════════════════════════
# Core flags (match -Od -Oi -FC -Z7)
FLAGS="-Isrc"                    # Include path
FLAGS="$FLAGS -std=c11"          # C11 standard
FLAGS="$FLAGS -g"                # Debug symbols (-Z7)
FLAGS="$FLAGS -O0"               # Disable optimizations (-Od)
# FLAGS="$FLAGS -ftime-report"     # Time report for each compilation phase

# Warning flags (match -WX -W4)
FLAGS="$FLAGS -Werror"           # Warnings as errors (-WX)
FLAGS="$FLAGS -Wall"             # Enable all warnings (-W4)
FLAGS="$FLAGS -Wextra"           # Extra warnings (-W4)
# FLAGS="$FLAGS -Wno-unused-parameter"     # -wd4100
# FLAGS="$FLAGS -Wno-unused-variable"      # -wd4189

# Linker flags - Dead code elimination (match -opt:ref)
FLAGS="$FLAGS -ffunction-sections -fdata-sections"     # Separate code/data into sections for linker GC (-opt:ref)
FLAGS="$FLAGS -Wl,--gc-sections"                       # Remove unused code (-opt:ref)
FLAGS="$FLAGS -Wl,-Map=build/game.map"                 # Generate map file (-Fmwin32_handmade.map)

# Math library
FLAGS="$FLAGS -lm"

# Platform defines (match -DHANDMADE_*)
FLAGS="$FLAGS -DHANDMADE_INTERNAL=1"     # -DHANDMADE_INTERNAL=1
FLAGS="$FLAGS -DHANDMADE_SLOW=1"         # -DHANDMADE_SLOW=1

# Source files
SRC="src/main.c src/platform/_common/input.c src/platform/_common/backbuffer.c src/platform/_common/memory.c src/platform/_common/debug-file-io.c src/game.c"

# Backend-specific flags
if [ "$BACKEND" = "x11" ]; then
    FLAGS="$FLAGS -DUSE_X11 -lX11 -Wno-unused-parameter"
    SRC="$SRC src/platform/x11/backend.c src/platform/x11/audio.c"
elif [ "$BACKEND" = "raylib" ]; then
    FLAGS="$FLAGS -DUSE_RAYLIB -lraylib -lpthread"
    SRC="$SRC src/platform/raylib/backend.c src/platform/raylib/audio.c"
else
    echo "Unknown backend: $BACKEND"
    exit 1
fi

# Build
echo "Compiling with flags: $FLAGS"
clang $SRC -o build/game $FLAGS

# Success message
echo "Build complete: build/game"
if [ -f build/game.map ]; then
    echo "Map file: build/game.map"
fi
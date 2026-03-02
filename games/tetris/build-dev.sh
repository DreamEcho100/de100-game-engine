#!/bin/bash
set -e
mkdir -p build

BACKEND="raylib"
RUN_AFTER_BUILD=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --backend=*)
            BACKEND="${1#*=}"
        ;;
        -r|--run)
            RUN_AFTER_BUILD=true
        ;;
        --help|-h)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
        ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
        ;;
    esac
    shift
done

DE100_BACKEND_LIBS="-lm"
SOURCES="src/game.c src/utils/draw-shapes.c src/utils/draw-text.c"

case "$BACKEND" in
    x11)
        DE100_BACKEND_LIBS="$DE100_BACKEND_LIBS -lX11 -lxkbcommon -lGL -lGLX"
        SOURCES="$SOURCES src/main_x11.c"
    ;;
    raylib)
        case "$DE100_OS" in
            windows) DE100_BACKEND_LIBS="$DE100_BACKEND_LIBS -lraylib -lpthread -ldl" ;;
            macos)   DE100_BACKEND_LIBS="$DE100_BACKEND_LIBS -lraylib -lpthread -framework Cocoa -framework IOKit" ;;
            *)       DE100_BACKEND_LIBS="$DE100_BACKEND_LIBS -lraylib -lpthread -ldl" ;;
        esac
        SOURCES="$SOURCES src/main_raylib.c"
    ;;
    *)
        echo "Error: Unknown backend '$BACKEND'" >&2
        echo "Available: x11, raylib, auto" >&2
        exit 1
    ;;
esac

BINARY="game"
FLAGS="-Wall -Wextra -g -O0"
PLATFORM_EXE_PATH="./build/${BINARY}"

clang $FLAGS -o "$PLATFORM_EXE_PATH" $SOURCES $DE100_BACKEND_LIBS
echo "Done! Run with: $PLATFORM_EXE_PATH"

if [[ "$RUN_AFTER_BUILD" == true ]]; then
    echo ""
    echo "═══ Running Game ═══"
    echo ""
    exec "$PLATFORM_EXE_PATH"
fi

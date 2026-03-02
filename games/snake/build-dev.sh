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

BACKEND_LIBS="-lm"
SOURCES="src/game.c src/utils/draw-shapes.c src/utils/draw-text.c"


DETECTED_OS=""
case "$(uname -s)" in
    Linux*)               DETECTED_OS="linux" ;;
    Darwin*)              DETECTED_OS="macos" ;;
    FreeBSD*)             DETECTED_OS="freebsd" ;;
    MINGW*|MSYS*|CYGWIN*) DETECTED_OS="windows" ;;
    *)
        echo "Warning: Unknown OS '$DETECTED_OS', assuming POSIX-like" >&2
        echo "posix"
    ;;
esac

case "$BACKEND" in
    x11)
        BACKEND_LIBS="$BACKEND_LIBS -lX11 -lxkbcommon -lGL -lGLX"
        SOURCES="$SOURCES src/main_x11.c"
    ;;
    raylib)
        case "$DETECTED_OS" in
            windows) BACKEND_LIBS="$BACKEND_LIBS -lraylib -lpthread -ldl" ;;
            macos)   BACKEND_LIBS="$BACKEND_LIBS -lraylib -lpthread -framework Cocoa -framework IOKit" ;;
            *)       BACKEND_LIBS="$BACKEND_LIBS -lraylib -lpthread -ldl" ;;
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
FLAGS="-Wall -Wextra -g -O0 -DBACKEND=$BACKEND"
PLATFORM_EXE_PATH="./build/${BINARY}"

clang $FLAGS -o "$PLATFORM_EXE_PATH" $SOURCES $BACKEND_LIBS
echo "Done! Run with: $PLATFORM_EXE_PATH"

if [[ "$RUN_AFTER_BUILD" == true ]]; then
    echo ""
    echo "═══ Running Game ═══"
    echo ""
    exec "$PLATFORM_EXE_PATH"
fi

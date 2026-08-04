#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_DIR=${BUILD_DIR:-"$PROJECT_ROOT/build-lvgl-arm"}
CMAKE_C_COMPILER=${CMAKE_C_COMPILER:-arm-linux-gcc}
LVGL_ROOT_DIR=${LVGL_ROOT_DIR:-"$PROJECT_ROOT/../lvgl_project/lv_port_pc_vscode/lvgl"}
FREETYPE_ROOT_DIR=${FREETYPE_ROOT_DIR:-"$PROJECT_ROOT/../freetype/freetype_arm"}
JOBS=${JOBS:-2}

# Make the bundled ARM toolchain available when the terminal was not started
# from the toolchain environment.
ARM_TOOLCHAIN_BIN=${ARM_TOOLCHAIN_BIN:-/usr/local/arm/5.4.0/usr/bin}
if [ -d "$ARM_TOOLCHAIN_BIN" ]; then
    PATH="$ARM_TOOLCHAIN_BIN:$PATH"
    export PATH
fi

# Keep the original deployment targets, but allow local build-only use.
DEPLOY=${DEPLOY:-0}
BOARD_APP_ONLY=${BOARD_APP_ONLY:-root@192.168.33.60}
BOARD_FULL=${BOARD_FULL:-root@192.168.33.60}
REMOTE_ROOT=${REMOTE_ROOT:-/root/workspace}

require_command()
{
    if ! command -v "$1" >/dev/null 2>&1; then
        printf 'Missing command: %s\n' "$1" >&2
        printf 'Current PATH: %s\n' "$PATH" >&2
        exit 127
    fi
}

require_command bash
require_command convert
require_command cmake
require_command "$CMAKE_C_COMPILER"
if [ "$DEPLOY" = "1" ]; then
    require_command scp
fi

if [ "${CLEAN_BUILD:-0}" = "1" ]; then
    rm -rf -- "$BUILD_DIR"
fi

printf '%s\n' "[1/4] Generate weather assets"
"$PROJECT_ROOT/scripts/generate_weather_assets.sh"

printf '%s\n' "[2/4] Configure ARM build: $BUILD_DIR"
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
    -DCMAKE_C_COMPILER="$CMAKE_C_COMPILER" \
    -DLVGL_ROOT_DIR="$LVGL_ROOT_DIR" \
    -DFREETYPE_ROOT_DIR="$FREETYPE_ROOT_DIR"

printf '%s\n' "[3/4] Build application"
cmake --build "$BUILD_DIR" --parallel "$JOBS"

if [ "$DEPLOY" = "1" ]; then
    printf '%s\n' "[4/4] Deploy to boards"
    scp "$BUILD_DIR/myapp" "$BOARD_APP_ONLY:$REMOTE_ROOT/"
    scp -r "$PROJECT_ROOT/resource" "$BOARD_FULL:$REMOTE_ROOT/"
    scp "$BUILD_DIR/myapp" "$BOARD_FULL:$REMOTE_ROOT/"
else
    printf '%s\n' "[4/4] Deployment skipped (DEPLOY=$DEPLOY)"
fi

printf 'Done: %s/myapp\n' "$BUILD_DIR"

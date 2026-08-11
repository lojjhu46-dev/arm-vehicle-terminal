#!/bin/bash

#请重新清理旧构建并编译、部署：

#cd /home/gec/workspace/PROJECT_LVGL/Project
#CLEAN_BUILD=1 DEPLOY=1 ./64bit.make.sh

# Sourcing a build script must not apply `set -e` or `exit` to the caller's
# shell. Re-run it as a child process and only return its status to the caller.

#GENERATE_WEATHER_ASSETS=0 DEPLOY=0 BUILD_DIR=build JOBS=2 ./64bit.make.sh

if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
    _64BIT_MAKE_SCRIPT=${BASH_SOURCE[0]}
    case "$_64BIT_MAKE_SCRIPT" in
        */*) ;;
        *) _64BIT_MAKE_SCRIPT="./$_64BIT_MAKE_SCRIPT" ;;
    esac
    if "$_64BIT_MAKE_SCRIPT" "$@"; then
        _64BIT_MAKE_STATUS=0
    else
        _64BIT_MAKE_STATUS=$?
    fi
    unset _64BIT_MAKE_SCRIPT
    return "$_64BIT_MAKE_STATUS"
fi

set -Eeuo pipefail

SCRIPT_DIR=${0%/*}
if [ "$SCRIPT_DIR" = "$0" ]; then
    SCRIPT_DIR=.
fi
PROJECT_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR" && pwd)
WORKSPACE_ROOT=$(CDPATH= cd -- "$PROJECT_ROOT/../.." && pwd)

die()
{
    printf '[ERROR] %s\n' "$*" >&2
    exit 1
}

on_error()
{
    local status=$?

    printf '\n[ERROR] command failed with exit code %d\n' "$status" >&2
    printf '[ERROR] script: %s:%s\n' "${BASH_SOURCE[1]:-$0}" \
        "${BASH_LINENO[0]:-?}" >&2
    printf '[ERROR] command: %s\n' "$BASH_COMMAND" >&2
    exit "$status"
}
trap on_error ERR

prepend_path()
{
    local directory=$1

    [ -d "$directory" ] || return 0
    case ":${PATH:-}:" in
        *":$directory:"*) ;;
        *) PATH="$directory${PATH:+:$PATH}" ;;
    esac
}

# A terminal task may start with an incomplete PATH. Add the common system
# directories before looking up any command; the shebang above also avoids
# requiring env(1) to locate Bash.
PATH=${PATH:-}
ARM_TOOLCHAIN_ROOT=${ARM_TOOLCHAIN_ROOT:-/usr/local/arm/5.4.0}
ARM_TOOLCHAIN_BIN=${ARM_TOOLCHAIN_BIN:-"$ARM_TOOLCHAIN_ROOT/usr/bin"}
prepend_path "$ARM_TOOLCHAIN_BIN"
for system_path in /usr/local/sbin /usr/local/bin /usr/sbin /usr/bin /sbin /bin; do
    prepend_path "$system_path"
done
export PATH

resolve_executable()
{
    local requested=$1
    local description=$2
    local resolved

    if [[ "$requested" == */* ]]; then
        [ -x "$requested" ] || die "$description is not executable: $requested"
        printf '%s\n' "$requested"
        return 0
    fi
    resolved=$(command -v "$requested" 2>/dev/null || true)
    [ -n "$resolved" ] && [ -x "$resolved" ] || \
        die "Cannot find $description '$requested' (PATH=$PATH)"
    printf '%s\n' "$resolved"
}

make_project_path()
{
    case "$1" in
        /*) printf '%s\n' "$1" ;;
        *) printf '%s/%s\n' "$PROJECT_ROOT" "$1" ;;
    esac
}

validate_flag()
{
    local name=$1
    local value=$2

    case "$value" in
        0|1) ;;
        *) die "$name must be 0 or 1: $value" ;;
    esac
}

usage()
{
    cat <<'EOF'
Usage: ./64bit.make.sh

Environment overrides:
  BUILD_DIR=...                  ARM build directory
  CLEAN_BUILD=1                  Remove BUILD_DIR before configuring
  GENERATE_WEATHER_ASSETS=0      Skip ImageMagick weather asset generation
  JOBS=2                         Parallel build jobs
  DEPLOY=1                       Enable the existing SCP deployment steps
  ARM_TOOLCHAIN_BIN=...          Directory containing arm-linux-gcc/c++
  CMAKE_C_COMPILER=...           C compiler command or absolute path
  CMAKE_CXX_COMPILER=...         C++ compiler command or absolute path
  LVGL_ROOT_DIR=...              LVGL 9 source directory
  FREETYPE_ROOT_DIR=...          ARM FreeType SDK directory
  LV_BUILD_CONF_PATH=...         LVGL configuration header
EOF
}

case "${1:-}" in
    '' ) ;;
    -h|--help ) usage; exit 0 ;;
    * ) die "Unknown argument '$1'; use --help for usage" ;;
esac

BUILD_DIR=$(make_project_path "${BUILD_DIR:-$PROJECT_ROOT/build-lvgl-arm}")
LVGL_ROOT_DIR=$(make_project_path \
    "${LVGL_ROOT_DIR:-$WORKSPACE_ROOT/lvgl_project/lv_port_pc_vscode/lvgl}")
FREETYPE_ROOT_DIR=$(make_project_path \
    "${FREETYPE_ROOT_DIR:-$WORKSPACE_ROOT/freetype/freetype_arm}")
LV_BUILD_CONF_PATH=$(make_project_path \
    "${LV_BUILD_CONF_PATH:-$PROJECT_ROOT/lv_conf.h}")

if [ -z "${CMAKE_C_COMPILER:-}" ]; then
    if [ -x "$ARM_TOOLCHAIN_BIN/arm-linux-gcc" ]; then
        CMAKE_C_COMPILER="$ARM_TOOLCHAIN_BIN/arm-linux-gcc"
    else
        CMAKE_C_COMPILER=arm-linux-gcc
    fi
fi
if [ -z "${CMAKE_CXX_COMPILER:-}" ]; then
    if [ -x "$ARM_TOOLCHAIN_BIN/arm-linux-c++" ]; then
        CMAKE_CXX_COMPILER="$ARM_TOOLCHAIN_BIN/arm-linux-c++"
    else
        CMAKE_CXX_COMPILER=arm-linux-c++
    fi
fi

BASH_BIN=$(resolve_executable "${BASH_BIN:-/bin/bash}" Bash)
CMAKE_BIN=$(resolve_executable "${CMAKE_BIN:-cmake}" CMake)
C_COMPILER_PATH=$(resolve_executable "$CMAKE_C_COMPILER" "ARM C compiler")
CXX_COMPILER_PATH=$(resolve_executable "$CMAKE_CXX_COMPILER" "ARM C++ compiler")

GENERATE_WEATHER_ASSETS=${GENERATE_WEATHER_ASSETS:-1}
CLEAN_BUILD=${CLEAN_BUILD:-0}
DEPLOY=${DEPLOY:-0}
JOBS=${JOBS:-2}
validate_flag GENERATE_WEATHER_ASSETS "$GENERATE_WEATHER_ASSETS"
validate_flag CLEAN_BUILD "$CLEAN_BUILD"
validate_flag DEPLOY "$DEPLOY"
case "$JOBS" in
    ''|*[!0-9]*|0) die "JOBS must be a positive integer: $JOBS" ;;
esac

BOARD_APP_ONLY=${BOARD_APP_ONLY:-root@192.168.33.60}
BOARD_FULL=${BOARD_FULL:-root@192.168.33.60}
REMOTE_ROOT=${REMOTE_ROOT:-/root/workspace}

[ -d "$PROJECT_ROOT/src" ] || die "Project source directory is missing: $PROJECT_ROOT/src"
[ -d "$LVGL_ROOT_DIR" ] || die "LVGL directory is missing: $LVGL_ROOT_DIR"
[ -f "$LVGL_ROOT_DIR/lvgl.h" ] || die "LVGL header is missing: $LVGL_ROOT_DIR/lvgl.h"
[ -f "$LV_BUILD_CONF_PATH" ] || die "LVGL configuration is missing: $LV_BUILD_CONF_PATH"
[ -f "$FREETYPE_ROOT_DIR/include/ft2build.h" ] || \
    die "FreeType headers are missing under: $FREETYPE_ROOT_DIR"
[ -f "$FREETYPE_ROOT_DIR/lib/libfreetype.a" ] || \
    die "FreeType ARM library is missing under: $FREETYPE_ROOT_DIR"
[ -f "$PROJECT_ROOT/resource/fonts/SIMHEI.TTF" ] || \
    die "UI font is missing: $PROJECT_ROOT/resource/fonts/SIMHEI.TTF"
[ -f "$PROJECT_ROOT/scripts/generate_weather_assets.sh" ] || \
    die 'Weather asset generator is missing'

if [ "$GENERATE_WEATHER_ASSETS" = 1 ]; then
    CONVERT_BIN=$(resolve_executable convert 'ImageMagick convert')
else
    CONVERT_BIN=''
fi
if [ "$DEPLOY" = 1 ]; then
    SCP_BIN=$(resolve_executable scp SCP)
else
    SCP_BIN=''
fi

if [ "$CLEAN_BUILD" = 1 ]; then
    printf '%s\n' "[clean] Removing build directory: $BUILD_DIR"
    rm -rf -- "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"

if [ "$GENERATE_WEATHER_ASSETS" = 1 ]; then
    printf '%s\n' "[1/4] Generate weather assets ($CONVERT_BIN)"
    # The helper resolves convert through PATH. CONVERT_BIN is resolved above
    # so the prerequisite failure is reported before any source is changed.
    "$BASH_BIN" "$PROJECT_ROOT/scripts/generate_weather_assets.sh"
else
    printf '%s\n' '[1/4] Weather asset generation skipped'
fi

printf '%s\n' "[2/4] Configure ARM build: $BUILD_DIR"
cmake_args=(
    -S "$PROJECT_ROOT"
    -B "$BUILD_DIR"
    "-DCMAKE_C_COMPILER=$C_COMPILER_PATH"
    "-DCMAKE_CXX_COMPILER=$CXX_COMPILER_PATH"
    "-DLVGL_ROOT_DIR=$LVGL_ROOT_DIR"
    "-DFREETYPE_ROOT_DIR=$FREETYPE_ROOT_DIR"
    "-DLV_BUILD_CONF_PATH=$LV_BUILD_CONF_PATH"
)
"$CMAKE_BIN" "${cmake_args[@]}"

printf '%s\n' "[3/4] Build application with $JOBS job(s)"
"$CMAKE_BIN" --build "$BUILD_DIR" --parallel "$JOBS"
[ -x "$BUILD_DIR/myapp" ] || die "Build finished without executable: $BUILD_DIR/myapp"

if [ "$DEPLOY" = 1 ]; then
    printf '%s\n' '[4/4] Deploy to boards'
    "$SCP_BIN" "$BUILD_DIR/myapp" "$BOARD_APP_ONLY:$REMOTE_ROOT/"
    "$SCP_BIN" -r "$PROJECT_ROOT/resource" "$BOARD_FULL:$REMOTE_ROOT/"
    "$SCP_BIN" "$BUILD_DIR/myapp" "$BOARD_FULL:$REMOTE_ROOT/"
else
    printf '%s\n' '[4/4] Deployment skipped (DEPLOY=0)'
fi

printf 'Done: %s/myapp\n' "$BUILD_DIR"

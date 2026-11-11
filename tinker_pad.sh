#!/usr/bin/env bash
# Build and load tinker_pad onto the RP2350 USB stick.
#
#   ./tinker_pad.sh            build, then load onto the board (the default)
#   ./tinker_pad.sh help       show this help
#   ./tinker_pad.sh clean      remove the build folder
#   ./tinker_pad.sh configure  run cmake
#   ./tinker_pad.sh build      compile the firmware (.uf2 and .elf)
#   ./tinker_pad.sh reset      put the stick in bootloader mode (1200 baud reset)
#   ./tinker_pad.sh flash      copy the .uf2 to the RPI-RP2 drive
#   ./tinker_pad.sh load       reset, wait for the drive, then flash
#   ./tinker_pad.sh all        clean, configure, build, then load
#
# Settings can be overridden with environment variables:
#   PICO_SDK_PATH  (default /work/pico/pico-sdk-2.3.1)
#   PICO_BOARD     (default pico2)
#   SERIAL_PORT    (default /dev/ttyACM0)
#   DRIVE          (default /run/media/$USER/RP2350)
#   BUILD_DIR      (default b2350)
#   PICOTOOL_FETCH_FROM_GIT_PATH  (default ~/.cache/tinker_pad/picotool)

set -euo pipefail

cd "$(dirname "$(readlink -f "$0")")"

PICO_SDK_PATH="${PICO_SDK_PATH:-/work/pico/pico-sdk-2.3.1}"
PICO_BOARD="${PICO_BOARD:-pico2}"
SERIAL_PORT="${SERIAL_PORT:-/dev/ttyACM0}"
DRIVE="${DRIVE:-/run/media/${USER:-skb}/RP2350}"
BUILD_DIR="${BUILD_DIR:-b2350}"
UF2="$BUILD_DIR/src/tinker_pad.uf2"
# picotool is cloned once into a shared folder, so 'clean' doesn't fetch it again.
PICOTOOL_FETCH_FROM_GIT_PATH="${PICOTOOL_FETCH_FROM_GIT_PATH:-${XDG_CACHE_HOME:-$HOME/.cache}/tinker_pad/picotool}"
export PICO_SDK_PATH PICOTOOL_FETCH_FROM_GIT_PATH

say() { printf '==> %s\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

usage() {
    sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'
}

do_clean() {
    say "removing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
}

do_configure() {
    say "configuring $BUILD_DIR for $PICO_BOARD"
    cmake -S . -B "$BUILD_DIR" -DPICO_BOARD="$PICO_BOARD" >/dev/null
}

do_build() {
    [ -f "$BUILD_DIR/CMakeCache.txt" ] || do_configure
    say "building"
    cmake --build "$BUILD_DIR" -j"$(nproc)"
    [ -f "$UF2" ] || die "build finished but $UF2 is missing"
    say "built $UF2"
}

do_reset() {
    [ -e "$SERIAL_PORT" ] || die "no serial port $SERIAL_PORT: is the stick plugged in and running the app?"
    say "resetting $SERIAL_PORT into bootloader mode"
    stty -F "$SERIAL_PORT" 1200
}

do_wait_drive() {
    say "waiting for $DRIVE"
    for _ in $(seq 1 15); do
        [ -d "$DRIVE" ] && return 0
        sleep 1
    done
    die "$DRIVE did not appear. Hold BOOT while plugging the stick in, then run: $0 flash"
}

do_flash() {
    [ -f "$UF2" ] || die "$UF2 not found: run '$0 build' first"
    [ -d "$DRIVE" ] || die "$DRIVE is not mounted: put the stick in bootloader mode first"
    local size
    size=$(stat -c %s "$UF2")
    say "copying $UF2 ($size bytes) to $DRIVE"
    if command -v pv >/dev/null 2>&1; then
        pv -s "$size" "$UF2" > "$DRIVE/$(basename "$UF2")"
    else
        dd if="$UF2" of="$DRIVE/$(basename "$UF2")" bs=64K status=progress conv=fsync
    fi
    say "syncing the drive (the stick reboots when this finishes)"
    sync
    say "flashed. The stick reboots into tinker_pad."
}

do_load() {
    do_reset
    do_wait_drive
    do_flash
}

case "${1:-}" in
    "")        do_build; do_load ;;
    help|-h|--help) usage ;;
    clean)     do_clean ;;
    configure) do_configure ;;
    build)     do_build ;;
    reset)     do_reset ;;
    flash)     do_flash ;;
    load)      do_load ;;
    all)       do_clean; do_configure; do_build; do_load ;;
    *)         printf 'unknown command: %s\n\n' "$1" >&2; usage >&2; exit 2 ;;
esac

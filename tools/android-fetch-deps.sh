#!/bin/bash
# Fetch SDL2 and SDL2_image for the Android test APK into android/deps at pinned commits
# (zlib licence; sources are fetched for the build, not committed).
set -euo pipefail
DEPS=$(cd "$(dirname "$0")/.." && pwd)/android/deps
mkdir -p "$DEPS"

fetch() {  # name tag commit
    local dir=$DEPS/$1
    rm -rf "$dir"
    git clone -q --depth 1 --branch "$2" "https://github.com/libsdl-org/$1.git" "$dir"
    local head; head=$(git -C "$dir" rev-parse HEAD)
    [ "$head" = "$3" ] || { echo "$1 $2: expected $3, got $head"; exit 1; }
    echo "$1 $2 $head"
}

fetch SDL release-2.32.10 5d249570393f7a37e037abf22cd6012a4cc56a71
fetch SDL_image release-2.8.8 c1bf2245b0ba63a25afe2f8574d305feca25af77

# Local changes to SDL, kept as patches so they are reapplied when SDL is updated:
#   sdl-text-input-keys.patch  phone keyboard: only characters the TG50 keyboard types are sent as
#                              keys and deleted with backspace (Hangul composition deleted text)
for p in "$DEPS/../patches"/sdl-*.patch; do
    git -C "$DEPS/SDL" apply "$p"
    echo "applied $(basename "$p")"
done

# Only SDL's Java sources are used from its sample project; drop its Gradle wrapper (an unused
# binary jar that the wrapper validation would reject).
rm -rf "$DEPS/SDL/android-project/gradle" "$DEPS/SDL/android-project/gradlew" "$DEPS/SDL/android-project/gradlew.bat"

#!/bin/bash
# Build check only: compile the OS 5 (uARM) emulator core with the Android NDK for arm64-v8a and
# link it into a shared library with no undefined symbols. Not an app: no SDL, no console
# (readline), no NetLib host backend, no ROM. Sources come from the existing Makefiles
# (src/uarm SOURCE_C/SOURCE_CXX, src/common SOURCE_C/SOURCE_CXX), so nothing is listed twice.
#
# usage: ANDROID_NDK_HOME=<ndk> tools/android-core-check.sh [out-dir]
set -euo pipefail

NDK=${ANDROID_NDK_HOME:?set ANDROID_NDK_HOME}
API=${ANDROID_API:-26}
OUT=${1:-build-android-core}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
TC=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin
CC="$TC/clang --target=aarch64-linux-android$API"
CXX="$TC/clang++ --target=aarch64-linux-android$API"

mkdir -p "$OUT/obj"
OUT=$(cd "$OUT" && pwd)

# Variable from a Makefile, with line continuations and $(...) references removed.
make_list() {
    python3 - "$1" "$2" <<'EOF'
import re, sys
text = open(sys.argv[1]).read()
m = re.search(r'^' + sys.argv[2] + r'\s*=\s*((?:.*\\\n)*.*)', text, re.M)
print("\n".join(i for i in re.sub(r'\\\n', ' ', m.group(1)).split() if not i.startswith('$(')))
EOF
}

SOURCES=()
for v in SOURCE_C SOURCE_CXX; do
    while read -r f; do SOURCES+=("src/uarm/$f"); done < <(make_list "$ROOT/src/uarm/Makefile" $v)
    while read -r f; do SOURCES+=("src/common/$f"); done < <(make_list "$ROOT/src/common/Makefile" $v)
done

# Warning flags and C standards as in src/uarm/Makefile and src/common/Makefile (gnu11 / gnu99,
# C++17); warnings are reported, not fatal.
WARN="-Wextra -Wall -Wno-unused-parameter -Wno-unknown-pragmas -Wno-visibility
      -Wno-constant-conversion -Wno-constant-logical-operand"
INC="-I$ROOT/src/uarm/uarm -I$ROOT/src/uarm -I$ROOT/src/common -I$ROOT/src/common/zip
     -I$ROOT/src/argparse -I$ROOT/src/cloudpilot/nanopb -I$ROOT/src/cloudpilot/proto"
FLAGS="-fPIC -O2 -D_FILE_OFFSET_BITS=64 $WARN $INC"

: > "$OUT/compile.log"
fail=0; i=0
for f in "${SOURCES[@]}"; do
    i=$((i + 1))
    obj="$OUT/obj/$(printf '%03d' $i)_$(basename "$f").o"
    case "$f" in
        src/uarm/*.c) cmd="$CC -std=gnu11 $FLAGS -c $ROOT/$f -o $obj" ;;
        *.c) cmd="$CC -std=gnu99 $FLAGS -c $ROOT/$f -o $obj" ;;
        *) cmd="$CXX -std=c++17 $FLAGS -c $ROOT/$f -o $obj" ;;
    esac
    echo "== $f" >> "$OUT/compile.log"
    if ! $cmd >> "$OUT/compile.log" 2>&1; then
        echo "FAILED: $f"; fail=$((fail + 1))
    fi
done

echo "sources: ${#SOURCES[@]} (src/uarm and src/common), failed: $fail"
echo "warnings: $(grep -c 'warning:' "$OUT/compile.log" || true)"
grep 'warning:' "$OUT/compile.log" | sed 's/.*warning: //' | sort | uniq -c | sort -rn | head -20 || true
[ "$fail" = 0 ] || exit 1

# Shared library from all objects (no archive, so every object is linked), no undefined symbols.
$CXX -shared -o "$OUT/libuarmcore.so" "$OUT"/obj/*.o -static-libstdc++ \
    -Wl,--no-undefined -Wl,-z,defs 2>&1 | tee "$OUT/link.log"

READELF=$TC/llvm-readelf
NM=$TC/llvm-nm
echo "library: $(stat -c %s "$OUT/libuarmcore.so") bytes"
$READELF -h "$OUT/libuarmcore.so" | grep -E 'Class|Machine'
echo "needed: $($READELF -d "$OUT/libuarmcore.so" | grep NEEDED | sed 's/.*\[\(.*\)\]/\1/' | tr '\n' ' ')"
$NM -D --undefined-only "$OUT/libuarmcore.so" | awk '{print $NF}' > "$OUT/undefined.txt"
$NM -DC --defined-only "$OUT/libuarmcore.so" > "$OUT/exports.txt"
echo "undefined (resolved by the NEEDED system libraries): $(wc -l < "$OUT/undefined.txt")"

# Every object must define at least one symbol (so nothing compiled to an empty object), and
# core entry points must be present in the library.
empty=0
for o in "$OUT"/obj/*.o; do
    [ -n "$($NM --defined-only "$o" | grep -E ' [TtDdBbRr] ')" ] || { echo "empty object: $o"; empty=$((empty + 1)); }
done
echo "objects linked: $(ls "$OUT"/obj/*.o | wc -l), objects without symbols: $empty"
miss=0
for s in 'deviceSetup(' 'cpuInit(' 'MainLoop::Cycle(' 'sonyTG50Touch(' 'ad7873PenInput(' \
         'palmEpochSeconds(' 'SessionFile5::Deserialize(' 'tg50ucInit('; do
    if grep -qF " T $s" "$OUT/exports.txt"; then echo "  exported: $s"
    else echo "  MISSING: $s"; miss=$((miss + 1)); fi
done
[ "$empty" = 0 ] && [ "$miss" = 0 ]

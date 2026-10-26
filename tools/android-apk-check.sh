#!/bin/bash
# Static checks on the Android test APK (no device, no ROM): manifest SDK levels, ABI, native
# libraries stored uncompressed and 16 KiB aligned (ZIP and ELF), core entry points in libmain.so,
# memfd_create binding, no ROM-like files.
#
# usage: ANDROID_HOME=<sdk> ANDROID_NDK_HOME=<ndk> tools/android-apk-check.sh <apk> <work-dir>
set -uo pipefail

APK=$(realpath "$1")
WORK=$(realpath -m "$2")
BT=$ANDROID_HOME/build-tools/35.0.0
TC=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin
LIBS="libSDL2.so libSDL2_image.so libmain.so"

fail=0
ok() { echo "OK    $*"; }
bad() { echo "FAIL  $*"; fail=1; }

rm -rf "$WORK"; mkdir -p "$WORK/lib"
echo "apk: $(stat -c %s "$APK") bytes"

# Manifest
"$BT/aapt2" dump badging "$APK" > "$WORK/badging.txt"
grep -E "^(package|minSdkVersion|sdkVersion|targetSdkVersion|native-code|launchable-activity)" "$WORK/badging.txt"
grep -qE "^(minSdkVersion|sdkVersion):'26'$" "$WORK/badging.txt" && ok "minSdk 26" || bad "minSdk is not 26"
grep -qx "targetSdkVersion:'35'" "$WORK/badging.txt" && ok "targetSdk 35" || bad "targetSdk is not 35"
grep -qx "native-code: 'arm64-v8a'" "$WORK/badging.txt" && ok "native code arm64-v8a only" \
    || bad "native-code is not arm64-v8a only"

# APK entries: lib/ contents, compression and data offset alignment (independent of zipalign).
python3 - "$APK" "$LIBS" <<'EOF' || fail=1
import sys, zipfile, struct
apk, expected = sys.argv[1], sys.argv[2].split()
bad = 0
with zipfile.ZipFile(apk) as z, open(apk, 'rb') as f:
    names = z.namelist()
    libs = sorted(n for n in names if n.startswith('lib/'))
    print('lib entries:', ' '.join(libs))
    want = sorted('lib/arm64-v8a/' + l for l in expected)
    if libs != want:
        print('FAIL  lib/ entries differ from', ' '.join(want)); bad = 1
    else:
        print('OK    lib/ holds exactly', ' '.join(expected))
    for info in z.infolist():
        if not info.filename.endswith('.so'):
            continue
        f.seek(info.header_offset)
        h = f.read(30)
        name_len, extra_len = struct.unpack('<HH', h[26:30])
        data = info.header_offset + 30 + name_len + extra_len
        stored = info.compress_type == zipfile.ZIP_STORED
        aligned = data % 16384 == 0
        print('%s  %s stored=%s data_offset=0x%x aligned16k=%s' % ('OK   ' if stored and aligned else 'FAIL ',
              info.filename, stored, data, aligned))
        bad |= not (stored and aligned)
    roms = [n for n in names if n.lower().endswith(('.rom', '.nor', '.img', '.ses'))]
    if roms:
        print('FAIL  ROM-like entries:', roms); bad = 1
    else:
        print('OK    no ROM-like entries in the APK')
sys.exit(bad)
EOF

if "$BT/zipalign" -c -P 16 -v 4 "$APK" > "$WORK/zipalign.txt" 2>&1; then
    ok "zipalign -c -P 16 -v 4"
else
    bad "zipalign -c -P 16 -v 4"; grep -v '(OK' "$WORK/zipalign.txt" | tail -20
fi

# ELF checks on each library
(cd "$WORK/lib" && unzip -q -j "$APK" 'lib/arm64-v8a/*.so')
for lib in $LIBS; do
    so=$WORK/lib/$lib
    [ -f "$so" ] || { bad "$lib missing"; continue; }
    machine=$("$TC/llvm-readelf" -h "$so" | awk -F: '/Machine/ {gsub(/^ +/, "", $2); print $2}')
    aligns=$("$TC/llvm-readelf" -lW "$so" | awk '$1 == "LOAD" {print $NF}' | sort -u | tr '\n' ' ')
    needed=$("$TC/llvm-readelf" -d "$so" | sed -n 's/.*NEEDED.*\[\(.*\)\]/\1/p' | tr '\n' ' ')
    echo "$lib: $(stat -c %s "$so") bytes, $machine, LOAD align $aligns, needed $needed"
    [ "$machine" = "AArch64" ] || bad "$lib is not AArch64"
    [ "$aligns" = "0x4000 " ] && ok "$lib LOAD segments 16 KiB aligned" || bad "$lib LOAD alignment $aligns"
done

# Core entry points linked into libmain.so
"$TC/llvm-nm" -DC --defined-only "$WORK/lib/libmain.so" > "$WORK/exports.txt"
for s in 'SDL_main' 'deviceSetup(' 'cpuInit(' 'MainLoop::Cycle(' 'sonyTG50Touch(' 'ad7873PenInput(' \
         'palmEpochSeconds(' 'SessionFile5::Deserialize(' 'tg50ucInit(' 'RomInfo5::RomInfo5(' \
         'SocPXA::SocPXA(' 'SdlRenderer::Draw(' 'SdlEventHandler::HandleEvents(' 'SdlAudioDriver::Start('; do
    grep -qF " T $s" "$WORK/exports.txt" && ok "libmain.so exports $s" || bad "libmain.so lacks $s"
done

# memfd_create: binding in each library, the libc++ object that references it, and the code
# around each call (to see whether the caller checks for a null weak symbol first).
echo "--- memfd_create"
for lib in $LIBS; do
    "$TC/llvm-readelf" --dyn-syms -W "$WORK/lib/$lib" | awk -v l="$lib" '$NF ~ /^memfd_create/ {print l": type "$4", bind "$5", ndx "$7", "$NF}'
done
for lib in $LIBS; do
    echo "$lib relocations: $("$TC/llvm-readelf" -rW "$WORK/lib/$lib" | grep -w memfd_create | awk '{print $3}' | sort | uniq -c | tr '\n' ' ')"
done
# Which NDK static library or object references it (aarch64 runtime libraries).
find "$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64" -name '*.a' \( -path '*aarch64*' -o -path '*/lib/clang/*' \) |
while read -r a; do
    "$TC/llvm-nm" -A "$a" 2>/dev/null | grep -w memfd_create | grep -v -E '/(x86|i686|arm-|riscv)' \
        | sed "s|$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/||"
done | head -10
"$TC/llvm-objdump" -d --no-show-raw-insn "$WORK/lib/libmain.so" > "$WORK/libmain.dis"
grep -n 'memfd_create' "$WORK/libmain.dis" | head -5
grep -n -E 'bl\s.*<memfd_create@plt>' "$WORK/libmain.dis" | cut -d: -f1 | while read -r n; do
    fn=$(head -n "$n" "$WORK/libmain.dis" | grep -E '^[0-9a-f]+ <' | tail -1)
    echo "call at line $n in $fn"
    sed -n "$((n > 14 ? n - 14 : 1)),$((n + 2))p" "$WORK/libmain.dis"
done

echo "--- result: $([ "$fail" = 0 ] && echo pass || echo FAIL)"
exit $fail

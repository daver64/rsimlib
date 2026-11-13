#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$HOME/Android/Sdk}}"
AAPT="$ANDROID_SDK_ROOT/build-tools/36.0.0/aapt"

check_app() {
    local name="$1" id="$2" label="$3" apk="$ROOT_DIR/build/android/apks/$1.apk"
    local badging listing
    badging="$("$AAPT" dump badging "$apk")"
    listing="$(unzip -Z1 "$apk")"
    if ! grep -Fq "package: name='$id' " <<< "$badging" ||
       ! grep -Fq "label='$label'" <<< "$badging"; then
        echo "$name: incorrect application ID or label." >&2
        exit 1
    fi
    for abi in arm64-v8a armeabi-v7a x86_64; do
        if ! grep -Fxq "lib/$abi/libmain.so" <<< "$listing"; then
            echo "$name: missing native program for $abi." >&2
            exit 1
        fi
    done
}

check_app exasteroids com.daver64.simlib.exasteroids exasteroids
check_app showcase com.daver64.simlib.android "simlib Android Showcase"

asteroids_main="$(unzip -p "$ROOT_DIR/build/android/apks/exasteroids.apk" lib/x86_64/libmain.so | sha256sum)"
showcase_main="$(unzip -p "$ROOT_DIR/build/android/apks/showcase.apk" lib/x86_64/libmain.so | sha256sum)"
if [[ "$asteroids_main" == "$showcase_main" ]]; then
    echo "The two APKs contain the same native program." >&2
    exit 1
fi
echo "Android APK identities, labels, ABIs and native programs are distinct."

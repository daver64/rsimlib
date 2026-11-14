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

hashes=()
for properties in "$ROOT_DIR"/android/apps/*/app.properties; do
    name="$(basename "$(dirname "$properties")")"
    [[ -f "$ROOT_DIR/build/android/apks/$name.apk" ]] || continue
    id="$(sed -n 's/^applicationId=//p' "$properties")"
    label="$(sed -n 's/^label=//p' "$properties")"
    check_app "$name" "$id" "$label"
    hashes+=("$(unzip -p "$ROOT_DIR/build/android/apks/$name.apk" lib/x86_64/libmain.so | sha256sum)")
done
if (( ${#hashes[@]} == 0 )); then
    echo "No built APKs found in build/android/apks." >&2
    exit 1
fi
if [[ "$(printf '%s\n' "${hashes[@]}" | sort | uniq -d)" != "" ]]; then
    echo "Two APKs contain the same native program." >&2
    exit 1
fi
echo "Android APK identities, labels, ABIs and native programs are distinct (${#hashes[@]} APKs)."

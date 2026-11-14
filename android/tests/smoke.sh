#!/usr/bin/env bash
# Installs and launches built APKs on the connected device or emulator, then reports which ones stay running.
# Usage: android/tests/smoke.sh [app...]   (default: every APK in build/android/apks)
# Set ANDROID_SERIAL to pick a device and SMOKE_SECONDS to change the run time (default 6).
# Screenshots are written to build/android/smoke/<app>.png.
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$HOME/Android/Sdk}}"
ADB="$ANDROID_SDK_ROOT/platform-tools/adb"
SECONDS_TO_RUN="${SMOKE_SECONDS:-6}"
OUT_DIR="$ROOT_DIR/build/android/smoke"
mkdir -p "$OUT_DIR"

if (( $# == 0 )); then
    shopt -s nullglob
    apks=("$ROOT_DIR"/build/android/apks/*.apk)
    set --
    for apk in "${apks[@]}"; do
        name="${apk##*/}"
        set -- "$@" "${name%.apk}"
    done
fi

failed=()
for app in "$@"; do
    apk="$ROOT_DIR/build/android/apks/$app.apk"
    id="$(sed -n 's/^applicationId=//p' "$ROOT_DIR/android/apps/$app/app.properties")"
    if [[ ! -f "$apk" || -z "$id" ]]; then
        echo "$app: no APK or app.properties"
        failed+=("$app")
        continue
    fi
    "$ADB" install -r "$apk" > /dev/null || { echo "$app: install failed"; failed+=("$app"); continue; }
    "$ADB" logcat -c
    "$ADB" shell am start -n "$id/com.daver64.simlib.android.SimlibActivity" > /dev/null
    sleep "$SECONDS_TO_RUN"
    if [[ -z "$("$ADB" shell pidof "$id" | tr -d '\r')" ]]; then
        echo "$app: NOT RUNNING"
        "$ADB" logcat -d -s AndroidRuntime:E SDL:E SDL/APP:E simlib:E DEBUG:F | tail -6
        failed+=("$app")
    else
        echo "$app: running"
        for _ in 1 2 3; do
            "$ADB" exec-out screencap -p > "$OUT_DIR/$app.png" || true
            [ -s "$OUT_DIR/$app.png" ] && break
            sleep 1
        done
    fi
    "$ADB" shell am force-stop "$id"
    "$ADB" uninstall "$id" > /dev/null
done

if (( ${#failed[@]} )); then
    echo "Not running: ${failed[*]}"
    exit 1
fi

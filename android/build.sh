#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$HOME/Android/Sdk}}"
ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-$ANDROID_SDK_ROOT/ndk/29.0.14206865}"
VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg-mingw}"
BUILD_JOBS="${ANDROID_BUILD_JOBS:-2}"

if (( $# == 0 )); then
    shopt -s nullglob
    app_configs=("$ROOT_DIR"/android/apps/*/app.cmake)
    if (( ${#app_configs[@]} == 0 )); then
        echo "No Android apps are registered in android/apps." >&2
        exit 1
    fi
    set -- "${app_configs[@]%/app.cmake}"
    apps=()
    for app_dir in "$@"; do
        apps+=("${app_dir##*/}")
    done
else
    apps=("$@")
fi

for app in "${apps[@]}"; do
    if [[ ! "$app" =~ ^[a-z][a-z0-9_]*$ ]] ||
       [[ ! -f "$ROOT_DIR/android/apps/$app/app.cmake" ]] ||
       [[ ! -f "$ROOT_DIR/android/apps/$app/app.properties" ]]; then
        echo "Unknown Android app: $app (expected app.cmake and app.properties in android/apps/$app)." >&2
        exit 1
    fi
done

if [[ -n "${JAVA_HOME:-}" ]]; then
    javac_bin="$JAVA_HOME/bin/javac"
else
    javac_bin="$(command -v javac || true)"
fi
if [[ -z "$javac_bin" || ! -x "$javac_bin" ]]; then
    echo "A full JDK (Java 17 or newer, including javac) is required; a JRE is not enough." >&2
    echo "Install one (for example: sudo apt install openjdk-21-jdk-headless) or set JAVA_HOME to a JDK." >&2
    exit 1
fi

if [[ ! -x "$ANDROID_SDK_ROOT/cmake/3.31.6/bin/cmake" ]]; then
    echo "Android CMake 3.31.6 is missing from $ANDROID_SDK_ROOT" >&2
    exit 1
fi
if [[ ! -f "$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" ]]; then
    echo "Android NDK 29.0.14206865 is missing from $ANDROID_NDK_ROOT" >&2
    exit 1
fi
if [[ ! -x "$VCPKG_ROOT/vcpkg" ]]; then
    echo "Set VCPKG_ROOT to a bootstrapped vcpkg checkout." >&2
    exit 1
fi

export ANDROID_SDK_ROOT
export ANDROID_HOME="$ANDROID_SDK_ROOT"
export ANDROID_NDK_HOME="$ANDROID_NDK_ROOT"
export VCPKG_ROOT
export VCPKG_DISABLE_METRICS=1

for abi in arm64-v8a armeabi-v7a x86_64; do
    case "$abi" in
        arm64-v8a) triplet=arm64-android-api21 ;;
        armeabi-v7a) triplet=arm-neon-android-api21 ;;
        x86_64) triplet=x64-android-api21 ;;
    esac

    "$ANDROID_SDK_ROOT/cmake/3.31.6/bin/cmake" \
        -S "$ROOT_DIR/android/native" \
        -B "$ROOT_DIR/build/android/$abi" \
        -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
        -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
        -DVCPKG_OVERLAY_TRIPLETS="$ROOT_DIR/cmake/vcpkg_triplets" \
        -DVCPKG_TARGET_TRIPLET="$triplet" \
        -DVCPKG_HOST_TRIPLET=x64-linux \
        -DVCPKG_MANIFEST_DIR="$ROOT_DIR" \
        -DANDROID_ABI="$abi" \
        -DANDROID_PLATFORM=android-21 \
        -DANDROID_STL=c++_shared \
        -DSIMLIB_ANDROID_JNI_ROOT="$ROOT_DIR/build/android/jniLibs" \
        -DSIMLIB_VALIDATE_SHADERS=OFF \
        -DSIMLIB_BUILD_SHADER_COMPILER=OFF \
        -DCMAKE_BUILD_TYPE=Debug
    targets=()
    for app in "${apps[@]}"; do
        targets+=("${app}_main")
    done
    "$ANDROID_SDK_ROOT/cmake/3.31.6/bin/cmake" --build "$ROOT_DIR/build/android/$abi" \
        --parallel "$BUILD_JOBS" --target "${targets[@]}"

    case "$abi" in
        arm64-v8a) stl_dir=aarch64-linux-android ;;
        armeabi-v7a) stl_dir=arm-linux-androideabi ;;
        x86_64) stl_dir=x86_64-linux-android ;;
    esac
    for app in "${apps[@]}"; do
        cp "$ROOT_DIR/build/android/jniLibs/common/$abi/libSDL2.so" \
            "$ROOT_DIR/build/android/jniLibs/$app/$abi/libSDL2.so"
        cp "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/$stl_dir/libc++_shared.so" \
            "$ROOT_DIR/build/android/jniLibs/$app/$abi/libc++_shared.so"
    done
done

for app in "${apps[@]}"; do
    "$ROOT_DIR/android/sample/gradlew" --no-daemon -p "$ROOT_DIR/android/sample" \
        -PsimlibApp="$app" exportDebugApk
    echo "Android APK: $ROOT_DIR/build/android/apks/$app.apk"
done

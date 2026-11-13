#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$HOME/Android/Sdk}}"
ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-$ANDROID_SDK_ROOT/ndk/29.0.14206865}"
VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg-mingw}"
BUILD_JOBS="${ANDROID_BUILD_JOBS:-2}"

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
        -DSIMLIB_ANDROID_JNI_LIBS_DIR="$ROOT_DIR/build/android/jniLibs/$abi" \
        -DSIMLIB_VALIDATE_SHADERS=OFF \
        -DSIMLIB_BUILD_SHADER_COMPILER=OFF \
        -DCMAKE_BUILD_TYPE=Debug
    "$ANDROID_SDK_ROOT/cmake/3.31.6/bin/cmake" --build "$ROOT_DIR/build/android/$abi" --parallel "$BUILD_JOBS"

    case "$abi" in
        arm64-v8a) stl_dir=aarch64-linux-android ;;
        armeabi-v7a) stl_dir=arm-linux-androideabi ;;
        x86_64) stl_dir=x86_64-linux-android ;;
    esac
    cp "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/$stl_dir/libc++_shared.so" \
        "$ROOT_DIR/build/android/jniLibs/$abi/libc++_shared.so"
done

"$ROOT_DIR/android/sample/gradlew" --no-daemon -p "$ROOT_DIR/android/sample" assembleDebug

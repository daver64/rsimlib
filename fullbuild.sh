#!/bin/sh
set -eu
cd "$(dirname "$0")"

if [ ! -f build-win/CMakeCache.txt ] ||
   ! grep -q '^VCPKG_TARGET_TRIPLET:STRING=x64-mingw-posix$' build-win/CMakeCache.txt; then
    echo "Configure build-win for the x64-mingw-posix cross-build before running fullbuild.sh." >&2
    exit 1
fi

echo "== Linux =="
cmake --fresh -S . -B build -DCMAKE_INSTALL_PREFIX="$HOME/opt" \
      -DCMAKE_BUILD_TYPE=Release \
      -DSIMLIB_RENDERERS="gl;vulkan" \
      -DSIMLIB_GUI=imgui \
      -DSIMLIB_VIDEO=ffmpeg
cmake --build build --clean-first -j4
cmake --install build

echo "== Windows (mingw) =="
cmake -S . -B build-win -DCMAKE_INSTALL_PREFIX="$HOME/opt-win" \
      -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY="$PWD/build-win/lib"
cmake --build build-win --clean-first -j4 --target simlib_convert
cmake --install build-win

echo "== Android =="
./android/build.sh

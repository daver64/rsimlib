# Building, Installing & Distributing

[← Back to README](../README.md)

**Always required**
- CMake 3.16+
- A C++17 compiler
- SDL2, SDL2_ttf, SDL2_image, SDL2_mixer (development packages)
- libpng, zlib

**Per renderer** (see `SIMLIB_RENDERERS` below; you only need the ones you build)
- `gl`: OpenGL development headers/libraries (GLEW as well on Windows)
- `vulkan`: Vulkan headers/loader, plus `glslangValidator` (from `glslang-tools`) and `spirv-tools`, which are also needed at runtime to compile arbitrary user GLSL shaders on the Vulkan backend
- `d3d12`: Windows only, the Windows SDK; optional `glslang`/`spirv-cross`/`directx-dxc` for runtime shader translation

**Optional features**
- `SIMLIB_VIDEO=ffmpeg`: FFmpeg 5.1+ development packages (`libavformat`, `libavcodec`, `libavutil`, `libswscale`, `libswresample`)

### Debian / Ubuntu Setup:

Base packages plus the default renderers (OpenGL and Vulkan):

```bash
sudo apt install cmake build-essential libsdl2-dev libsdl2-ttf-dev \
    libsdl2-image-dev libsdl2-mixer-dev libpng-dev zlib1g-dev \
    libgl1-mesa-dev libvulkan-dev glslang-tools spirv-tools
```

An OpenGL-only build (`-DSIMLIB_RENDERERS=gl`) can drop `libvulkan-dev glslang-tools spirv-tools`; a Vulkan-only build (`-DSIMLIB_RENDERERS=vulkan`) can drop `libgl1-mesa-dev`. For FFmpeg video add:

```bash
sudo apt install libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev
```

Dear ImGui, Lua, sol2, Box2D, and pl_mpeg are fetched automatically via CMake's `FetchContent` — no manual setup required.

Dear ImGui is optional: configure with `-DSIMLIB_GUI=none` to build without it (the `gui_*` API becomes no-ops and the ImGui examples are skipped).

Renderers are selected with `-DSIMLIB_RENDERERS="gl;vulkan;d3d12"` (default `gl;vulkan`, plus `d3d12` on Windows; Android defaults to `gles`). At least one is required; `gles` is Android-only and `d3d12` is Windows-only. Disabled renderers are not compiled or linked (so e.g. `-DSIMLIB_RENDERERS=gl` needs no Vulkan SDK), the matching examples (`ex3d_gl`, `exvulkan`/`ex3d_vulkan`) are skipped, and requesting a disabled backend with `--gl`/`--vulkan`/`--d3d12` reports an error. The default backend is the first enabled one (D3D12 on Windows when enabled).

| Option | Values | Default |
|--------|--------|---------|
| `SIMLIB_RENDERERS` | `gl`, `gles`, `vulkan`, `d3d12` (list) | `gl;vulkan` (+ `d3d12` on Windows; `gles` on Android) |
| `SIMLIB_GUI` | `imgui`, `none` | `imgui` |
| `SIMLIB_VIDEO` | `plmpeg`, `ffmpeg`, `none` | `plmpeg` |
| `SIMLIB_D3D12_BUILD_TRANSLATION` | `ON`, `OFF` | `ON` (Windows, with `d3d12`) |
| `SIMLIB_D3D12_REQUIRE_TRANSLATION` | `ON`, `OFF` | `ON` (Windows, with `d3d12`) |

With `SIMLIB_D3D12_BUILD_TRANSLATION` (the default), glslang and SPIRV-Cross are downloaded and built from source when vcpkg does not provide them, which lengthens the first build. `SIMLIB_D3D12_REQUIRE_TRANSLATION` applies when that is `OFF`: it makes the configure step fail when glslang or SPIRV-Cross is missing, because without them the Direct3D 12 lighting, bloom and compute effects cannot run.

On Windows, install GLEW, libpng, and zlib with vcpkg, then configure CMake with the vcpkg toolchain. GLEW loads the OpenGL 4.3 entry points used by the optional OpenGL backend; libpng and zlib are required by PNG export:

```powershell
vcpkg install glew:x64-windows libpng:x64-windows zlib:x64-windows glslang:x64-windows spirv-cross:x64-windows directx-dxc:x64-windows "sdl2[vulkan]:x64-windows"
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake"
```

The `[vulkan]` feature matters: vcpkg's default SDL2 is built without Vulkan, and the Vulkan backend then fails at startup with "Vulkan support is either not configured in SDL or not available". The Vulkan backend also needs `vulkan-1.dll` (installed with the GPU driver) and the compiled `.spv` shaders next to the executables. If `glslangValidator` is not installed (vcpkg's glslang omits it unless you ask for `glslang[tools]`, and the Vulkan SDK includes it), CMake builds glslang's compiler from source and uses it to generate the `.spv` files, which needs network access and Python 3 at configure/build time; pass `-DSIMLIB_BUILD_SHADER_COMPILER=OFF` to skip this. `glslang` and `spirv-cross` are needed for the Direct3D 12 effects (lighting, bloom, compute); without them those examples fail on `--d3d12`.

### Video Playback

`sl::open_video()` (see [`exvideo`](../src/examples/exvideo/exvideo.cpp)) plays video into a bitmap with synced audio.
The decoder is chosen at configure time with `-DSIMLIB_VIDEO=<plmpeg|ffmpeg|none>`:

| Value | Formats | Notes |
|-------|---------|-------|
| `plmpeg` (default) | MPEG1 video + MP2 audio (`.mpg`) | Bundled `pl_mpeg`, no extra dependencies. Local files only. |
| `ffmpeg` | Most formats (`.mp4`, `.mkv`, `.webm`, H.264...) | Needs FFmpeg 5.1+ dev packages (found via pkg-config). Also opens URLs/streams and memory buffers. |
| `none` | - | Video API compiled as stubs; `open_video()` returns `nullptr` and sets `last_error()`. |

All backends share the same API, including `seek_video()`, `video_position()`, `video_has_audio()` and
`video_backend_name()`. FFmpeg is LGPL (GPL if built with GPL components); link it dynamically.
To prepare footage for the default backend:

```bash
ffmpeg -i input.mp4 -c:v mpeg1video -q:v 2 -c:a mp2 -format mpeg cutscene.mpg
```

Play with `./build/exvideo cutscene.mpg [--loop]` (on-screen controls: Space pauses, Left/Right seek 5 s, Up/Down or the slider set volume, M mutes, click the bar to scrub, I toggles the overlay). Small test clips live in
`tests/data/`.

### Build Commands:

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

### Install to a Local Prefix

Configure simlib with the prefix where you want it installed, then build and install the library:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX="$HOME/opt"
cmake --build build --target simlib -j"$(nproc)"
cmake --install build
```

The install contains the public headers, `simlib` static library, bundled ImGui/Lua/sol2/Box2D dependencies, shader assets, and a CMake package. SDL2, SDL2_ttf, SDL2_image, SDL2_mixer, OpenGL, Vulkan, PNG, and zlib remain system prerequisites; install their development packages on the machine building the application.

On the Linux release machine, `sh fullbuild.sh` clean-builds and installs the Linux build to `~/opt` and the existing `x64-mingw-posix` cross-build to `~/opt-win`, using at most four build jobs. Configure `build-win` with the MinGW vcpkg toolchain and triplet first; the script preserves its cross-build settings and keeps its libraries separate from the Linux build. The Windows build covers the installable library and converter, not every example.

### Android (OpenGL ES)

The Android target builds the static simlib library and a minimal SDL-based sample APK for `arm64-v8a`, `armeabi-v7a`, and `x86_64`, using OpenGL ES 3.1 and Android API 21. It requires the Android command-line SDK, NDK, CMake, Ninja, a full JDK (Java 17 or newer, including `javac`), and a bootstrapped vcpkg checkout; a JRE alone is insufficient. If Gradle selects a JRE, set `JAVA_HOME` to the JDK installation. The repository manifest supplies SDL2, SDL2_image, SDL2_mixer, SDL2_ttf, SQLite, libpng, and zlib. GLES does not provide the desktop polygon wireframe mode or GPU timer diagnostics.

Install the Android packages if they are not already present:

```bash
sdkmanager "platforms;android-36" "build-tools;36.0.0" \
    "ndk;29.0.14206865" "cmake;3.31.6" "emulator" \
    "platform-tools" "system-images;android-36;google_apis;x86_64"
```

Set the toolchain paths (replace `VCPKG_ROOT` with your vcpkg checkout), then build all three APK ABIs:

```bash
git submodule update --init android/SDL2
export ANDROID_HOME="$HOME/Android/Sdk"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/29.0.14206865"
export VCPKG_ROOT="$HOME/vcpkg"
android/build.sh
```

The debug APK is written to `android/sample/app/build/outputs/apk/debug/app-debug.apk`. Its interactive orbital showcase demonstrates 2D shapes, text, animation, particle trails, and touch-driven palette changes. It is intended to exercise Android-supported simlib features rather than port the full desktop example suite.

For an x86_64 desktop smoke test, create and start an API 36 emulator. The emulator must expose an OpenGL ES 3.1 context; some software renderers provide only ES 3.0. On a host with compatible graphics drivers, `-gpu host` uses the host GPU:

```bash
avdmanager create avd --name simlib-api36-x86_64 \
    --package "system-images;android-36;google_apis;x86_64" --device pixel_6
emulator -avd simlib-api36-x86_64 -gpu host
adb install -r android/sample/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.daver64.simlib.android/.SimlibActivity
```

For a Galaxy phone, enable USB debugging, authorize the computer on the device, and use `adb devices` to confirm it is listed as `device`. Then use the same `adb install` and `adb shell am start` commands. The phone needs an OpenGL ES 3.1-capable GPU.

---

### Configure a Consumer

In the application's `CMakeLists.txt`, find the installed package and link the exported target:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app LANGUAGES CXX)

find_package(simlib CONFIG REQUIRED)
add_executable(my_app src/main.cpp)
target_link_libraries(my_app PRIVATE simlib::simlib)
```

On Windows, SDL's main shim is linked only by graphical executables, so add its target to the application:

```cmake
if(WIN32)
    target_link_libraries(my_app PRIVATE SDL2::SDL2main)
endif()
```

Tell CMake where the local prefix is when configuring the application:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/opt"
cmake --build build -j"$(nproc)"
```

Runtime shader assets (`share/simlib/shaders/{glsl,d3d,vulkan}`) are searched for in this order, so an installed or copied tree can be moved without rebuilding:

1. `$SIMLIB_DATA_DIR/shaders` — set `SIMLIB_DATA_DIR` to the directory that contains `shaders/` (for example `<prefix>/share/simlib`)
2. next to the executable: `<exe>/shaders`, `<exe>/share/simlib/shaders`, `<exe>/../share/simlib/shaders`
3. the `CMAKE_INSTALL_PREFIX` baked in at configure time
4. the source and build trees simlib was built from

If you only move the install, point `SIMLIB_DATA_DIR` at the new location or place the `share/simlib` folder relative to your executable.

### Distribute an Application

`find_package(simlib)` also provides `simlib_add_package()`, which adds a `<target>_package` target that archives your executable with everything it needs at run time: shared libraries such as the SDL2 DLLs, simlib's shaders (including the compiled Vulkan SPIR-V), `dxcompiler.dll`/`dxil.dll` when known, your assets and the licences.

```cmake
simlib_add_package(TARGET my_app NAME my_app-1.0-win64 ASSETS assets EXTRA_FILES README.txt)
```

```bash
cmake --build build --config Release --target my_app_package   # -> build/package/my_app-1.0-win64.zip
```

The archive is a `.zip` on Windows and a `.tar.gz` elsewhere (override with `FORMAT zip|tar.gz`); `EXCLUDE <regex>...` leaves matching files out of `ASSETS`. Unpack anywhere and run; simlib finds the shaders beside the executable. Set `SIMLIB_PACKAGE_DXC_DIR` to a folder holding the DXC DLLs if they are not detected. On Linux the system SDL2 libraries are not bundled. Vulkan users still need `vulkan-1.dll` from their GPU driver.

Run the test suite:

```bash
ctest --test-dir build --output-on-failure
```

Test targets are built with assertions enabled even in Release builds.

---

## Documentation Generation

If Doxygen is installed, generate HTML API documentation and section-3 man pages with:

```bash
cmake --build build --target docs
```

`cmake --install build` also generates and installs the API man pages under `${CMAKE_INSTALL_PREFIX}/share/man/man3` (or the platform's configured man directory). Doxygen documents only explicitly documented public declarations; the docs target currently completes with no undocumented-symbol warnings.

For a user-local prefix such as `$HOME/opt`, add its man directory to `MANPATH` to make the pages discoverable:

```bash
export MANPATH="$HOME/opt/share/man${MANPATH:+:$MANPATH}"
man 3 simlib
man 3 sl_Bitmap
```

Add the `export MANPATH=...` line to your shell startup file to keep it across sessions.

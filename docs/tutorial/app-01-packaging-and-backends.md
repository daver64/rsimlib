# Lesson 1 (app): Packaging and backends

[← Tutorial index](README.md) · Previous: [Flight](sim-04-flight.md) (or any lesson: this one is about shipping what you made)

**Goal:** turn a simlib program into a zip that a friend can unpack and run, and know which graphics backend it will use.

## The idea

- simlib draws with **OpenGL**, **Vulkan** or **Direct3D 12** (Windows). Every lesson passes its arguments to `configure_graphics_backend_from_args`, so a user can pick one with `--gl`, `--vulkan` or `--d3d12`. With no flag the default is used.
- Each backend needs **shaders**. GLSL and HLSL ship as text; Vulkan needs compiled SPIR-V (`.spv`) files. They live in a `shaders/` folder, and simlib looks for it in this order: `$SIMLIB_DATA_DIR/shaders`, `<exe>/shaders`, `<exe>/share/simlib/shaders`, `<exe>/../share/simlib/shaders`, the install prefix, then the source and build trees. Beside the executable wins over anything on the developer's machine, so a packaged app never depends on your source checkout.
- Your app also needs its runtime libraries (the SDL2 DLLs on Windows), its assets, and for Direct3D 12 the DXC compiler DLLs (`dxcompiler.dll`, `dxil.dll`).
- `simlib_add_package()` gathers all of that for you into one zip.

## The CMake file

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_app CXX)

find_package(simlib REQUIRED)       # an installed simlib, or a build tree via CMAKE_PREFIX_PATH

add_executable(my_app src/main.cpp)
target_link_libraries(my_app PRIVATE simlib)

simlib_add_package(TARGET my_app
    NAME my_app-1.0-win64
    ASSETS assets
    EXTRA_FILES README.txt)
```

Build the package target:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/simlib
cmake --build build --config Release --target my_app_package
```

The result is `build/package/my_app-1.0-win64.zip`. It holds the executable, the shared libraries it needs, `shaders/` (GLSL, HLSL and compiled SPIR-V), `dxcompiler.dll` and `dxil.dll` when known, your `assets/` folder, `README.txt` and the licence files.

## What just happened

1. `find_package(simlib)` makes `simlib_add_package()` available as well as the `simlib` target.
2. The package target copies the executable and everything it needs into a staging folder, then zips it.
3. On a user's machine, unzip anywhere and run. The shaders are found beside the executable, so nothing has to be installed.

## Things to check before you ship

- **Test from the unzipped folder**, ideally on a machine without the development tools, with `--gl`, `--vulkan` and `--d3d12`. A "cannot open shader" error means something is missing from the package, and the message names the path it tried.
- **Load assets relative to the executable or the working directory, not an absolute path** on your machine. A path such as `C:/Users/you/project/...` works only for you.
- **Vulkan** also needs `vulkan-1.dll`, which comes from the player's GPU driver and is not shipped. If it is missing, the app prints why Vulkan failed; players can fall back to `--gl`.
- **Direct3D 12** needs the DXC DLLs. If they are not detected, set `SIMLIB_PACKAGE_DXC_DIR` to the folder that holds them when you configure.
- **Linux**: the package does not bundle the system SDL2 libraries, so tell users to install them.
- Set `SIMLIB_DATA_DIR` to a folder containing `shaders/` to test a different shader set without rebuilding.

## Try it

1. Package one of your lesson programs and unzip it to a new folder.
2. Run it with `--gl`, then `--vulkan`.
3. Rename the `shaders` folder in your unzipped copy and read the error message, then put it back.
4. Add a second executable to the same zip with `EXTRA_TARGETS`.
5. Make a small `README.txt` of controls and ship it with `EXTRA_FILES`.

## Go further

- README: [Distribute an application](../../README.md#distribute-an-application) and [Configure a consumer](../../README.md#configure-a-consumer).
- Back to the [tutorial index](README.md), or try the [Showcase Guide](../showcases.md).

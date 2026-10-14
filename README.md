# simlib

`simlib` is an easy-to-use, feature-packed Allegro4-style C++ game framework and rendering engine for 2D and 3D, built on SDL2 with selectable OpenGL 4.3 and Vulkan backends (plus Direct3D 11/12 on Windows). It combines immediate-mode 2D drawing with 3D surfaces, mesh and model rendering, voxel worlds, and direct access to backend graphics APIs, alongside auto-batched sprites and primitives, TrueType typography, integrated Dear ImGui GUI, SDL_mixer audio, post-process effects, 2D dynamic soft shadows and lighting, ZIP asset archives, cellular automata fluid simulation, Perlin/simplex noise, integrated Box2D physics, and sandboxed Lua scripting via sol2.

## Quick start

A minimal application only needs `sl.h`, which pulls in the whole public API:

```cpp
#include "sl.h"

int main()
{
    if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }
    sl::Event event;
    bool running = true;
    while (running)
    {
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit)
                running = false;
            sl::display_handle_event(event);
        }

        sl::clear_to_colour(sl::screen, sl::Colour{146, 200, 62});
        // ... draw your frame ...
        sl::show_video_bitmap();
        sl::end_frame();
    }
}
```

Link your CMake target against the `simlib` library target:

```cmake
add_executable(my_app src/my_app.cpp)
target_link_libraries(my_app PRIVATE simlib)
```


Create the archive used by `exresources` from the repository root with:

```bash
./build/slpack resource_demo.zip assets
./build/exresources resource_demo.zip
```

## Documentation Guides

For in-depth guides and detailed API tables, see the documentation topics:

- 📖 **[Core API Reference](docs/api_reference.md)** — Display, bitmaps, shape primitives, persistent font atlases, audio subsystems, fluid simulation, and ZIP archives.
- 🧩 **[Entity Component System (ECS) Integration Guide](docs/ecs.md)** — Architectural patterns, clean separation of concerns, and interoperability with RECS, EnTT, Flecs, or custom ECS frameworks.
- 🗡️ **[`exroguelike` ASCII Roguelike Guide](docs/exroguelike_guide.md)** — Architectural walkthrough of the turn-based roguelike (512x512 procedural overworld, 3x3 sector dungeons, FOV, 2D lighting, and decoupling `simlib`, `recs`, and `rworld`).
- 🧪 **[`excafluid` Physics Sandbox Guide](docs/excafluid_guide.md)** — Cellular automata fluid mechanics, thermodynamics, phase changes, and Box2D Archimedes buoyancy coupling (includes demo video).
- 🖌️ **[`slpaint` Retro Paint Guide](docs/slpaint_guide.md)** — Architectural walkthrough of the paint app (drawing tools, undo/redo stack, floating selection, and preview overlays).
- 🕹️ **[`sltest` Sample Game Guide](docs/sltest_guide.md)** — Structural walk-through of the sample game's state machines, entities, and UI.
- 🎨 **[Graphics Effects & 2D Lighting](docs/graphics_effects.md)** — 17+ post-processing shaders, `PingPongBuffer` compositing pipeline, radial lights, and dynamic polygon soft shadows.
- ⚙️ **[Renderer Backends & Architecture](docs/renderers.md)** — OpenGL 4.3, Vulkan, Direct3D backends, automatic hardware batching, runtime GLSL-to-SPIR-V compilation, and direct 3D rendering APIs.
- 🧊 **[3D Surfaces & Model Loading](docs/api_reference.md#3d-surfaces-includesurface3dh)** — Offscreen MSAA 3D targets, camera perspective, directional lighting, OBJ/glTF/GLB models, and seamless 2D HUD compositing.
- 🧱 **[Voxel Chunks & World](docs/api_reference.md#voxel-chunks--world-includevoxelh)** — 16x16x16 chunk rendering, hidden-face culling into GPU vertex buffers, texture atlas mapping, multi-chunk coordination, and 3D DDA raycasting for Minecraft-style worlds.
- 🚀 **[Physics & Collision Integration](docs/physics.md)** — Opaque Box2D C++ wrapper, simple particle/AABB entity physics, and Lua physics bindings.
- 📜 **[Sandboxed Lua Scripting & Canvas](docs/scripting_lua.md)** — Hardened Lua sandbox via sol2, persistent canvas drawing API, audio scripting, and C++ event dispatching.

---

## Prerequisites & Building

- CMake 3.16+
- A C++17 compiler
- SDL2, SDL2_ttf, SDL2_image, SDL2_mixer (development packages)
- OpenGL, Vulkan, libpng, zlib
- `glslangValidator` (from `glslang-tools`), also required at runtime for compiling arbitrary user GLSL shaders on the Vulkan backend

### Debian / Ubuntu Setup:

```bash
sudo apt install cmake build-essential libsdl2-dev libsdl2-ttf-dev \
    libsdl2-image-dev libsdl2-mixer-dev libpng-dev zlib1g-dev libgl1-mesa-dev \
    libvulkan-dev glslang-tools spirv-tools
```

Dear ImGui, Lua, sol2, Box2D, and pl_mpeg are fetched automatically via CMake's `FetchContent` — no manual setup required.

### Video Playback

`sl::open_video()` (see [`exvideo`](src/examples/exvideo/exvideo.cpp)) decodes MPEG1 video and MP2 audio only
(via the bundled `pl_mpeg` decoder) — it cannot read `.mp4`/`.mov`/`.webm`/H.264 files directly. Transcode
source footage with `ffmpeg` first:

```bash
ffmpeg -i input.mp4 -c:v mpeg1video -q:v 2 -c:a mp2 -format mpeg cutscene.mpg
```

Then play it back with `./build/exvideo cutscene.mpg [--loop]`.

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

### Configure a Consumer

In the application's `CMakeLists.txt`, find the installed package and link the exported target:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app LANGUAGES CXX)

find_package(simlib CONFIG REQUIRED)
add_executable(my_app src/main.cpp)
target_link_libraries(my_app PRIVATE simlib::simlib)
```

Tell CMake where the local prefix is when configuring the application:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/opt"
cmake --build build -j"$(nproc)"
```

If you change the install prefix, reconfigure simlib with the new `CMAKE_INSTALL_PREFIX` and reinstall it. The configured prefix is also used to locate the installed runtime shader assets.

Run the test suite:

```bash
ctest --test-dir build --output-on-failure
```

---

## 3D Graphics: Surfaces, Models & Voxel Worlds

`simlib` includes both a high-level 3D drawing API and lower-level ways to build renderer-specific scenes. The backend-neutral `sl::Surface3D` API ([include/surface3d.h](include/surface3d.h)) is implemented for OpenGL and Vulkan. Create an offscreen colour-and-depth surface with optional multisample anti-aliasing (MSAA), draw with a 3D `Camera` and directional `Light3D`, render built-in primitive meshes or loaded Wavefront `.obj`/`.mtl`, `.gltf`, and `.glb` models, add 2D overlays such as HUD text and sprites, and composite the result into a 2D frame.

```cpp
// Create an offscreen 3D surface with 4x MSAA
sl::Surface3D *surface = sl::create_surface_3d(640, 480, 4);
sl::Model *crate = sl::load_model("assets/models/crate.obj");
sl::Camera camera;
camera.position = {2.5f, 2.2f, 4.0f};
camera.target = {0.0f, 0.3f, 0.0f};

// Draw 3D scene and 2D overlays into the surface
if (sl::begin_surface_3d(surface, camera))
{
    sl::clear_surface_3d({20, 30, 60});
    sl::draw_model(crate, sl::model_matrix({0, 0, 0}, {0, rotation, 0}));
    sl::gprintf(16, 16, {255, 255, 255}, "MSAA %dx Surface3D", sl::surface_3d_samples(surface));
    sl::end_surface_3d();
}

// Composite the 3D surface bitmap into your 2D scene
sl::draw_sprite(sl::surface_3d_bitmap(surface), 80, 60);
```

For block-based worlds, the voxel API provides chunk rendering, hidden-face culling, texture-atlas mapping, and 3D raycasting; see [Voxel Chunks & World](docs/api_reference.md#voxel-chunks--world-includevoxelh). For renderer-specific control, the [3D examples](docs/renderers.md#direct-3d-escape-hatches) demonstrate direct OpenGL, Vulkan, Direct3D 11, and Direct3D 12 rendering. `Surface3D` currently supports OpenGL and Vulkan; Direct3D examples use their direct-rendering APIs.

---

## Running the Examples

Every graphical example runs on OpenGL by default and accepts `--gl` or `--vulkan` (on Windows, `--d3d11` and `--d3d12` are also available).

```bash
./build/exhello         # Minimal hello-world window and text
./build/exfont          # Proportional & monospace TrueType font rendering with glyph atlas
./build/extriangle      # Immediate-mode 2D primitives and thickness
./build/exrotatesprite  # Rotating and scaling sprites with timing
./build/exbitmap        # Bitmap creation, pixel access, blitting, PNG loading
./build/exlighting      # 2D radial dynamic lights and polygon shadow casters
./build/exphysics       # Box2D rigid bodies and contact polling
./build/exvulkan        # Vulkan backend textured sprites and render targets
./build/exdb            # Embedded SQLite transactions and prepared statements (rdb)
./build/exshader        # Custom GLSL shader with runtime SPIR-V compilation
./build/exrendertarget  # Off-screen render-target composition
./build/exaudio         # Sound effect mixer and streamed music
./build/exinput         # Keyboard, mouse, gamepad axes/buttons, and hotplugging
./build/exgui           # Dear ImGui widgets and integration
./build/exparticles     # Particle emitters, fades, and gravity
./build/exresources     # Encrypted/compressed ZIP asset archive loading
./build/exatlas         # Texture atlas slicing and sprite grid navigation
./build/exconfig        # JSON configuration loading, saving, dotted keys, and live UI
./build/expostprocess   # Full post-processing effects and ping-pong chaining
./build/excafluid       # Cellular automata fluid mechanics, thermodynamics, phase changes etc
./build/excompute       # GPU compute shader particle simulation & SSBO buffer readback
./build/exgpuparticles  # 150k GPU compute particles with gravity wells, explosions, and glow
./build/execs           # Interactive Entity Component System (RECS) integration
./build/exroguelike     # Coloured ASCII roguelike (512x512 overworld, dungeons, FOV, lights)
./build/exisometric     # 2.5D isometric projection, tile picking, elevation, and 2D lighting
./build/ex3d_surface    # Offscreen 3D surface with MSAA, models (OBJ/glTF/GLB), and 2D HUD
./build/exvoxel         # 16x16x16 voxel chunk rendering (Minecraft clone demo)
./build/ex3d_gl         # Direct OpenGL 3D escape hatch
./build/ex3d_vulkan     # Direct Vulkan 3D escape hatch
./build/ex3d_d3d11      # Direct Direct3D 11 3D escape hatch
./build/ex3d_d3d12      # Direct Direct3D 12 3D escape hatch
./build/slpaint         # Retro paint app with brush/shapes, palette, undo/redo
./build/exspriteeditor  # Pixel sprite/animation editor (1x1-512x512, zoom, frames, PNG spritesheet I/O)
./build/exvideo         # MPEG1/MP2 "cutscene" video playback into a bitmap (see ffmpeg note below)
./build/sltest          # Sample game: menu, physics playground, Lua console
./build/slpack          # CLI asset-packing tool
./build/slunpack        # CLI asset-unpacking tool
```

### Example Walkthrough Source Links
- **[`exroguelike`](src/examples/exroguelike/exroguelike.cpp)** ([Guide](docs/exroguelike_guide.md)) — Turn-based ASCII roguelike showcasing procedural generation, FOV, 2D lighting, and decoupling `simlib`, `recs`, and `rworld`.
- **[`execs`](src/examples/execs/execs.cpp)** ([Guide](docs/ecs.md)) — Interactive Entity Component System (ECS) demonstration showing decoupled simulation, archetype chunk iteration, and parallel updates.
- **[`excafluid`](src/examples/excafluid/excafluid.cpp)** ([Guide](docs/excafluid_guide.md)) — Cellular automata fluid mechanics, thermodynamics, phase changes, and Box2D Archimedes buoyancy coupling (includes demo video).
- **[`slpaint`](src/examples/slpaint/slpaint.cpp)** ([Guide](docs/slpaint_guide.md)) — Retro paint app with brush/shapes, thickness, palette, and multi-level undo/redo.
- **[`expostprocess`](src/examples/expostprocess/expostprocess.cpp)** ([Guide](docs/graphics_effects.md)) — 17+ post-processing shader effects chained with `PingPongBuffer`.
- **[`exgpuparticles`](src/examples/exgpuparticles/exgpuparticles.cpp)** ([Guide](docs/exgpuparticles_guide.md)) — 150k GPU compute particles with gravity wells, explosions, and cross-backend compute shader uniforms.
- **[`sltest`](src/examples/sltest/main.cpp)** ([Guide](docs/sltest_guide.md)) — Sample game with main menu, physics playground, and embedded Lua REPL.


### Example Source Links
- **[`exhello`](src/examples/exhello/exhello.cpp)** — Window setup, frame presentation, clearing, and basic text.
- **[`exfont`](src/examples/exfont/exfont.cpp)** — TrueType loading, font selection, measurement, and text rendering.
- **[`extriangle`](src/examples/extriangle/extriangle.cpp)** — Immediate-mode 2D primitives: triangles, rectangles, circles, and lines.
- **[`exrotatesprite`](src/examples/exrotatesprite/exrotatesprite.cpp)** — Bitmap loading, sprite rotation, scaling, and frame timing.
- **[`exbitmap`](src/examples/exbitmap/exbitmap.cpp)** — Bitmap creation, pixel access, blitting, and image loading.
- **[`exlighting`](src/examples/exlighting/exlighting.cpp)** — Render targets, radial lights, shadow casters, and `LightingPass`.
- **[`exphysics`](src/examples/exphysics/exphysics.cpp)** — Box2D world/body/fixture creation, stepping, and contact polling.
- **[`exvulkan`](src/examples/exvulkan/exvulkan.cpp)** — Backend selection, textured sprites, render targets, and Bloom.
- **[`exlua`](src/examples/exlua/exlua.cpp)** — Lua runtime setup and execution through sol2.
- **[`exluaconsole`](src/examples/exluaconsole/exluaconsole.cpp)** — Interactive Lua command input, output, and console integration.
- **[`exdb`](src/examples/exdb/exdb.cpp)** — SQLite transactions, prepared statements, and queries through `rdb::Database`.
- **[`exshader`](src/examples/exshader/exshader.cpp)** — File-based GLSL loading, runtime compilation, and textured quads.
- **[`exrendertarget`](src/examples/exrendertarget/exrendertarget.cpp)** — Off-screen rendering, render-target lifetime, and compositing.
- **[`exaudio`](src/examples/exaudio/exaudio.cpp)** — SDL_mixer sound effects, streamed music, pause/resume, and volume.
- **[`exinput`](src/examples/exinput/exinput.cpp)** — Held keyboard state, mouse state, gamepad axes/buttons, and hotplugging.
- **[`exgui`](src/examples/exgui/exgui.cpp)** — Dear ImGui initialization, event forwarding, widgets, and rendering.
- **[`exparticles`](src/examples/exparticles/exparticles.cpp)** — Particle emission, movement, lifetime/colour fades, and gravity.
- **[`exresources`](src/examples/exresources/exresources.cpp)** — ZIP archive opening, entry enumeration, and packaged image loading.
- **[`exatlas`](src/examples/exatlas/exatlas.cpp)** — Texture atlas slicing, tile index mapping, and sprite grid rendering.
- **[`exconfig`](src/examples/exconfig/exconfig.cpp)** — JSON configuration loading, saving, dotted key paths, and typed getters/setters.
- **[`excompute`](src/examples/excompute/excompute.cpp)** — GPU compute shader particle simulation and SSBO data readback.
- **[`exgpuparticles`](src/examples/exgpuparticles/exgpuparticles.cpp)** — 150k GPU compute particles with mouse-driven gravity wells, explosions, and additive glow.
- **[`execs`](src/examples/execs/execs.cpp)** ([Guide](docs/ecs.md)) — Entity Component System (ECS) integration, archetype chunk iteration, and parallel updates.
- **[`exroguelike`](src/examples/exroguelike/exroguelike.cpp)** ([Guide](docs/exroguelike_guide.md)) — Coloured ASCII roguelike combining `simlib`, `recs` (ECS), and `rworld` (world generation).
- **[`exisometric`](src/examples/exisometric/exisometric.cpp)** — 2.5D dimetric isometric projection, elevation carving, mouse tile-picking, depth sorting, and soft lighting.
- **[`ex3d_surface`](src/examples/ex3d_surface/ex3d_surface.cpp)** — High-level 3D surface target with MSAA, primitive meshes, model loading (OBJ with MTL, glTF, GLB), directional lighting, and 2D HUD compositing.
- **[`exvoxel`](src/examples/exvoxel/exvoxel.cpp)** — Minecraft clone demo: 16x16x16 voxel chunk rendering, hidden-face culling, procedural terrain (FastNoiseLite), pixel-art texture atlas, first-person camera, and 3D DDA block break/place raycasting.
- **[`ex3d_gl`](src/examples/ex3d_gl/ex3d_gl.cpp)** — Direct OpenGL 3D rendering with depth testing, VAO/VBOs, and GLM camera matrices.
- **[`ex3d_vulkan`](src/examples/ex3d_vulkan/ex3d_vulkan.cpp)** — Direct Vulkan 3D rendering with shared device/swapchain and custom pipelines.
- **[`exspriteeditor`](src/examples/exspriteeditor/exspriteeditor.cpp)** — Pixel sprite/animation editor: zoomed pixel-perfect canvas (1x1-512x512), per-frame undo/redo, animation playback, and PNG spritesheet save/load.
- **[`exvideo`](src/examples/exvideo/exvideo.cpp)** — MPEG1/MP2 video decoding via `pl_mpeg` into a bitmap, with synced audio playback; see [Video Playback](#video-playback) below for the required source format.
- **[`ex3d_d3d11`](src/examples/ex3d_d3d11/ex3d_d3d11.cpp)** — Direct3D 11 3D rendering escape hatch.
- **[`ex3d_d3d12`](src/examples/ex3d_d3d12/ex3d_d3d12.cpp)** — Direct3D 12 3D rendering escape hatch.
- **[`slpack`](src/examples/slpack/slpack.cpp)** / **[`slunpack`](src/examples/slunpack/slunpack.cpp)** — Asset packaging tools.

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

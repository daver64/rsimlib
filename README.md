# simlib

`simlib` is an easy-to-use, feature-packed Allegro4-style 2D game framework and rendering engine built on
SDL2, with high-performance selectable OpenGL 4.3 and Vulkan backends. It combines nostalgic, immediate-mode simplicity with modern game development machinery: auto-batched hardware sprite and primitive rendering, TrueType typography, integrated Dear ImGui GUI, SDL_mixer audio, post-process effects, 2D dynamic soft shadows and lighting, ZIP asset archives, cellular automata fluid simulation, Perlin/simplex noise, integrated Box2D physics, SQLite database persistence (`rdb`), and sandboxed Lua scripting via [sol2](https://github.com/ThePhD/sol2).

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
./slpack resource_demo.zip assets
./exresources resource_demo.zip
```

## Documentation Guides

For in-depth guides and detailed API tables, see the documentation topics:

- 📖 **[Core API Reference](docs/api_reference.md)** — Display, bitmaps, shape primitives, persistent font atlases, audio subsystems, fluid simulation, and ZIP archives.
- 🧪 **[`excafluid` Physics Sandbox Guide](docs/excafluid_guide.md)** — Cellular automata fluid mechanics, thermodynamics, phase changes, and Box2D Archimedes buoyancy coupling (includes demo video).
- 🖌️ **[`slpaint` Retro Paint Guide](docs/slpaint_guide.md)** — Architectural walkthrough of the paint app (drawing tools, undo/redo stack, floating selection, and preview overlays).
- 🕹️ **[`sltest` Sample Game Guide](docs/sltest_guide.md)** — Structural walk-through of the sample game's state machines, entities, and UI.
- 🎨 **[Graphics Effects & 2D Lighting](docs/graphics_effects.md)** — 17+ post-processing shaders, `PingPongBuffer` compositing pipeline, radial lights, and dynamic polygon soft shadows.
- ⚙️ **[Renderer Backends & Architecture](docs/renderers.md)** — OpenGL 4.3, Vulkan, Direct3D backends, automatic hardware batching, runtime GLSL-to-SPIR-V compilation, and 3D escape hatches.
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

Dear ImGui, Lua, sol2, and Box2D are fetched automatically via CMake's `FetchContent` — no manual setup required.

### Build Commands:

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

Run the test suite:

```bash
ctest --test-dir build --output-on-failure
```

---

## Running the Examples

Every graphical example runs on OpenGL by default and accepts `--gl` or `--vulkan` (on Windows, `--d3d11` and `--d3d12` are also available).

```bash
./exhello         # Minimal hello-world window and text
./exfont          # Proportional & monospace TrueType font rendering with glyph atlas
./extriangle      # Immediate-mode 2D primitives and thickness
./exrotatesprite  # Rotating and scaling sprites with timing
./exbitmap        # Bitmap creation, pixel access, blitting, PNG loading
./exlighting      # 2D radial dynamic lights and polygon shadow casters
./exphysics       # Box2D rigid bodies and contact polling
./exvulkan        # Vulkan backend textured sprites and render targets
./exdb            # Embedded SQLite transactions and prepared statements (rdb)
./exshader        # Custom GLSL shader with runtime SPIR-V compilation
./exrendertarget  # Off-screen render-target composition
./exaudio         # Sound effect mixer and streamed music
./exinput         # Keyboard, mouse, gamepad axes/buttons, and hotplugging
./exgui           # Dear ImGui widgets and integration
./exparticles     # Particle emitters, fades, and gravity
./exresources     # Encrypted/compressed ZIP asset archive loading
./exatlas         # Texture atlas slicing and sprite grid navigation
./expostprocess   # Full post-processing effects and ping-pong chaining
./excafluid       # Cellular Automata fluid, falling sand, chemistry, and Box2D
./ex3d            # Direct OpenGL 3D escape hatch
./ex3d_vulkan     # Direct Vulkan 3D escape hatch
./slpaint         # Retro paint app with brush/shapes, palette, undo/redo
./sltest          # Sample game: menu, physics playground, Lua console
./slpack          # CLI asset-packing tool
./slunpack        # CLI asset-unpacking tool
```

### Example Walkthrough Source Links
- **[`excafluid`](src/examples/excafluid/excafluid.cpp)** ([Guide](docs/excafluid_guide.md)) — Cellular Automata fluid, falling sand, chemistry, and Box2D buoyancy.
- **[`slpaint`](src/examples/slpaint/slpaint.cpp)** ([Guide](docs/slpaint_guide.md)) — Retro paint app with brush/shapes, thickness, palette, and multi-level undo/redo.
- **[`expostprocess`](src/examples/expostprocess/expostprocess.cpp)** ([Guide](docs/graphics_effects.md)) — 17+ post-processing shader effects chained with `PingPongBuffer`.
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
- **[`ex3d`](src/examples/ex3d/ex3d.cpp)** — Direct OpenGL 3D rendering with depth testing, VAO/VBOs, and GLM camera matrices.
- **[`ex3d_vulkan`](src/examples/ex3d_vulkan/ex3d_vulkan.cpp)** — Direct Vulkan 3D rendering with shared device/swapchain and custom pipelines.
- **[`slpack`](src/examples/slpack/slpack.cpp)** / **[`slunpack`](src/examples/slunpack/slunpack.cpp)** — Asset packaging tools.

---


## Documentation Generation

If Doxygen is installed, generate complete HTML API documentation with:

```bash
cmake --build build --target docs
```

# simlib

`simlib` is an easy-to-use, feature-rich C++ multimedia and rendering framework for 2D and 3D applications, with an Allegro 4-style API built on SDL2 and selectable OpenGL, Vulkan and Direct3D 12 backends.

It combines immediate-mode 2D drawing and automatic batching with a powerful 3D rendering system, including meshes, materials, terrain, voxel worlds, lighting, normal mapping, post-processing and GPU compute. Beyond graphics, simlib provides integrated audio, video playback, physics, fluid simulation, procedural generation and Lua scripting — giving you the building blocks to create games, visual simulations and interactive applications in one library.

**Documentation lives in the [simlib wiki](https://github.com/daver64/rsimlib/wiki)**: tutorial, API reference, guides, the example gallery and the showreels. The pages are kept in [docs/](docs/tutorial/README.md) and regenerated into the wiki by `tools/sync-wiki.py`.

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

## Building

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

Dependencies, install prefixes, Windows and Visual Studio builds, consuming the installed library and packaging an app are in [docs/building.md](docs/building.md).

## Running the examples

Every graphical example runs on OpenGL by default on Linux and Direct3D 12 on Windows, and accepts `--gl`, `--vulkan` or `--d3d12` (`--d3d12` is Windows only). Browse them in the [Example Gallery](docs/examples/README.md) or start with the [Showcase Guide](docs/showcases.md) (`./build/showcase2d`, `showcase3d`, `showcasevoxel`).

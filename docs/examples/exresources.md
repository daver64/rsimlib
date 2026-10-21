# exresources

ZIP resource archives: opens `resource_demo.zip`, lists entries and previews a Lua script from inside it.

![exresources screenshot](img/exresources.png)

## What it shows
- Opening an archive
- Listing entries
- Reading `scene.lua` from the archive

## Key code

In `exresources.cpp`, `Archive` opens the ZIP, locates its script entry, reads its bytes, and loads the archived texture.
```cpp
    const std::string archive_path = argc > 1 ? argv[1] : "resource_demo.zip";
    sl::Archive archive;
    if (!archive.open(archive_path))
    {
        std::fprintf(stderr, "Unable to open %s: %s\n", archive_path.c_str(), sl::last_error().c_str());
        return -1;
    }

    const std::string texture_entry = archive.contains("assets/textures/balloon_red.png")
                                          ? "assets/textures/balloon_red.png"
                                          : "textures/balloon_red.png";
    const std::string script_entry = archive.contains("assets/scripts/scene.lua")
                                         ? "assets/scripts/scene.lua"
                                         : "scripts/scene.lua";
    sl::Bitmap *texture = sl::load_bitmap(archive, texture_entry);
    const std::vector<std::uint8_t> script_bytes = archive.read(script_entry);
```

## Running
```bash
./build/exresources
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Run from the repository root so `resource_demo.zip` is found; the only arguments accepted are backend flags.

## Controls
| Input | Action |
|-------|--------|
| Escape | Exit |

## Key APIs
- Resources — [Resource Archives](../api_reference.md#resource-archives-includeresourceh) · [`resource.h`](../../include/resource.h)

## Source
[src/examples/exresources/exresources.cpp](../../src/examples/exresources/exresources.cpp)

## See also
- [exlua](exlua.md)
- [exconfig](exconfig.md)

See the [`slpack`/`slunpack`](../../README.md) tools for creating archives.

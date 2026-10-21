# exresources

ZIP resource archives: opens `resource_demo.zip`, lists entries and previews a Lua script from inside it.

![exresources screenshot](img/exresources.png)

## What it shows
- Opening an archive
- Listing entries
- Reading `scene.lua` from the archive

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

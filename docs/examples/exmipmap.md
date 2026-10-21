# exmipmap

Texture filtering comparison: three textured planes shown with bilinear, mipmapped and mipmapped 16x anisotropic sampling.

![exmipmap screenshot](img/exmipmap.png)

## What it shows
- Creating plane meshes and a checker texture
- Per-material filtering settings: bilinear, mipmaps and anisotropy
- A swaying camera that makes filtering differences visible
- Rendering through a `Surface3D`

## Running
```bash
./build/exmipmap
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Shows a notice instead of 3D content on backends without 3D surface support.

## Controls
| Input | Action |
|-------|--------|
| Space | Pause or resume the camera sway |
| Escape | Exit |

## Key APIs
- `create_surface_3d`, `begin_surface_3d`, `end_surface_3d` — [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh) · [`surface3d.h`](../../include/surface3d.h)
- `create_plane_mesh`, `draw_mesh`, `Material`, `Camera`, `set_light_3d` — [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh) · [`surface3d.h`](../../include/surface3d.h)

## Source
[src/examples/exmipmap/exmipmap.cpp](../../src/examples/exmipmap/exmipmap.cpp)

## See also
- [ex3d_surface](ex3d_surface.md)
- [expbr](expbr.md)
- [exnormalmap3d](exnormalmap3d.md)

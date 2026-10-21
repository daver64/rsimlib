# exnormalmap3d

3D normal mapping: a cube and sphere with a procedural normal-mapped material under a 3D light, with a toggle to compare.

![exnormalmap3d screenshot](img/exnormalmap3d.png)

## What it shows
- Drawing meshes into a `Surface3D` with `begin_surface_3d` / `end_surface_3d`
- Creating cube and sphere meshes and a `Material`
- Normal-mapped lighting with `set_light_3d`
- Showing the 3D surface bitmap on the 2D screen

## Running
```bash
./build/exnormalmap3d
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Prints a message and shows no 3D content on backends without 3D surface support.

## Controls
| Input | Action |
|-------|--------|
| Space | Toggle 3D normal mapping |
| Escape | Exit |

## Key APIs
- `create_surface_3d`, `begin_surface_3d`, `end_surface_3d`, `surface_3d_bitmap` — [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh) · [`surface3d.h`](../../include/surface3d.h)
- `create_cube_mesh`, `create_sphere_mesh`, `draw_mesh`, `Material`, `Camera`, `set_light_3d` — [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh) · [`surface3d.h`](../../include/surface3d.h)

## Source
[src/examples/exnormalmap3d/exnormalmap3d.cpp](../../src/examples/exnormalmap3d/exnormalmap3d.cpp)

## See also
- [exnormalmap](exnormalmap.md)
- [expbr](expbr.md)
- [ex3d_surface](ex3d_surface.md)

See also the [Graphics Effects guide](../graphics_effects.md).

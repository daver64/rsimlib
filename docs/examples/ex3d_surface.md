# ex3d_surface

Backend-independent 3D with `Surface3D`: procedural meshes, an OBJ pyramid loaded from memory, lighting and MSAA, composited into the 2D screen.

![ex3d_surface screenshot](img/ex3d_surface.png)

## What it shows
- `create_surface_3d` with MSAA and `surface_3d_bitmap` compositing
- Procedural cube, plane and sphere meshes
- `load_obj_mesh_from_memory` and `load_model`
- `set_light_3d` lighting

## Running
```bash
./build/ex3d_surface
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Set `RSIMLIB_SCREENSHOT=<file.png>` to save a screenshot on an early frame.

## Controls
| Input | Action |
|-------|--------|
| Escape | Exit |

## Key APIs
- `create_surface_3d`, `begin_surface_3d`, `end_surface_3d`, `surface_3d_bitmap` — [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh) · [`surface3d.h`](../../include/surface3d.h)
- `create_cube_mesh`, `create_plane_mesh`, `create_sphere_mesh`, `draw_mesh` — [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh) · [`surface3d.h`](../../include/surface3d.h)
- `load_model`, `draw_model`, `set_light_3d` — [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh) · [`surface3d.h`](../../include/surface3d.h)

## Source
[src/examples/ex3d_surface/ex3d_surface.cpp](../../src/examples/ex3d_surface/ex3d_surface.cpp)

## See also
- [exmodel3d](exmodel3d.md)
- [exinstancing](exinstancing.md)
- [ex3d_gl](ex3d_gl.md)

# ex3d_surface

Backend-independent 3D with `Surface3D`: procedural meshes, an OBJ pyramid loaded from memory, lighting and MSAA, composited into the 2D screen.

![ex3d_surface screenshot](img/ex3d_surface.png)

## What it shows
- `create_surface_3d` with MSAA and `surface_3d_bitmap` compositing
- Procedural cube, plane and sphere meshes
- `load_obj_mesh_from_memory` and `load_model`
- `set_light_3d` lighting

## Key code

In `ex3d_surface.cpp`, the MSAA surface, procedural meshes, OBJ-from-memory mesh and model files are created up front:

```cpp
sl::Surface3D *surface = sl::create_surface_3d(640, 480, 4);
sl::Mesh *cube = sl::create_cube_mesh(1.0f, {240, 120, 80});
sl::Mesh *sphere = sl::create_sphere_mesh(0.5f, 32, 16, {90, 170, 255});
sl::Mesh *pyramid = sl::load_obj_mesh_from_memory(pyramid_obj, std::strlen(pyramid_obj), {255, 220, 90});
sl::Mesh *plane = sl::create_plane_mesh(8.0f, 8.0f, {120, 130, 120});
sl::Model *crate = sl::load_model(model_path("crate.obj"));
```

Lighting is set with `set_light_3d`, and each frame the 3D scene is drawn between `begin_surface_3d` and `end_surface_3d`, with a 2D HUD drawn into the surface bitmap:

```cpp
sl::set_light_3d({{-0.4f, -1.0f, -0.6f}, 0.25f});
// ...
if (sl::begin_surface_3d(surface, camera))
{
    sl::clear_surface_3d({20, 30, 60});
    sl::draw_mesh(cube, sl::model_matrix({-0.8f, 0, 0}, {time * 0.7f, time, 0}), {{255, 255, 255}, true, checker});
    if (crate)
        sl::draw_model(crate, sl::model_matrix({-2.2f, 0.0f, -1.0f}, {0, time * 0.4f, 0}));
    sl::rectfill(sl::surface_3d_bitmap(surface), 16.0f, 16.0f, 250.0f, 68.0f, {15, 20, 35, 200});
    sl::end_surface_3d();
}
sl::draw_sprite(sl::surface_3d_bitmap(surface), 80, 60);
```

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

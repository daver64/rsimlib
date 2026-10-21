# exnormalmap3d

3D normal mapping: a cube and sphere with a procedural normal-mapped material under a 3D light, with a toggle to compare.

![exnormalmap3d screenshot](img/exnormalmap3d.png)

## What it shows
- Drawing meshes into a `Surface3D` with `begin_surface_3d` / `end_surface_3d`
- Creating cube and sphere meshes and a `Material`
- Normal-mapped lighting with `set_light_3d`
- Showing the 3D surface bitmap on the 2D screen

## Key code

In `exnormalmap3d.cpp`, a `Surface3D` and the cube and sphere meshes are created up front.

```cpp
    sl::Surface3D *surface = sl::create_surface_3d(640, 480, 4);
    sl::Mesh *cube = sl::create_cube_mesh(1.2f);
    sl::Mesh *sphere = sl::create_sphere_mesh(0.7f, 48, 24);
```

Between `begin_surface_3d` and `end_surface_3d`, a moving light and a `Material` with a normal map are used to draw both meshes.

```cpp
        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({15, 18, 30});
            // The light circles the scene so the studs' shading visibly moves.
            sl::set_light_3d({{-std::cos(time), -0.5f, -std::sin(time)}, 0.12f});
            sl::Material material;
            material.texture = colour;
            material.normal_map = use_normals ? normal : nullptr;
            sl::draw_mesh(cube, sl::model_matrix({-1.1f, 0, 0}, {0.4f, 0.6f, 0}), material);
            sl::draw_mesh(sphere, sl::model_matrix({1.1f, 0, 0}, {0, time * 0.3f, 0}), material);
            sl::end_surface_3d();
        }
```

The 3D surface's bitmap is then shown on the 2D screen.

```cpp
        sl::clear_to_colour(sl::screen, {40, 40, 40});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 80, 60);
```

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

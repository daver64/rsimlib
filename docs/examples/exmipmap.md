# exmipmap

Texture filtering comparison: three textured planes shown with bilinear, mipmapped and mipmapped 16x anisotropic sampling.

![exmipmap screenshot](img/exmipmap.png)

## What it shows
- Creating plane meshes and a checker texture
- Per-material filtering settings: bilinear, mipmaps and anisotropy
- A swaying camera that makes filtering differences visible
- Rendering through a `Surface3D`

## Key code

In `exmipmap.cpp`, the checker texture's filtering state (bilinear, mipmaps, anisotropy) is set on the bitmap before its first draw.

```cpp
// These are read when the GPU texture is first created, so set them before the first draw.
bitmap->smooth = true;
bitmap->mipmaps = mipmaps;
bitmap->anisotropy = anisotropy;
```

A `Surface3D` and plane mesh are created, with one texture per filtering mode.

```cpp
sl::Surface3D *surface = sl::create_surface_3d(900, 480, 4);
sl::Mesh *plane = sl::create_plane_mesh(2.0f, 60.0f);
// One texture per mode, since the sampling state is fixed per texture.
sl::Bitmap *textures[3] = {make_checker(512, false, 1.0f), make_checker(512, true, 1.0f),
                           make_checker(512, true, 16.0f)};
```

Each frame the camera sways sideways and the three planes are drawn into the surface with their own materials.

```cpp
camera.position = {std::sin(phase) * 1.5f, 0.6f, 3.0f};
if (sl::begin_surface_3d(surface, camera))
{
    sl::clear_surface_3d({140, 170, 210});
    sl::set_light_3d({{-0.3f, -1.0f, -0.4f}, 0.6f});
    for (int i = 0; i < 3; ++i)
    {
        sl::Material material;
        material.texture = textures[i];
        sl::draw_mesh(plane, sl::model_matrix({(i - 1) * 2.2f, 0.0f, -27.0f}, {0, 0, 0}), material);
    }
    sl::end_surface_3d();
}
```

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

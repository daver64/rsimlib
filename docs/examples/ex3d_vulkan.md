# ex3d_vulkan

A minimal raw Vulkan 3D example: a spinning model drawn with Vulkan-specific buffers and shaders. Vulkan only.

![ex3d_vulkan screenshot](img/ex3d_vulkan.png)

## What it shows
- Selecting the Vulkan backend in code
- Vulkan pipeline and buffer setup
- `Camera` and `model_matrix` transforms

## Running
```bash
./build/ex3d_vulkan
```

Always uses the Vulkan backend; it takes no backend flag.

## Controls
| Input | Action |
|-------|--------|
| Escape | Exit |

## Key APIs
- `Camera`, `model_matrix` — [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh) · [`surface3d.h`](../../include/surface3d.h)
- `set_graphics_backend` — [Application Lifecycle](../api_reference.md#application-lifecycle--display-includedisplayh) · [`display.h`](../../include/display.h)

## Source
[src/examples/ex3d_vulkan/ex3d_vulkan.cpp](../../src/examples/ex3d_vulkan/ex3d_vulkan.cpp)

## See also
- [ex3d_gl](ex3d_gl.md)
- [ex3d_surface](ex3d_surface.md)
- [exvulkan](exvulkan.md)

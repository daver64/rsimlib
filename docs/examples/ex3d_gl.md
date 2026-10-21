# ex3d_gl

A minimal raw OpenGL 3D example: a spinning model drawn with a custom shader and the simlib `Camera`. OpenGL only.

![ex3d_gl screenshot](img/ex3d_gl.png)

## What it shows
- Using the OpenGL backend directly alongside simlib
- Compiling a custom 3D shader
- `Camera` and `model_matrix` transforms

## Running
```bash
./build/ex3d_gl
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Requires the OpenGL backend; run with `--gl`.

## Controls
| Input | Action |
|-------|--------|
| Escape | Exit |

## Key APIs
- `Camera`, `model_matrix` — [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh) · [`surface3d.h`](../../include/surface3d.h)
- `set_graphics_backend`, `graphics_backend` — [Application Lifecycle](../api_reference.md#application-lifecycle--display-includedisplayh) · [`display.h`](../../include/display.h)

## Source
[src/examples/ex3d_gl/ex3d_gl.cpp](../../src/examples/ex3d_gl/ex3d_gl.cpp)

## See also
- [ex3d_surface](ex3d_surface.md)
- [ex3d_vulkan](ex3d_vulkan.md)

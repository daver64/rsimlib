# exshader

Loads a GLSL vertex and fragment shader from files and uses it to tint a sprite.

![exshader screenshot](img/exshader.png)

## What it shows
- Loading shader source with `Shader::load_files`
- Picking the vertex shader per backend (OpenGL or Vulkan)
- Binding the shader while drawing a bitmap
- Reporting shader errors with `Shader::error`

## Running
```bash
./build/exshader
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Reads `shaders/glsl/tint.frag` and the default 2D vertex shader. On Vulkan, runtime GLSL compilation needs `glslangValidator`.

## Controls
| Input | Action |
|-------|--------|
| Escape, or closing the window | Exit |

## Key APIs
- `Shader`, `Shader::load_files`, `Shader::error` — [Shaders](../api_reference.md#shaders-compute--storage-buffers-includegraphics_fxh) · [`graphics_fx.h`](../../include/graphics_fx.h)
- `load_bitmap`, `time_ms` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)
- `graphics_backend` — [Application Lifecycle](../api_reference.md#application-lifecycle--display-includedisplayh) · [`display.h`](../../include/display.h)

## Source
[src/examples/exshader/exshader.cpp](../../src/examples/exshader/exshader.cpp)

## See also
- [excompute](excompute.md)
- [expostprocess](expostprocess.md)

# exshader

Loads a GLSL vertex and fragment shader from files and uses it to tint a sprite.

![exshader screenshot](img/exshader.png)

## What it shows
- Loading shader source with `Shader::load_files`
- Picking the vertex shader per backend (OpenGL or Vulkan)
- Binding the shader while drawing a bitmap
- Reporting shader errors with `Shader::error`

## Key code

In `exshader.cpp`, the vertex shader is chosen per backend and `Shader::load_files` loads it with the fragment shader, with `Shader::error` reporting failures.

```cpp
    sl::Shader shader;
    const char *vertex_shader = sl::graphics_backend() == sl::GraphicsBackend::vulkan
                                    ? "shaders/vulkan/vulkan_2d.vert"
                                    : "shaders/glsl/default_2d.vert";
    const bool loaded = shader.load_files(
        vertex_shader,
        "shaders/glsl/tint.frag",
        {"uTexture"});
    // ...
    if (!loaded)
    {
        std::fprintf(stderr, "Shader error: %s\n", shader.error().c_str());
    }
```

Each frame, the shader is used to draw the bitmap with `draw_textured_quad`.

```cpp
            shader.draw_textured_quad(texture, x - 96.0f, y, 192.0f, 192.0f);
```

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

# ex3d_vulkan

A minimal raw Vulkan 3D example: a spinning model drawn with Vulkan-specific buffers and shaders. Vulkan only.

![ex3d_vulkan screenshot](img/ex3d_vulkan.png)

## What it shows
- Selecting the Vulkan backend in code
- Vulkan pipeline and buffer setup
- `Camera` and `model_matrix` transforms

## Key code

In `ex3d_vulkan.cpp`, the Vulkan backend is selected in code before the graphics mode is set.

```cpp
    if (!sl::set_graphics_backend(sl::GraphicsBackend::vulkan) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        std::fprintf(stderr, "Failed to set Vulkan graphics backend or graphics mode.\n");
        return -1;
    }
```

The pipeline and vertex buffer are set up through the Vulkan context.

```cpp
    if (!context->load_shader_module(shader_dir / "vulkan/vulkan_ex3d.vert.spv", vertex_module, error) ||
        !context->load_shader_module(shader_dir / "vulkan/vulkan_ex3d.frag.spv", fragment_module, error) ||
        !context->create_dynamic_descriptor_layout({}, descriptor_layout, error) ||
        !context->create_graphics_pipeline(vertex_module, fragment_module, descriptor_layout,
                                           sl::detail::PrimitiveType::triangles, pipeline, error, nullptr, 0, true))
```

Each frame, `Camera` and `model_matrix` build the MVP matrix that is passed to the 3D draw.

```cpp
        const glm::mat4 model = sl::model_matrix({0.0f, 0.0f, 0.0f}, {time * 0.7f, time, time * 0.35f});
        const glm::mat4 view = camera.view_matrix();
        const glm::mat4 projection = camera.projection_matrix(800.0f / 600.0f);
        const glm::mat4 mvp = projection * view * model;
```

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

# Renderer Backends & Architecture

[← Back to README](../README.md)

The window/context, presentation lifecycle, 2D texture and render-target resources, shader operations, and 2D vertex submission are routed through an internal renderer interface in `src/engine/renderer.*`.

---

## Backend Selection

`simlib` supports selectable rendering backends:
- **OpenGL 4.3** (Default)
- **Vulkan**
- **Direct3D 11** (Windows only)
- **Direct3D 12** (Windows only, boilerplate)

Select the backend programmatically before calling `set_gfx_mode()`, or via command-line arguments:

```cpp
// Option 1: Automatic command-line parsing (--gl, --vulkan, --d3d11, --d3d12)
sl::configure_graphics_backend_from_args(argc, argv);

// Option 2: Explicit code selection
sl::set_graphics_backend(sl::GraphicsBackend::vulkan);

sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600);
```

Check compiled support with `sl::graphics_backend_available(backend)`.

---

## Automatic Hardware Batching

All 2D primitives, outline/filled shapes, font text, particles, and sprites are automatically decomposed into GPU-friendly triangle and line streams. The active renderer accumulates vertices into a dynamic staging buffer and flushes them in a single draw call per batch.

### Auto-Flush Triggers
Batches automatically flush when:
- Texture handle changes (sprites using different textures)
- Primitive topology changes (e.g., lines vs triangles)
- Active shader program changes
- Blend mode changes (e.g., standard vs premultiplied alpha)
- Render targets are switched (`begin_render_target()` / `end_render_target()`)
- Frame presentation (`show_video_bitmap()`, `end_frame()`, `clear()`)
- ImGui rendering (`sl::gui::render()`)

---

## Custom Runtime Shaders & SPIR-V Compilation

Arbitrary runtime GLSL shaders work on both OpenGL and Vulkan backends. On OpenGL, `Shader::load()` compiles and links GLSL directly. On Vulkan, `Shader::load()` uses `glslangValidator` at runtime to compile GLSL into SPIR-V, dynamically building matching descriptor layouts and pipeline state:

```cpp
sl::Shader tint;
tint.load(vertexGlsl, fragmentGlsl,
    {"uTexture"},                              // fragment sampler names, in binding order
    {{"uTintColor", 0, sizeof(float) * 3}});   // {name, byte offset, byte size} in push constants
```

The orthographic projection uniform `uProjection` (`mat4`) is always automatically bound.

### Multitexturing & Multi-Sampler Shaders

You can bind multiple textures to custom shaders across OpenGL and Vulkan using the `set_texture()` helpers or standalone `sl::bind_texture()`:

```cpp
sl::Shader multiShader;
multiShader.load(
    vertexGlsl,
    fragmentGlsl,
    {"uBaseTexture", "uNormalMap", "uLightmap"}, // Vulkan bindings 0, 1, 2
    {{"uIntensity", 0, sizeof(float)}}           // Push constant uniforms
);

// Method 1: Bind directly through the shader instance (sets unit + uniform on GL, updates descriptor on Vulkan)
multiShader.set_texture("uNormalMap", 1, normalBitmap);
multiShader.set_texture("uLightmap", 2, lightmapBitmap);

// Method 2: Global texture unit binding
sl::bind_texture(1, normalBitmap);

// Draw the primary quad with texture unit 0
multiShader.draw_textured_quad(baseBitmap, x, y, width, height);
```

### Offline Shader Validation
When `glslangValidator` and `spirv-val` are installed, CMake provides validation targets:

```bash
cmake --build build --target shader_validation
cmake --build build --target vulkan_shader_validation
```

---

## Direct 3D Escape Hatches

`simlib` provides examples demonstrating direct 3D graphics without imposing a rigid 3D engine layer:
- **[`ex3d`](../src/examples/ex3d/ex3d.cpp)**: Direct OpenGL 3D with VAO/VBOs, custom depth testing, and GLM camera matrices.
- **[`ex3d_vulkan`](../src/examples/ex3d_vulkan/ex3d_vulkan.cpp)**: Direct Vulkan 3D using the engine's shared Vulkan device, swapchain, depth attachments, custom pipelines, and MVP push constants.

---

[← Back to README](../README.md)

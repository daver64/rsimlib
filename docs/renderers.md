# Renderer Backends & Architecture

[← Back to README](../README.md)

The window/context, presentation lifecycle, 2D texture and render-target resources, shader operations, and 2D vertex submission are routed through an internal renderer interface in `src/engine/renderer.*`.

---

## Backend Selection

`simlib` supports selectable rendering backends:
- **OpenGL 4.3** (default on Linux)
- **Vulkan**
- **Direct3D 12** (Windows only, default on Windows)

Select the backend programmatically before calling `set_gfx_mode()`, or via command-line arguments:

```cpp
// Option 1: Automatic command-line parsing (--gl, --vulkan, --d3d12)
sl::configure_graphics_backend_from_args(argc, argv);

// Option 2: Explicit code selection
sl::set_graphics_backend(sl::GraphicsBackend::vulkan);

sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600);
```

Check compiled support with `sl::graphics_backend_available(backend)`.

Which backends are compiled is a configure-time choice: `-DSIMLIB_RENDERERS="gl;vulkan;d3d12"` (default `gl;vulkan`, plus `d3d12` on Windows; at least one is required, `d3d12` is Windows-only). Only the listed backends are built and linked, and `SIMLIB_RENDERER_GL`, `SIMLIB_RENDERER_VULKAN` and `SIMLIB_RENDERER_D3D12` are defined for consumers. The default backend is the first enabled one, and selecting a disabled backend fails with an error from `last_error()`.

**The backend is fixed for the life of the process.** Choose it once, before `set_gfx_mode()`, and keep it until `shutdown()`. Switching backends at runtime (for example calling `set_gfx_mode()` again after `set_graphics_backend()` with a different backend) is not supported. Bitmaps, render targets, meshes and other GPU resources are tied to the backend that created them and are not migrated, so handles from the old backend become invalid. To change backend, restart the application.

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

### Direct3D 12 shaders

The Direct3D 12 backend translates GLSL to HLSL (glslang, then SPIR-V, then SPIRV-Cross) and compiles it with DXC to
shader model 6.x (up to 6.6, as supported by the device). It supports user shaders, compute, storage buffers,
`image2D` storage textures, vertex-stage samplers and up to 32 samplers per shader. If `dxcompiler.dll` and
`dxil.dll` are missing (`vcpkg install directx-dxc:x64-windows`), it falls back to fxc at shader model 5.1,
limited to 16 samplers per shader. Ship both DLLs next to your executable.

---

## Threading and Start-up Cost

Graphics calls (drawing, `create_mesh`, texture upload) belong on the main thread on every backend. Decode and
simulate on your own threads if you like; for assets use `load_bitmap_async()` / `load_model_async()`, which decode
on workers and upload a bounded amount per `end_frame()` (see `set_async_upload_budget()`), so loading does not stall.

On Vulkan and Direct3D 12, `create_surface_3d()` builds the 2D, mesh, instanced, terrain and sky pipelines up front
so the first draws do not hitch. `FrameStats::pipelines_created` should stay flat once a scene is running.

## Surface3D

The backend-neutral `Surface3D` API (`include/surface3d.h`) is implemented for OpenGL, Vulkan and Direct3D 12. All three backends support true MSAA (`create_surface_3d(w, h, samples)`). Use the escape hatches below for raw access.

**Blending:** `Material::blend` (`MaterialBlend::alpha` or `additive`) works for 3D meshes and instanced meshes on all three backends (the Direct3D 12 path has not been tested on Windows yet). Blended draws do not write depth and are not sorted, so draw them after the opaque geometry, farthest first.

## Direct 3D Escape Hatches

`simlib` provides examples demonstrating direct 3D graphics without imposing a rigid 3D engine layer:
- **[`ex3d_gl`](../src/examples/ex3d_gl/ex3d_gl.cpp)**: Direct OpenGL 3D with VAO/VBOs, custom depth testing, and GLM camera matrices.
- **[`ex3d_vulkan`](../src/examples/ex3d_vulkan/ex3d_vulkan.cpp)**: Direct Vulkan 3D using the engine's shared Vulkan device, swapchain, depth attachments, custom pipelines, and MVP push constants.
- **[`ex3d_d3d12`](../src/examples/ex3d_d3d12/ex3d_d3d12.cpp)**: Direct3D 12 3D escape hatch.

---

[← Back to README](../README.md)

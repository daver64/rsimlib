# Graphics Effects & 2D Lighting

[← Back to README](../README.md)

`simlib` includes configurable post-process effects for render targets and bitmaps, as well as a 2D dynamic lighting engine with soft polygon shadow casting.

---

## Post-Processing Effects

Post-processing effects operate on render targets and bitmaps. [`expostprocess`](../src/examples/expostprocess/expostprocess.cpp) shows how to compose them with `PingPongBuffer`, including an interactive Shockwave triggered from the mouse position.

### Lifecycle Note
Effects that own GPU resources must be initialized after `set_gfx_mode()` and shut down before the display is destroyed. After the final frame, call `wait_for_graphics()` once before releasing GPU-backed effects, render targets, bitmaps, or ImGui resources; call `shutdown()` last.

### Available Effects

| Type | Main API | Description |
| --- | --- | --- |
| `Shader` | `load`, `use`, `set_uniform`, `reset` | Load a built-in or custom GLSL shader, bind it, set uniforms, and release it. Custom GLSL is compiled to SPIR-V at runtime on Vulkan. |
| `PingPongBuffer` | `initialise`, `begin`, `source`, `target`, `advance`, `shutdown` | Alternate between two render targets while chaining fullscreen effects without allocating one bitmap per pass. |
| `Bloom` | `initialise`, `set_threshold`, `set_intensity`, `set_radius`, `set_downsample`, `apply`, `shutdown` | Extract bright pixels, blur them, and composite the glow over a bitmap. |
| `Vignette` | `initialise`, `set_radius`, `set_softness`, `set_intensity`, `apply`, `shutdown` | Darken the edges of a bitmap around its centre. |
| `ColourAdjust` | `initialise`, `set_brightness`, `set_contrast`, `set_saturation`, `set_exposure`, `apply`, `shutdown` | Adjust brightness, contrast, saturation, and exposure. |
| `Blur` | `initialise`, `set_radius`, `set_iterations`, `apply`, `shutdown` | Apply a separable Gaussian blur using temporary render targets. |
| `ChromaticAberration` | `initialise`, `set_strength`, `apply`, `shutdown` | Separate RGB samples toward the edges for lens and damage effects. |
| `Pixelate` | `initialise`, `set_pixel_size`, `apply`, `shutdown` | Reduce a bitmap to configurable screen-space colour blocks. |
| `RadialBlur` | `initialise`, `set_centre`, `set_strength`, `set_samples`, `apply`, `shutdown` | Blur along rays from a focal point. |
| `HeatHaze` | `initialise`, `set_strength`, `set_frequency`, `set_time`, `apply`, `shutdown` | Apply animated sinusoidal UV distortion. |
| `Shockwave` | `initialise`, `set_centre`, `set_radius`, `set_width`, `set_strength`, `apply`, `shutdown` | Push pixels outward along an expanding ring centered on a focal point. |
| `CRTFilter` | `initialise`, `set_pixel_size`, `set_scanline_strength`, `set_curvature`, `apply`, `shutdown` | Combine scanlines, light curvature, and pixelation for a CRT look. |
| `DitherFilter` | `initialise`, `set_pixel_size`, `set_levels`, `apply`, `shutdown` | Apply ordered Bayer dithering to reduce colour depth. |
| `FilmGrain` | `initialise`, `set_strength`, `set_time`, `apply`, `shutdown` | Add animated monochrome film grain with configurable intensity. |
| `LightingPass` | `initialise`, `set_ambient`, `set_light_height`, `apply`, `shutdown` | Apply coloured radial lights and polygon-caster shadows to a bitmap. |
| `ScreenFade` | `set_colour`, `colour`, `apply` | Draw a solid colour overlay, including alpha, over the current screen. |
| `ScreenShake` | `trigger`, `update`, `clear`, `active` | Apply a decaying camera offset to 2D projections. |

### Chaining Effects with PingPongBuffer

To chain fullscreen effects, render a scene into a source render target, then alternate the source and target with `PingPongBuffer` for each pass:

```cpp
sl::PingPongBuffer effects;
effects.initialise(800, 600);

sl::ColourAdjust colour_adjust;
sl::Blur blur;
colour_adjust.initialise();
blur.initialise();

sl::Bitmap *result = scene;

effects.begin(result);
sl::begin_render_target(effects.target());
sl::clear_render_target({0, 0, 0});
colour_adjust.apply(effects.source(), 0, 0, 800, 600);
sl::end_render_target();
result = effects.advance();

effects.begin(result);
sl::begin_render_target(effects.target());
sl::clear_render_target({0, 0, 0});
blur.apply(effects.source(), 0, 0, 800, 600);
sl::end_render_target();
result = effects.advance();

sl::draw_sprite(result, 0.0f, 0.0f);
```

---

## 2D Dynamic Lighting & Shadows

`LightingPass` accepts an arbitrary number of lights per `LightingPass::apply()` call through backend-specific shader storage buffers. The first eight lights can cast dynamic geometric polygon shadows; additional lights provide unshadowed illumination.

### Light Properties

| Field | Description |
| --- | --- |
| `x`, `y` | Light position in pixels, using top-left origin. |
| `radius` | Maximum illumination distance in pixels. |
| `intensity` | Brightness multiplier for this light. |
| `shadow_softness` | Shadow-mask filter radius in pixels (`0` produces hard shadows). |
| `direction_x`, `direction_y` | Direction vector in screen space (used when spotlight cone angles are non-zero). |
| `inner_angle`, `outer_angle` | Inner and outer spotlight cone angles in degrees (`0` disables cone falloff for omni point light). |
| `colour` | RGB light colour. |

### Shadow Casters

`ShadowCaster` describes a polygon that blocks light. Its `vertices` are screen-space pixel coordinates using the top-left origin, ordered around the polygon perimeter. Use `make_rectangle_shadow_caster(left, top, right, bottom)` when a rectangle is the most convenient representation. The implementation projects polygon edges away from each light and supports soft shadow filtering through `shadow_softness`.

### Normal Mapping

Sprites and tiles can carry a tangent-space normal map (RGB = XYZ, green pointing up the image, as exported by most tools) so lights shade their surface detail.

1. Pair each colour bitmap or atlas with a normal map of the **same size and layout** using `set_normal_map(bitmap, normal_map)`. The pairing is not owned; keep the normal map alive while it is used.
2. Draw the scene to `scene` as usual.
3. Redraw the same sprites/tiles between `begin_normal_pass(normals)` and `end_normal_pass()`, where `normals` is a render target the size of the scene. Inside the pass each draw writes its paired normal map instead of its colour; bitmaps without a normal map are skipped, and pixels left unwritten are lit as flat (so un-mapped content looks as it did before). Only draw sprites and tiles in a normal pass, so `draw_tilemap()` works directly.
4. Light with `lighting.apply(scene, normals, lights, ...)`. `set_light_height()` sets how far, in pixels, lights sit above the surface (default 64); lower values give more dramatic shading.

```cpp
sl::set_normal_map(brick, brick_normal);
// ... draw scene into `scene` ...
sl::begin_normal_pass(normals);
sl::draw_sprite(brick, x, y);          // writes brick_normal
sl::end_normal_pass();
lighting.set_light_height(48.0f);
lighting.apply(scene, normals, {light});
```

Rotated sprites and horizontal, vertical and diagonal flips (including tile flips) transform their normals correctly on OpenGL, Vulkan and Direct3D 12, so highlights stay on the lit side. Only sprite and tile draws are supported inside a normal pass. Normal-mapped lighting works on OpenGL, Vulkan, and Direct3D 12. D3D12 effects require the GLSL-to-HLSL shader translation dependencies described in [D3D12 shaders](./renderers.md#d3d12-shaders). See `exnormalmap` for a runnable example.

### Lighting Example

Render the scene to an offscreen target before applying lighting:

```cpp
sl::Bitmap *scene = sl::create_render_target(800, 600);
sl::LightingPass lighting;
lighting.initialise();
lighting.set_ambient(0.18f);

const std::vector<sl::ShadowCaster> casters = {
    sl::make_rectangle_shadow_caster(300.0f, 370.0f, 500.0f, 400.0f),
    {{{170.0f, 390.0f}, {230.0f, 320.0f}, {290.0f, 390.0f}}},
};

sl::begin_render_target(scene);
sl::clear_render_target({45, 48, 56});
// Draw the scene here...
sl::end_render_target();

sl::Light light;
light.x = 400.0f;
light.y = 240.0f;
light.radius = 320.0f;
light.intensity = 1.2f;
light.shadow_softness = 3.0f;
light.colour = {255, 190, 110};

lighting.apply(scene, light, 0, 0, 800, 600, true, casters);
sl::destroy_bitmap(scene);
lighting.shutdown();
```

---

[← Back to README](../README.md)

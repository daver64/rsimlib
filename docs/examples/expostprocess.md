# expostprocess

A gallery of full-screen post-processing effects that can be toggled live over an animated scene.

![expostprocess screenshot](img/expostprocess.png)

## What it shows
- Rendering a scene into a render target and applying effects through a `PingPongBuffer`
- Bloom, vignette, colour adjust, blur, chromatic aberration and pixelate
- Radial blur, heat haze, CRT filter, dither and film grain
- Triggered effects: screen shake and shockwave
- Per-effect readiness checks so unsupported effects are skipped

## Key code

In `expostprocess.cpp`, the scene is drawn into a render target and a `PingPongBuffer` is set up; each effect has a `*_ready` flag from its `initialise()` so unsupported ones are skipped:

```cpp
sl::Bitmap *scene = sl::create_render_target(800, 600);
sl::PingPongBuffer post_process;
post_process.initialise(800, 600);
// ...
const bool bloom_ready = bloom.initialise();
const bool vignette_ready = vignette.initialise();
// ...
else if (event.key() == sl::Event::Key::letter_x && shockwave_ready)
    shockwave_elapsed = 0.0f;
// ...
else if (event.key() == sl::Event::Key::letter_s)
    screen_shake.trigger(12.0f, 0.45f);
```

Each enabled effect reads the previous result from the ping-pong buffer and writes to the other side, with the shockwave driven by its elapsed time:

```cpp
if (shockwave_elapsed >= 0.0f && shockwave_elapsed < 0.9f)
{
    shockwave.set_centre(static_cast<float>(sl::mouse_x()) / 800.0f,
                         static_cast<float>(sl::mouse_y()) / 600.0f);
    shockwave.set_radius(shockwave_elapsed * 0.95f);
    post_process.begin(effect_source);
    sl::begin_render_target(post_process.target());
    sl::clear_render_target({8, 11, 18});
    shockwave.apply(post_process.source(), 0, 0, 800, 600);
    sl::end_render_target();
    effect_source = post_process.advance();
}
```

## Running
```bash
./build/expostprocess
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| B / V / C / L | Toggle Bloom / Vignette / Colour adjust / Blur |
| A / P / R / H / T | Toggle Aberration / Pixelate / Radial blur / Heat haze / CRT |
| D / F | Toggle Dither / Film grain |
| S | Screen shake |
| X | Shockwave |
| Escape | Exit |

## Key APIs
- `PingPongBuffer`, `Bloom`, `Vignette`, `ColourAdjust`, `Blur` — [Post-Processing](../api_reference.md#post-processing-graphics-effects-includegraphics_fxh) · [`graphics_fx.h`](../../include/graphics_fx.h)
- `ChromaticAberration`, `Pixelate`, `RadialBlur`, `HeatHaze`, `CRTFilter` — [Post-Processing](../api_reference.md#post-processing-graphics-effects-includegraphics_fxh) · [`graphics_fx.h`](../../include/graphics_fx.h)
- `DitherFilter`, `FilmGrain`, `ScreenShake`, `Shockwave` — [Post-Processing](../api_reference.md#post-processing-graphics-effects-includegraphics_fxh) · [`graphics_fx.h`](../../include/graphics_fx.h)
- `create_render_target`, `begin_render_target`, `end_render_target` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)

## Source
[src/examples/expostprocess/expostprocess.cpp](../../src/examples/expostprocess/expostprocess.cpp)

## See also
- [exlighting](exlighting.md)
- [exrendertarget](exrendertarget.md)
- [exshader](exshader.md)

See also the [Graphics Effects guide](../graphics_effects.md).

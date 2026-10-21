# expostprocess

A gallery of full-screen post-processing effects that can be toggled live over an animated scene.

![expostprocess screenshot](img/expostprocess.png)

## What it shows
- Rendering a scene into a render target and applying effects through a `PingPongBuffer`
- Bloom, vignette, colour adjust, blur, chromatic aberration and pixelate
- Radial blur, heat haze, CRT filter, dither and film grain
- Triggered effects: screen shake and shockwave
- Per-effect readiness checks so unsupported effects are skipped

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

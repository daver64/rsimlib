# exanimsprite

Animated sprites: looping sprites at different frame rates, a rotated pulsing bar, and click-to-spawn explosion rings.

![exanimsprite screenshot](img/exanimsprite.png)

## What it shows
- Packing frames into an atlas and creating `AnimatedSprite` objects
- Looping animations with different frame durations
- A rotated sprite driven by `draw_rotated`
- One-shot explosion animations managed by `AnimationManager`
- Frame timing with `get_frame_time`

## Running
```bash
./build/exanimsprite
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Mouse click | Spawn an explosion at the cursor |
| Space | Pause or resume the orbs |
| R | Toggle rotation of the bar |
| Escape | Quit |

## Key APIs
- `create_animated_sprite`, `AnimatedSprite`, `AnimationManager`, `create_atlas` — [Atlases](../api_reference.md#atlases--animated-sprites-includeatlash-includeanimated_spriteh) · [`atlas.h`](../../include/atlas.h)
- `create_bitmap`, `putpixel`, `textout` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)
- `get_frame_time`, `mouse_x`, `mouse_y` — [Events](../api_reference.md#events-input--timing-includeeventh-includeinputh-includesystemh-includegamepadh) · [`event.h`](../../include/event.h)

## Source
[src/examples/exanimsprite/exanimsprite.cpp](../../src/examples/exanimsprite/exanimsprite.cpp)

## See also
- [exatlas](exatlas.md)
- [exrotatesprite](exrotatesprite.md)
- [exparticles](exparticles.md)

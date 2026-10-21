# exanimsprite

Animated sprites: looping sprites at different frame rates, a rotated pulsing bar, and click-to-spawn explosion rings.

![exanimsprite screenshot](img/exanimsprite.png)

## What it shows
- Packing frames into an atlas and creating `AnimatedSprite` objects
- Looping animations with different frame durations
- A rotated sprite driven by `draw_rotated`
- One-shot explosion animations managed by `AnimationManager`
- Frame timing with `get_frame_time`

## Key code

In `exanimsprite.cpp`, the atlas feeds looping sprites at different frame durations, and the bar is configured to rotate around its centre.
```cpp
    const sl::Atlas atlas = sl::create_atlas(bitmap, tile, tile);
    sl::AnimationManager manager;
    std::vector<int> orbs;
    // Orbs share one animation at different speeds; the first index of each row is its start tile.
    const float durations[] = {0.12f, 0.06f, 0.03f, 0.016f};
    for (int i = 0; i < 4; ++i)
    {
        sl::AnimatedSprite orb = sl::create_animated_sprite(atlas, 0, frames, 120 + i * 180, 120, 96, 96, durations[i]);
        orbs.push_back(manager.add_animated_sprite(orb));
    }

    // A rotated sprite: the bar uses its tiles from row 1 and is also turned every frame.
    sl::AnimatedSprite bar = sl::create_animated_sprite(atlas, atlas.columns, frames, 450, 360, 128, 128, 0.04f);
    bar.draw_rotated = true;
    bar.pivotx = 64.0f;
    bar.pivoty = 64.0f;
```

In `exanimsprite.cpp`, a mouse click adds a one-shot explosion sprite, then `get_frame_time` drives rotation while the manager advances and draws animations.
```cpp
            else if (event.type() == sl::Event::Type::mouse_button_down)
            {
                // One-shot: plays the ring once (counts = frames - 1 advances) and then removes itself.
                sl::AnimatedSprite boom = sl::create_animated_sprite(
                    atlas, 2 * atlas.columns, frames, sl::mouse_x() - 48, sl::mouse_y() - 48, 96, 96, 0.03f, true,
                    frames - 1);
                manager.add_animated_sprite(boom);
                ++explosions;
            }
            sl::display_handle_event(event);
        }

        if (spin)
            angle += 120.0f * sl::get_frame_time() / 1000.0f;
        manager.set_sprite_rotation(bar_index, angle);
        manager.update();

        sl::clear_to_colour(sl::screen, {20, 24, 34});
        manager.draw_sprites(sl::screen);
```
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

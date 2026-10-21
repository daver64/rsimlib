# ex2dfeatures

A tour of newer 2D features: text blocks, blend modes, clip rects, sprite animation clips, hex and infinite tile maps, and integer-scaled low-resolution targets.

![ex2dfeatures screenshot](img/ex2dfeatures.png)

## What it shows
- Word-wrapped, aligned, outlined and shadowed text with `draw_text_block` and `TextStyle`
- Blend modes with `set_blend_mode` and clip rectangles
- `Animator` clips from a `SpriteSheet` built from an atlas
- Hexagonal and infinite tile maps (`create_hex_tilemap`, `create_infinite_tilemap`)
- An 80x45 render target scaled with `compute_scale_fit` and `draw_scaled_target`
- Unicode text rendering

## Key code

In `ex2dfeatures.cpp`, atlas frames become named `Animator` clips, while an infinite tile map stores cells at signed coordinates.
```cpp
    sl::SpriteSheet sheet = sl::sprite_sheet_from_atlas(sl::create_atlas(ball_sheet_bitmap, 24, 24), 0.12f);
    sheet.add_clip("bounce", 0, 3);
    sheet.add_clip("pingpong", 0, 3, true, sl::PlayDirection::ping_pong);
    sheet.add_clip("drop", 0, 2, false);
    sl::Animator animator(&sheet);
    int event_count = 0;
    animator.on_event = [&](const std::string &) { ++event_count; };
    sheet.add_event("bounce", 2, "land");
    animator.play("bounce");
    // ...
    sl::TileMap *world = sl::create_infinite_tilemap(32, 32);
    sl::tilemap_add_tileset(world, sl::create_atlas(hex_green, 32, 32));
    const int world_layer = sl::tilemap_add_layer(world, "ground");
    for (int i = -40; i < 40; ++i)
        sl::tilemap_set(world, world_layer, i, (i * 7) % 5, 1);
```

In `ex2dfeatures.cpp`, `TextStyle` configures wrapped text and the blend loop demonstrates additive, multiply and screen compositing.
```cpp
        sl::TextStyle wrapped;
        wrapped.wrap_width = 280;
        wrapped.align = sl::TextAlign::center;
        wrapped.outline = true;
        wrapped.shadow = true;
        wrapped.shadow_offset_x = 2;
        wrapped.shadow_offset_y = 2;
        sl::draw_text_block(font, 10, 10, {255, 240, 170},
                            "Word wrapping, centre alignment, an outline and a drop shadow, all from one call.", wrapped);
        // ...
        const sl::BlendMode modes[4] = {sl::BlendMode::normal, sl::BlendMode::additive, sl::BlendMode::multiply, sl::BlendMode::screen};
        const char *names[4] = {"normal", "additive", "multiply", "screen"};
        for (int i = 0; i < 4; ++i)
        {
            sl::set_blend_mode(modes[i]);
            sl::circlefill(sl::screen, bx + i * 150.0f + 50.0f, by + 70.0f, 35.0f, sl::Colour{230, 120, 60});
            sl::circlefill(sl::screen, bx + i * 150.0f + 90.0f, by + 70.0f, 35.0f, sl::Colour{80, 200, 230});
            sl::set_blend_mode(sl::BlendMode::normal);
            sl::gprintf(bx + i * 150, by, {255, 240, 170}, "%s", names[i]);
        }
```

In `ex2dfeatures.cpp`, the 80x45 scene is drawn into a render target and fitted to an integer scale for display.
```cpp
        sl::begin_render_target(lowres);
        sl::clear_render_target({40, 60, 90});
        sl::circlefill(lowres, 40.0f + std::cos(t * 2.0f) * 25.0f, 22.0f + std::sin(t * 3.0f) * 12.0f, 8.0f, sl::Colour{250, 220, 100});
        sl::rect(lowres, 0.0f, 0.0f, 79.0f, 44.0f, sl::Colour{255, 255, 255});
        sl::end_render_target();
        sl::ScaleFit fit = sl::compute_scale_fit(80, 45, 300, 200, sl::ScaleMode::integer);
        fit.x += 620;
        fit.y += 330;
        sl::rectfill(sl::screen, 620, 330, 919, 529, sl::Colour{10, 12, 18});
        sl::draw_scaled_target(lowres, fit);
```


## Running
```bash
./build/ex2dfeatures
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Set `RSIMLIB_SCREENSHOT=<file.png>` to save a screenshot on an early frame.

## Controls
| Input | Action |
|-------|--------|
| Escape | Exit |

## Key APIs
- `draw_text_block`, `TextStyle`, `TextAlign` — [Typography](../api_reference.md#typography--text-rendering-includefonth) · [`font.h`](../../include/font.h)
- `set_blend_mode`, `set_clip_rect`, `create_render_target`, `draw_scaled_target`, `compute_scale_fit` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)
- `create_hex_tilemap`, `create_infinite_tilemap`, `draw_tilemap` — [Tile Maps](../api_reference.md#tile-maps-includetilemaph) · [`tilemap.h`](../../include/tilemap.h)
- `create_atlas`, `sprite_sheet_from_atlas`, `Animator` — [Atlases](../api_reference.md#atlases--animated-sprites-includeatlash-includeanimated_spriteh) · [`atlas.h`](../../include/atlas.h)

## Source
[src/examples/ex2dfeatures/ex2dfeatures.cpp](../../src/examples/ex2dfeatures/ex2dfeatures.cpp)

## See also
- [excamera2d](excamera2d.md)
- [extilemap](extilemap.md)
- [exanimsprite](exanimsprite.md)

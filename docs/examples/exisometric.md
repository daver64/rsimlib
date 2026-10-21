# exisometric

An isometric terrain editor: raise and lower diamond tiles, pan and zoom the view, and toggle textures and soft lighting.

![exisometric screenshot](img/exisometric.png)

## What it shows
- `IsometricTransform` screen-to-tile picking and tile-to-screen projection
- Height-stepped isometric tiles drawn as diamonds or from a texture atlas
- Soft lighting through `LightingPass` and `Light`
- Mouse picking of the hovered tile with live height readout

## Key code

In `exisometric.cpp`, the `IsometricTransform` is configured with tile size, elevation and origin, and the lighting pass is initialised with an ambient level.

```cpp
    sl::IsometricTransform iso;
    iso.config.tile_width = 48.0f;
    iso.config.tile_height = 24.0f;
    iso.config.elevation_height = 27.0f;
    iso.origin_x = 480.0f;
    iso.origin_y = 120.0f;
    iso.zoom = 1.0f;

    sl::Bitmap *scene = sl::create_render_target(960, 640);
    sl::LightingPass lighting;
    const bool lighting_ready = lighting.initialise();
    if (lighting_ready)
    {
        lighting.set_ambient(0.40f);
    }
```

Mouse picking projects each column to a `tile_diamond` and tests the cursor against it, and the hovered tile is then read out in the HUD.

```cpp
        const sl::IsoTileDiamond top = iso.tile_diamond(static_cast<float>(gx), static_cast<float>(gy), static_cast<float>(height));
        const sl::IsoTileDiamond ground = iso.tile_diamond(static_cast<float>(gx), static_cast<float>(gy), 0.0f);
// ...
        pick_tile(iso, map, mx, my, hovered_x, hovered_y);
// ...
            sl::gprintf(24, 92, {255, 215, 92}, "Hovered Tile: (%d, %d) | Height: %d blocks", hovered_x, hovered_y, hc.height);
```

The soft lighting places a pulsing torch `Light` at a tile projected with `world_to_screen`, then applies it to the rendered scene.

```cpp
            iso.world_to_screen(mid + 0.5f, mid + 0.5f, static_cast<float>(torch_z) + 0.8f, torch_sx, torch_sy);

            sl::Light torch;
            torch.x = torch_sx;
            torch.y = torch_sy;
            torch.radius = 320.0f * iso.zoom;
            torch.intensity = 1.3f + std::sin(torch_pulse) * 0.06f;
            torch.colour = {255, 180, 80};
            lights.push_back(torch);

            sl::clear_to_colour(sl::screen, {18, 22, 30});
            lighting.apply(scene, lights, 0, 0, 960, 640);
```

## Running
```bash
./build/exisometric
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Left click | Raise the hovered tile |
| Right click | Lower the hovered tile |
| Middle drag, or W A S D / arrow keys | Pan |
| + / - | Zoom |
| T | Toggle texture atlas |
| L | Toggle soft lighting |
| R | Reset the map |
| Escape | Exit |

## Key APIs
- `IsometricTransform`, `IsoTileDiamond` — [Isometric](../api_reference.md#isometric-math--projection-includeisometrich) · [`isometric.h`](../../include/isometric.h)
- `LightingPass`, `Light` — [2D Radial](../api_reference.md#2d-radial-lighting--polygon-shadows-includegraphics_fxh) · [`graphics_fx.h`](../../include/graphics_fx.h)
- `create_atlas`, `atlas_stretch_blit` — [Atlases](../api_reference.md#atlases--animated-sprites-includeatlash-includeanimated_spriteh) · [`atlas.h`](../../include/atlas.h)
- `rectfill`, `trianglefill`, `line` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)

## Source
[src/examples/exisometric/exisometric.cpp](../../src/examples/exisometric/exisometric.cpp)

## See also
- [extilemap](extilemap.md)
- [exlighting](exlighting.md)
- [expathfinding](expathfinding.md)

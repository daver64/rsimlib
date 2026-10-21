# exisometric

An isometric terrain editor: raise and lower diamond tiles, pan and zoom the view, and toggle textures and soft lighting.

![exisometric screenshot](img/exisometric.png)

## What it shows
- `IsometricTransform` screen-to-tile picking and tile-to-screen projection
- Height-stepped isometric tiles drawn as diamonds or from a texture atlas
- Soft lighting through `LightingPass` and `Light`
- Mouse picking of the hovered tile with live height readout

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

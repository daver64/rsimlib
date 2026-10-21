# expathfinding

Layered pathfinding: enemies pick between chase, scent and sound maps to hunt a player through a tile grid with editable walls.

![expathfinding screenshot](img/expathfinding.png)

## What it shows
- Chase maps: step distance from the player that enemies walk downhill
- Scent: the player’s fading trail, used when no chase route exists
- Sound: pulses spreading through open tiles while the player moves noisily
- Switching the visualised overlay
- Editing walls at runtime and recomputing

## Running
```bash
./build/expathfinding
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| W A S D / arrow keys | Move the player |
| Shift | Move noisily (emit sound) |
| Mouse click | Toggle a wall |
| Space | Cycle the overlay |
| Escape | Exit |

## Key APIs
- `pathfinding.h` tile-grid maps (chase, scent, sound layers) — [Tile-Grid](../api_reference.md#tile-grid-pathfinding-includepathfindingh) · [`pathfinding.h`](../../include/pathfinding.h)
- `key_down`, `mouse_x`, `mouse_y` — [Events](../api_reference.md#events-input--timing-includeeventh-includeinputh-includesystemh-includegamepadh) · [`event.h`](../../include/event.h)
- `rectfill`, `circlefill`, `line`, `textout` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)

## Source
[src/examples/expathfinding/expathfinding.cpp](../../src/examples/expathfinding/expathfinding.cpp)

## See also
- [extilemap](extilemap.md)
- [execs](execs.md)
- [exphysics](exphysics.md)

# exroguelike

A coloured ASCII roguelike: procedural overworld and dungeons, line-of-sight FOV, lights and combat, built on simlib, `recs` and `rworld`.

![exroguelike screenshot](img/exroguelike.png)

## What it shows
- Font-atlas ASCII cell rendering
- Procedural overworld (`rworld`) and dungeon generation
- Bresenham field of view and light sources
- Archetype ECS (`recs`) actors and turn-based combat
- HUD with environment panel

## Running
```bash
./build/exroguelike
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| H J K L, arrows, numpad 1-9 | Move / attack |
| . or numpad 5 | Wait a turn |
| > or Enter | Use stairs |
| < | Ascend stairs |
| Escape or Q | Quit |

## Key APIs
- `recs` ECS
- Fonts — [Typography](../api_reference.md#typography--text-rendering-includefonth) · [`font.h`](../../include/font.h)
- `get_default_monospace_font` — [Typography](../api_reference.md#typography--text-rendering-includefonth) · [`font.h`](../../include/font.h)

## Source
[src/examples/exroguelike/exroguelike.cpp](../../src/examples/exroguelike/exroguelike.cpp)

## See also
- [execs](execs.md)
- [expathfinding](expathfinding.md)

Deep dive: [Roguelike guide](../exroguelike_guide.md).

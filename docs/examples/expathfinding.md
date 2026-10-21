# expathfinding

Layered pathfinding: enemies pick between chase, scent and sound maps to hunt a player through a tile grid with editable walls.

![expathfinding screenshot](img/expathfinding.png)

## What it shows
- Chase maps: step distance from the player that enemies walk downhill
- Scent: the player’s fading trail, used when no chase route exists
- Sound: pulses spreading through open tiles while the player moves noisily
- Switching the visualised overlay
- Editing walls at runtime and recomputing

## Key code

In `expathfinding.cpp`, the `Pathfinder` is sized to the tile grid, and each frame it is updated from the player's position, with the last argument feeding the sound layer when the player moves noisily.

```cpp
    Pathfinder pathfinder;
    pathfinder.resize(columns, rows, cell, cell, blocked);
// ...
        pathfinder.update(player_col, player_row, moved && noisy);
```

Enemies never search: they read the next step from the chase layer first, then scent, then sound.

```cpp
            const PathLayer layers[] = {PathLayer::chase, PathLayer::scent, PathLayer::sound};
            const char *names[] = {"chase", "scent", "sound"};
            enemy.mode = "idle";
            enemy.dx = enemy.dy = 0;
            for (int i = 0; i < 3; ++i)
            {
                if (pathfinder.next_step(layers[i], enemy.col, enemy.row, step))
                {
                    enemy.col += step.dx;
                    enemy.row += step.dy;
                    enemy.dx = step.dx;
                    enemy.dy = step.dy;
                    enemy.mode = names[i];
                    break;
                }
            }
```

Clicking toggles a wall at runtime, and Space cycles the visualised overlay.

```cpp
            else if (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::space)
            {
                overlay = (overlay + 1) % 4;
            }
            else if (event.type() == sl::Event::Type::mouse_button_down)
            {
                const int col = static_cast<int>((sl::mouse_x() - origin_x) / cell);
                const int row = static_cast<int>((sl::mouse_y() - origin_y) / cell);
                if (col >= 0 && row >= 0 && col < columns && row < rows && !(col == player_col && row == player_row))
                    walls[row * columns + col] ^= 1;
            }
```

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

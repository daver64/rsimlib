# exroguelike

A coloured ASCII roguelike: procedural overworld and dungeons, line-of-sight FOV, lights and combat, built on simlib, `recs` and `rworld`.

![exroguelike screenshot](img/exroguelike.png)

## What it shows
- Font-atlas ASCII cell rendering
- Procedural overworld (`rworld`) and dungeon generation
- Bresenham field of view and light sources
- Archetype ECS (`recs`) actors and turn-based combat
- HUD with environment panel

## Key code

In `render.cpp`, the ASCII map is drawn one cell at a time: a background rectangle is filled and the glyph is drawn with `textout` from the monospace font.

```cpp
if (background.red != 0 || background.green != 0 || background.blue != 0)
{
    sl::rectfill(target, static_cast<float>(pixel_x), static_cast<float>(pixel_y),
                 static_cast<float>(pixel_x + CELL_WIDTH - 1), static_cast<float>(pixel_y + CELL_HEIGHT - 1), background);
}

if (character != ' ' && character != '\0')
{
    char text[2] = {character, '\0'};
    sl::textout(g_map_font, pixel_x + 2, pixel_y, foreground, text);
}
```

In `world_gen.cpp`, the overworld is generated from a seeded `rworld::World`, which is queried for the biome of each map cell.

```cpp
rworld::WorldConfig config;
config.seed = static_cast<uint64_t>(random_int(1, 1000000));
config.world_scale = 0.5f;
// ...
game.rworld_generator = std::make_shared<rworld::World>(config);
rworld::World &world = *game.rworld_generator;
// ...
const rworld::BiomeType biome = world.get_biome(lon, lat, std::max(0.0f, height));
```

In `game_logic.cpp`, the field of view tests every tile within the radius using a Bresenham line-of-sight walk that stops at tiles blocking sight.

```cpp
if (line_of_sight(game, origin.x, origin.y, x, y))
{
    tile->visible = true;
    tile->explored = true;
}
```

In `exroguelike.cpp`, the map is rendered to a target and lit by a `LightingPass` using the game's lights and shadow casters.

```cpp
auto lights = build_simlib_lights(game);
auto casters = build_simlib_shadow_casters(game);
lighting_pass.apply(scene, lights, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, casters);

render_ui(sl::screen, game);
```

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

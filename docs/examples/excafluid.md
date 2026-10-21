# excafluid

A falling-sand style sandbox: a multi-material cellular-automata fluid engine coupled to Box2D bodies, with lighting and bloom.

![excafluid screenshot](img/excafluid.png)

## What it shows
- 14 materials with heat conduction, phase changes and buoyancy
- Spray materials with the mouse
- Box2D rigid bodies (wood, metal, tank boxes) floating and sinking in the fluid
- Preset scenarios
- Optional lighting and bloom

## Running
```bash
./build/excafluid
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Mouse (hold) | Spray the selected material |
| 1-9, 0, - | Choose Sand, Water, Oil, Fire, Wood, Solid, Acid, Lava, Gunpowder, Plant, Pump |
| [ / ] | Brush radius |
| Space | Pause |
| R | Clear |
| P | Next scenario |
| C / V / T | Spawn a box at the cursor |
| B | Toggle bloom |
| L | Toggle lighting |
| Escape | Exit |

## Key APIs
- Fluid grid and Box2D coupling — [Cellular Automata](../api_reference.md#cellular-automata-fluid--granular-simulation-includefluidh) · [`fluid.h`](../../include/fluid.h)
- `Bloom`, lighting — [Post-Processing](../api_reference.md#post-processing-graphics-effects-includegraphics_fxh) · [`graphics_fx.h`](../../include/graphics_fx.h)

## Source
[src/examples/excafluid/excafluid.cpp](../../src/examples/excafluid/excafluid.cpp)

## See also
- [exfluid3d](exfluid3d.md)
- [exphysics](exphysics.md)
- [exlighting](exlighting.md)

Deep dive: [Cellular Automata Fluid guide](../excafluid_guide.md).

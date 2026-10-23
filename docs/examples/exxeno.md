# exxeno

A lit top-down shooter with synthesised sound and music. A space marine (WASD to move, mouse to aim, left button to fire) holds off xenomorphs in a tile-map arena. Everything is drawn from geometry; each kill leaves an acid splat on the floor.

![exxeno screenshot](img/exxeno.png)

## What it shows
- Tile-map decals with `tilemap_add_decal()`: splats stamped at free positions, rotated and scaled, drawn above the floor layer and below the walls
- A generated tileset: the splat sheet is drawn into a render target and added with `tilemap_add_tileset()`
- 2D lighting with `LightingPass`: a shadow-casting flashlight spot, a muzzle flash and a light on every bolt, with rectangle shadow casters for the walls
- Particle bursts from pools of `ParticleEmitter`s (acid, sparks, muzzle), drawn additively over the lit scene
- Synthesised audio with `generate_sample()` and `generate_audio()`: space-age plasma, impact, alien-shriek, hurt and power-down effects panned by screen position, plus a driving 156 BPM D-minor loop (kick, rolling saw bass, 16th arpeggio, backbeat, a rising siren in the last bar) mixed into one buffer and looped with `play_sample(..., loops = -1)`. No audio files are used
- Tile collision with `tilemap_rect_blocked()` for the marine, aliens and bolts

## Key code

The splat is a tile of a second tileset, stamped where the alien died:

```cpp
const std::uint32_t variant = static_cast<std::uint32_t>(random_between(0.0f, 4.0f)) % 4;
sl::tilemap_add_decal(map, 0, static_cast<std::uint32_t>(splat_first_gid) + variant, alien.x,
                      alien.y, random_between(0.0f, 360.0f), random_between(0.8f, 1.5f));
```

`after_layer` 0 puts the splat directly above the floor layer, so walls still draw over it. `map->max_decals` caps how many are kept; the oldest is discarded first.

The scene is rendered to a target, lit, and then the glowing effects are added on top:

```cpp
sl::draw_tilemap(map, 0.0f, 0.0f, screen_w, screen_h);
// ... aliens and marine ...
lighting.apply(scene, lights, 0, 0, screen_w, screen_h, casters);
sl::set_blend_mode(sl::BlendMode::additive);
```

## Controls
WASD move, mouse aim, left button fire, R restart after being overrun, Esc quit.

## Run
`./build/exxeno` from the repository root.

# Terrain (chunked LOD)

`terrain.h` provides a quadtree chunked-LOD terrain for flat arenas and for close-in planet terrain. Each chunk is a
`(chunk_cells + 1)^2` grid with skirts that hide cracks between levels. Chunks are built on background threads and
uploaded to the GPU on the render thread, a few per frame.

```cpp
sl::NoiseParameters noise;
noise.amplitude = 500.0;
sl::TerrainConfig config;
config.size = 16384.0;
config.origin = {-8192.0, 0.0, -8192.0};
sl::Terrain terrain(config, std::make_shared<sl::NoiseSource>(noise));

// Every frame, between begin_surface_3d and end_surface_3d:
terrain.update(sl::make_terrain_view(camera, camera_world_position, width, height));
terrain.draw(camera_world_position);   // drawn relative to the eye, so floats stay precise
```

Render with the camera at the origin and pass the eye position to `draw()` (a floating origin). This keeps vertex
precision at any distance from the world origin.

## Shapes

- `TerrainShape::flat`: a square of `size` metres with its minimum corner at `origin`; heights are added to `origin.y`.
- `TerrainShape::sphere`: six cube faces mapped onto a sphere of `radius` centred at `origin`. Heights are radial.
  The terrain does not spin; a spinning planet's terrain follows only the body's position.

## Height sources

| Source | Notes |
| --- | --- |
| `FunctionSource` | Callback `(x, z)` and an optional `(direction, radius)` callback for spheres. Must be thread-safe. |
| `NoiseSource` | Fractal OpenSimplex noise (`NoiseParameters`: octaves, ridged, floor). Sphere terrain uses 3D noise, so faces have no seams. Sphere sampling uses float noise coordinates, which limits detail of very fine octaves. |
| `HeightmapSource` | From a `Bitmap`; `pack_16_bit` reads 16-bit heights from the R and G channels, as bitmaps are 8-bit. Wraps in longitude on spheres. |
| `GridSource` | An editable heightmap: `set_sample`, `raise`, `flatten`, `smooth`. Edits rebuild only the touched chunks on flat terrain (a sphere rebuilds all). |

## Level of detail

A chunk is split while its screen-space error (`cell_size * error_scale`, projected) exceeds `max_screen_error` pixels,
with hysteresis on merge. The deepest level comes from `finest_cell_size` unless `max_level` is set. A chunk is only
replaced by its children once all four are built, so there are no holes while streaming. Chunks outside the view
frustum, beyond `view.far_distance`, or below the planet's horizon are skipped.

## Queries

These use the height source directly, so they are exact and independent of LOD and of what is loaded:
`height_at`, `normal_at`, `height_at_direction`, `normal_at_direction`, `surface_point`, `altitude_of`,
`normal_below` and `raycast`.

## Flight integration

```cpp
world.flat_ground = true;
world.flat_terrain = &terrain;     // gear contact, altitude and air density follow the terrain
planet_body->terrain = &planet;    // the same for a spherical body (the terrain's origin is ignored)
```

## Water

Terrain chunks are cached on the CPU, so waves cannot be baked into them. Use `sl::Ocean` instead: set `set_terrain(&terrain)` and a sea level, draw the terrain first, then `ocean.update(...)` and `ocean.draw()`. The terrain shows through the water by depth testing, and the ocean shoals its waves and adds foam where the depth goes to zero. See the [API reference](api_reference.md#ocean-includeoceanh) and `exterrain`.

## Appearance

Layers (`TerrainLayer`, default sand/grass/rock/snow, at most `terrain_max_layers` = 8) are weighted by height and
slope, optionally overridden by `weight_function` (`float weights[8]`), and blended into the vertex colour. Texture
coordinates are in metres.

`TerrainLayer::specular` (0–1, default 0) makes a layer shiny, for example water. It only takes effect when the
`Material` passed to `Terrain::draw` has `pbr = true` (which also supplies `roughness`); the highlight is a
dielectric GGX term for the sun and local lights. It works for both untextured and textured terrain.

### Layer textures

`Terrain::set_layer_textures(options)` textures the terrain from `config().layers` using GPU texture arrays. Each
layer's `albedo` (and optional `normal_map`) wraps, is mipmapped and is anisotropically filtered, and repeats at
its own `tile_metres` (the FlightGear `xsize`), so nothing needs baking into an atlas. Bitmaps can be any size: they
are resampled to one power-of-two layer size (the largest input, up to `options.max_size`, default 2048). The call
rebuilds the chunks, so make it at setup after `set_gfx_mode()`; `clear_layer_textures()` returns to vertex colours.
It returns false when the backend has no texture arrays (Direct3D 12 draws vertex colours) or no layer has an albedo.

- **Weights:** all eight surface types are carried in two vertex attributes (the vertex colour and `Vertex3D::extra`),
  so every type is always present: no swapping, hysteresis or rebuilds.
- **Variants:** `TerrainLayer::variants` lists extra albedo/normal pairs of a surface. The shader picks one per tile by
  hashed noise. Variants are extra array layers, so the total is limited only by the backend
  (`GL_MAX_ARRAY_TEXTURE_LAYERS` or `maxImageArrayLayers`).
- **Distance fade:** beyond `fade_start` metres each layer blends to its average colour (complete at `fade_end`), and
  normal-map bumps fade out with it.
- **Macro texture:** `macro_texture` (tile `macro_tile_metres`, weight `macro_strength`) modulates brightness to break
  up repetition at altitude.
- **Seamless UVs:** UVs wrap every `uv_period_metres` (the largest tile or the macro tile); each tile is snapped to a
  whole number per period so chunks and variants line up exactly.

Textures are copied to the GPU once, so later edits to the bitmaps are not seen.

### Normal maps

A layer's `normal_map` is a tangent-space normal map (green up, OpenGL style). Only the red and green channels are
used and the blue is rebuilt, so the shorter normals that mip averaging produces flatten the bumps naturally with
distance. The tangent frame comes from screen-space derivatives of the position and texture coordinates, so terrain
needs no tangent data (the tangent vertex slot carries the geomorph offset). Layers without a normal map are flat.

### Geomorphing

With `TerrainConfig::geomorph` (default on) every vertex stores the offset to its parent chunk's surface, and the
vertex shader blends it in as the chunk approaches the distance at which its parent would split, so level changes
happen without visible pops. It costs one extra grid of height samples per chunk build. Normals are not morphed.

Terrain meshes use the vertex tangent slot for the morph offset, so `Terrain::draw` always applies terrain shading;
do not draw terrain chunk meshes with an ordinary material.

## Lua

```lua
terrain.create("t", {
  shape = "flat", size = 8192, finest_cell_size = 1, geomorph = true,
  source = {type = "noise", seed = 3, amplitude = 300, ridged = true},
})
-- Sources: noise (seed, frequency, octaves, lacunarity, gain, amplitude, base_height, ridged, floor_height),
--          flat (height), grid (width, depth, world_width, world_depth, height), heightmap (sprite, min_height,
--          max_height, world_width, world_depth, pack_16_bit). Also: shape "sphere" with radius; chunk_cells,
--          max_level, max_screen_error, origin.
--          layers = {{sprite, normal, tile_metres, colour = {r,g,b}, variants = {"sprite", {sprite=, normal=}},
--                     min_height, max_height, height_blend, min_slope, max_slope, slope_blend, strength}, ...}
--          textures = {fade_start, fade_end, macro_sprite, macro_tile_metres, macro_strength, max_size}
terrain.view("t", 0, 0, 800, 600, {x, y, z}, {tx, ty, tz}, {fov = 60, far = 20000, up = {0, 1, 0}, sky = {150, 185, 230}})
local h   = terrain.height_at("t", x, z)               -- nil for an unknown terrain
local n   = terrain.normal_at("t", x, z)               -- {x, y, z}
local alt = terrain.altitude_of("t", x, y, z)
local hit = terrain.raycast("t", ox, oy, oz, dx, dy, dz, 5000)  -- {x, y, z, normal = {...}, distance} or nil
terrain.raise("t", x, z, radius, amount)               -- grid sources only: raise, flatten, smooth, set_sample
terrain.stats("t")                                     -- visible_chunks, visible_triangles, resident_chunks, ...
terrain.destroy("t")
```

`terrain.view` is retained like `voxel.view`: the terrain streams and draws every frame the canvas renders. The eye is
a world position in double precision. Lua states are not thread-safe, so Lua exposes data-defined sources only, not
height callbacks.

Sky, fog, clouds and scattered objects: see [World Flavour](world_flavour.md) and the Lua notes in [scripting_lua.md](scripting_lua.md#terrain-terrain).

Example: `exterrain` (add `planet` to the command line for planet mode).

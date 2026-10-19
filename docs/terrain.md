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

## Appearance

Layers (`TerrainLayer`, default sand/grass/rock/snow) are weighted by height and slope, optionally overridden by
`weight_function`, and blended into the vertex colour. Set `weights_in_vertex_colour` to keep the four weights in the
vertex colour instead. Texture coordinates are in tiles of `uv_tile_metres`.

### Splat atlas

`Terrain::set_layer_atlas(bitmap, fade_start, fade_end)` textures the terrain with four tiling layers laid out as a
2x2 atlas (layer 0 top left, 1 top right, 2 bottom left, 3 bottom right). The four layer weights travel in the
vertex colour and the shader blends the layers per pixel, so it works the same on OpenGL, Vulkan and D3D12. Layers
repeat every `uv_tile_metres`; beyond `fade_start` metres they cross-fade to a larger-scale copy (complete at
`fade_end`) to hide repetition. Set `bitmap->smooth = true` for bilinear filtering. Pass `nullptr` to return to vertex
colours. The call rebuilds the chunks, so make it at setup.

Limitations: textures have no mipmaps, so distant layers can shimmer (the fade only partly hides it), and layer normal
maps are not used.

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
--          max_level, max_screen_error, uv_tile_metres, origin.
terrain.set_atlas("t", "atlas_sprite", 150, 600)      -- 2x2 layer atlas from a loaded sprite
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

Example: `exterrain` (add `planet` to the command line for planet mode).

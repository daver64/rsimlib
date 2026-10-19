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

Not yet implemented: texture splatting shaders (texture samplers are still clamp-to-edge), geomorphing, and Lua
bindings.

Example: `exterrain` (add `planet` to the command line for planet mode).

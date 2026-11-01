# Tile Maps & 3D Quick Start

[← Back to README](../README.md)

## Tile Maps & Tiled Import

Load a map authored in the [Tiled](https://www.mapeditor.org/) editor (JSON `.tmj`/`.json`) and draw it with one call. Layers, tile flips, animated tiles, per-tile properties (a boolean `solid` property drives collision), object layers, and both orthogonal and isometric maps are supported. Drawing is culled and batched, and tiles take part in 2D lighting and normal mapping like any other sprite.

```cpp
sl::TileMap *map = sl::load_tiled_map("assets/level1.tmj");
if (!map)
    return -1; // sl::last_error() explains why

const sl::TileObject *spawn = sl::tilemap_find_object(map, "player_spawn");

// each frame
sl::update_tilemap(map, dt);                        // advances tile animations
bool blocked = sl::tilemap_rect_blocked(map, x, y, 16, 16); // collision against solid tiles
sl::draw_tilemap(map, camera_x, camera_y);          // draws only the visible cells

sl::destroy_tilemap(map);
```

Maps can also be built in code with `create_tilemap()` and `tilemap_add_layer()`. Tiled maps (`.tmj`/`.json` and native `.tmx`) load with CSV or base64 layer data (uncompressed, zlib or gzip), infinite maps, external `.tsj`/`.tsx` tilesets, image-collection tilesets, layer opacity and tint colours, group layers (children inherit the group's visibility, opacity, offset and tint), image layers and tile objects. `tilemap_add_decal()` stamps runtime splats, scorch marks and footprints at free positions. Not supported: zstd-compressed layers (choose zlib in Tiled), and infinite isometric maps. See [`extilemap`](../src/examples/extilemap/extilemap.cpp) (pass your own `.tmj` or `.tmx` path to view it) and the [Tile Maps API reference](api_reference.md#tile-maps-includetilemaph).

## 3D Graphics: Surfaces, Models & Voxel Worlds

`simlib` includes both a high-level 3D drawing API and lower-level ways to build renderer-specific scenes. The backend-neutral `sl::Surface3D` API ([include/surface3d.h](../include/surface3d.h)) is implemented for OpenGL, Vulkan and Direct3D 12. Create an offscreen colour-and-depth surface with optional multisample anti-aliasing (MSAA; supported on all three backends), draw with a 3D `Camera` and directional `Light3D`, render built-in primitive meshes or loaded Wavefront `.obj`/`.mtl`, `.gltf`, and `.glb` models, optionally shade them with tangent-space normal maps (`Material::normal_map`; tangents are generated automatically, and glTF `normalTexture` and MTL `norm`/`map_Bump` are loaded), add 2D overlays such as HUD text and sprites, and composite the result into a 2D frame.

```cpp
// Create an offscreen 3D surface with 4x MSAA
sl::Surface3D *surface = sl::create_surface_3d(640, 480, 4);
sl::Model *crate = sl::load_model("assets/models/crate.obj");
sl::Camera camera;
camera.position = {2.5f, 2.2f, 4.0f};
camera.target = {0.0f, 0.3f, 0.0f};

// Draw 3D scene and 2D overlays into the surface
if (sl::begin_surface_3d(surface, camera))
{
    sl::clear_surface_3d({20, 30, 60});
    sl::draw_model(crate, sl::model_matrix({0, 0, 0}, {0, rotation, 0}));
    sl::gprintf(16, 16, {255, 255, 255}, "MSAA %dx Surface3D", sl::surface_3d_samples(surface));
    sl::end_surface_3d();
}

// Composite the 3D surface bitmap into your 2D scene
sl::draw_sprite(sl::surface_3d_bitmap(surface), 80, 60);
```

For block-based worlds, the voxel API provides chunk rendering, hidden-face culling, texture-atlas mapping, and 3D raycasting; see [Voxel Chunks & World](api_reference.md#voxel-chunks--world-includevoxelh). For renderer-specific control, the [3D examples](renderers.md#direct-3d-escape-hatches) demonstrate direct OpenGL, Vulkan, and Direct3D 12 rendering. Normal-mapped 2D sprites/tiles under `LightingPass` ([`exnormalmap`](../src/examples/exnormalmap/exnormalmap.cpp)) and normal-mapped 3D meshes ([`exnormalmap3d`](../src/examples/exnormalmap3d/exnormalmap3d.cpp)) work on all three backends; see [Graphics Effects](graphics_effects.md).

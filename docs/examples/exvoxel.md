# exvoxel

A Minecraft-style voxel world: fly around, break and place blocks, with chunk meshing.

![exvoxel screenshot](img/exvoxel.png)

## What it shows
- Chunked voxel meshing
- Block raycast targeting
- Breaking and placing blocks
- Mouse-look with a locked cursor
- Chunk and triangle counters

## Key code

In `exvoxel.cpp`, a `VoxelWorld` is generated and meshed into chunks against a texture atlas by `update_dirty_chunks`.

```cpp
sl::VoxelAtlas atlas;
atlas.texture = atlas_bitmap;
atlas.tiles_per_row = 16;
atlas.tiles_per_col = 16;

sl::VoxelWorld world;
generate_asteroid(world);
world.update_dirty_chunks(&atlas);
```

The cursor is locked and the mouse delta drives the look direction.

```cpp
sl::set_relative_mouse_mode(true);
// ...
int dx = 0, dy = 0;
sl::mouse_delta(dx, dy);
constexpr float sensitivity = 0.003f;
yaw += static_cast<float>(dx) * sensitivity;
pitch -= static_cast<float>(dy) * sensitivity;
```

Each frame a block raycast finds the targeted block; left click breaks it and right click places a block, with dirty chunks re-meshed after each edit.

```cpp
const sl::VoxelRaycastResult ray_hit = world.raycast(cam_pos, forward, 7.5f);
// ...
if (mouse_captured && left_down && !prev_left_down && ray_hit.hit)
{
    // ...
    if (destroyed_block != sl::BLOCK_AIR &&
        world.set_block(ray_hit.hit_block.x, ray_hit.hit_block.y, ray_hit.hit_block.z, sl::BLOCK_AIR))
    {
        world.update_dirty_chunks(&atlas);
        // ...
    }
}
// ...
if (world.set_block(ray_hit.place_block.x, ray_hit.place_block.y, ray_hit.place_block.z, new_block))
{
    world.update_dirty_chunks(&atlas);
    // ...
}
```

The chunk and triangle counters are shown as live text.

```cpp
sl::gprintf(20, 36, {160, 210, 255}, "Chunks: %zu | Triangles: %d",
            world.chunk_count(), world.total_triangles());
```

## Running
```bash
./build/exvoxel
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Click the window / Tab | Lock the mouse and look |
| W A S D | Fly |
| Space / C | Up / down |
| Shift | Fast |
| Left click | Break block |
| Right click | Place block |
| 1-9, 0 | Choose block |
| Tab / Escape | Free the cursor |
| Escape (cursor free) | Exit |

## Key APIs
- Voxel world — [Voxel Chunks](../api_reference.md#voxel-chunks--world-includevoxelh) · [`voxel.h`](../../include/voxel.h)
- `Surface3D` — [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh) · [`surface3d.h`](../../include/surface3d.h)

## Source
[src/examples/exvoxel/exvoxel.cpp](../../src/examples/exvoxel/exvoxel.cpp)

## See also
- [exterrain](exterrain.md)
- [exinstancing](exinstancing.md)

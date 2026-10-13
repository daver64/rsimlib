#include "sl.h"

#include <cassert>
#include <cmath>
#include <iostream>

void test_block_definitions()
{
    std::cout << "Testing block definitions and registry..." << std::endl;

    const sl::BlockDefinition &air = sl::get_block_definition(sl::BLOCK_AIR);
    assert(!air.is_solid);
    assert(air.is_transparent);

    // Default unregistered solid block
    const sl::BlockDefinition &unregistered = sl::get_block_definition(99);
    assert(unregistered.is_solid);
    assert(!unregistered.is_transparent);

    // Custom block registration
    sl::BlockDefinition custom;
    custom.is_solid = true;
    custom.is_transparent = true; // e.g. glass
    custom.tint = {100, 150, 200, 255};
    custom.tiles[static_cast<int>(sl::VoxelFace::top)] = {2, 3};
    sl::register_block_definition(10, custom);

    const sl::BlockDefinition &retrieved = sl::get_block_definition(10);
    assert(retrieved.is_solid);
    assert(retrieved.is_transparent);
    assert(retrieved.tint.red == 100);
    assert(retrieved.tiles[static_cast<int>(sl::VoxelFace::top)].x == 2);
    assert(retrieved.tiles[static_cast<int>(sl::VoxelFace::top)].y == 3);
}

void test_voxel_atlas()
{
    std::cout << "Testing voxel atlas UV calculation..." << std::endl;

    sl::VoxelAtlas atlas;
    atlas.tiles_per_row = 16;
    atlas.tiles_per_col = 16;

    float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
    atlas.get_tile_uv(0, 0, u0, v0, u1, v1);
    assert(std::abs(u0 - 0.0f) < 0.0001f);
    assert(std::abs(v0 - 0.0f) < 0.0001f);
    assert(std::abs(u1 - 0.0625f) < 0.0001f);
    assert(std::abs(v1 - 0.0625f) < 0.0001f);

    atlas.get_tile_uv(4, 2, u0, v0, u1, v1);
    assert(std::abs(u0 - 0.25f) < 0.0001f);
    assert(std::abs(v0 - 0.125f) < 0.0001f);
    assert(std::abs(u1 - 0.3125f) < 0.0001f);
    assert(std::abs(v1 - 0.1875f) < 0.0001f);
}

void test_chunk_storage_and_lifecycle()
{
    std::cout << "Testing chunk storage, bounds checking, and lifecycle..." << std::endl;

    sl::VoxelChunk chunk(2, 3, 4);
    assert(chunk.chunk_x() == 2);
    assert(chunk.chunk_y() == 3);
    assert(chunk.chunk_z() == 4);
    assert(chunk.is_empty());

    // Out of bounds queries
    assert(chunk.get_block(-1, 0, 0) == sl::BLOCK_AIR);
    assert(chunk.get_block(16, 0, 0) == sl::BLOCK_AIR);
    assert(!chunk.set_block(16, 0, 0, 1));

    // Valid placement
    assert(chunk.set_block(5, 5, 5, 1));
    assert(chunk.get_block(5, 5, 5) == 1);
    assert(!chunk.is_empty());

    // Setting same block returns false (no change)
    assert(!chunk.set_block(5, 5, 5, 1));

    // Resetting to air
    assert(chunk.set_block(5, 5, 5, sl::BLOCK_AIR));
    assert(chunk.is_empty());

    // Fill
    chunk.fill(2);
    assert(!chunk.is_empty());
    assert(chunk.get_block(0, 0, 0) == 2);
    assert(chunk.get_block(15, 15, 15) == 2);

    chunk.fill(sl::BLOCK_AIR);
    assert(chunk.is_empty());

    // Move semantics
    chunk.set_block(1, 1, 1, 5);
    sl::VoxelChunk moved(std::move(chunk));
    assert(moved.chunk_x() == 2);
    assert(moved.get_block(1, 1, 1) == 5);
    assert(!moved.is_empty());
}

void test_face_culling()
{
    std::cout << "Testing face culling geometry counts..." << std::endl;

    // 1. Single isolated block has 6 visible faces
    {
        sl::VoxelChunk chunk;
        chunk.set_block(8, 8, 8, 1);
        assert(chunk.count_visible_faces() == 6);
    }

    // 2. Two adjacent solid blocks share 1 face; total visible = 6 + 6 - 2 = 10
    {
        sl::VoxelChunk chunk;
        chunk.set_block(8, 8, 8, 1);
        chunk.set_block(9, 8, 8, 1);
        assert(chunk.count_visible_faces() == 10);
    }

    // 3. A 2x2x2 solid cube (8 voxels): 6 sides * 4 quads = 24 visible faces
    {
        sl::VoxelChunk chunk;
        for (int z = 4; z < 6; ++z)
            for (int y = 4; y < 6; ++y)
                for (int x = 4; x < 6; ++x)
                    chunk.set_block(x, y, z, 1);
        assert(chunk.count_visible_faces() == 24);
    }

    // 4. A completely full 16x16x16 chunk (4096 voxels):
    // Only the outer boundary voxels have visible faces.
    // 6 faces * 16 * 16 = 1536 visible faces.
    {
        sl::VoxelChunk chunk;
        chunk.fill(1);
        assert(chunk.count_visible_faces() == 1536);
    }

    // 5. Boundary lookup culling across chunks:
    // If chunk has block at (15, 0, 0) and neighbor lookup returns solid at (16, 0, 0),
    // the +X face is culled (5 faces instead of 6).
    {
        sl::VoxelChunk chunk(0, 0, 0);
        chunk.set_block(15, 0, 0, 1);
        assert(chunk.count_visible_faces() == 6); // default without lookup treats border as air

        auto lookup = [](int wx, int wy, int wz) -> sl::BlockId
        {
            if (wx == 16 && wy == 0 && wz == 0)
                return 1;
            return sl::BLOCK_AIR;
        };

        assert(chunk.count_visible_faces(lookup) == 5);
    }
}

void test_world_coordinates()
{
    std::cout << "Testing world-to-chunk and world-to-local coordinate mapping..." << std::endl;

    // Origin
    assert(sl::VoxelWorld::world_to_chunk(0, 0, 0) == glm::ivec3(0, 0, 0));
    assert(sl::VoxelWorld::world_to_local(0, 0, 0) == glm::ivec3(0, 0, 0));

    // Top-right of chunk 0
    assert(sl::VoxelWorld::world_to_chunk(15, 15, 15) == glm::ivec3(0, 0, 0));
    assert(sl::VoxelWorld::world_to_local(15, 15, 15) == glm::ivec3(15, 15, 15));

    // Next chunk in X
    assert(sl::VoxelWorld::world_to_chunk(16, 0, 0) == glm::ivec3(1, 0, 0));
    assert(sl::VoxelWorld::world_to_local(16, 0, 0) == glm::ivec3(0, 0, 0));

    // Negative coordinates
    assert(sl::VoxelWorld::world_to_chunk(-1, -1, -1) == glm::ivec3(-1, -1, -1));
    assert(sl::VoxelWorld::world_to_local(-1, -1, -1) == glm::ivec3(15, 15, 15));

    assert(sl::VoxelWorld::world_to_chunk(-16, -16, -16) == glm::ivec3(-1, -1, -1));
    assert(sl::VoxelWorld::world_to_local(-16, -16, -16) == glm::ivec3(0, 0, 0));

    assert(sl::VoxelWorld::world_to_chunk(-17, 0, 0) == glm::ivec3(-2, 0, 0));
    assert(sl::VoxelWorld::world_to_local(-17, 0, 0) == glm::ivec3(15, 0, 0));
}

void test_world_management_and_raycast()
{
    std::cout << "Testing VoxelWorld block setting, dirty updates, and 3D DDA raycasting..." << std::endl;

    sl::VoxelWorld world;

    // Initially empty
    assert(world.get_block(0, 0, 0) == sl::BLOCK_AIR);

    // Place ground block at (0, 0, 0)
    world.set_block(0, 0, 0, 1);
    assert(world.get_block(0, 0, 0) == 1);
    assert(world.chunk_count() == 1);

    // Place block across boundary at (16, 0, 0)
    world.set_block(16, 0, 0, 2);
    assert(world.get_block(16, 0, 0) == 2);
    assert(world.chunk_count() == 2);

    // Raycast: straight down onto the block at (0, 0, 0)
    {
        glm::vec3 ray_origin(0.5f, 5.0f, 0.5f);
        glm::vec3 ray_dir(0.0f, -1.0f, 0.0f);
        sl::VoxelRaycastResult res = world.raycast(ray_origin, ray_dir, 10.0f);

        assert(res.hit);
        assert(res.hit_block == glm::ivec3(0, 0, 0));
        assert(res.place_block == glm::ivec3(0, 1, 0));
        assert(res.hit_face == sl::VoxelFace::top);
        assert(std::abs(res.distance - 4.0f) < 0.001f);
    }

    // Raycast: horizontal ray from -5 to +X towards block at (0, 0, 0)
    {
        glm::vec3 ray_origin(-5.0f, 0.5f, 0.5f);
        glm::vec3 ray_dir(1.0f, 0.0f, 0.0f);
        sl::VoxelRaycastResult res = world.raycast(ray_origin, ray_dir, 10.0f);

        assert(res.hit);
        assert(res.hit_block == glm::ivec3(0, 0, 0));
        assert(res.place_block == glm::ivec3(-1, 0, 0));
        assert(res.hit_face == sl::VoxelFace::west);
        assert(std::abs(res.distance - 5.0f) < 0.001f);
    }

    // Raycast: ray with negative direction components starting on exact integer boundary
    {
        // Block placed at (2, 2, 2)
        world.set_block(2, 2, 2, 3);
        // Origin on exact integer boundary (0.0, 18.0, 15.0) pointing down and towards (2, 2, 2)
        glm::vec3 ray_origin(2.5f, 10.0f, 2.5f);
        glm::vec3 ray_dir(0.0f, -1.0f, 0.0f);
        sl::VoxelRaycastResult res = world.raycast(ray_origin, ray_dir, 15.0f);

        assert(res.hit);
        assert(res.hit_block == glm::ivec3(2, 2, 2));
        assert(res.place_block == glm::ivec3(2, 3, 2));
        assert(res.hit_face == sl::VoxelFace::top);
        assert(std::abs(res.distance - 7.0f) < 0.001f);
    }

    // Raycast: ray with negative step from exact integer boundary
    {
        world.set_block(0, 0, 0, 1);
        // Origin at (5.0, 0.5, 0.5) pointing in negative X (-1, 0, 0)
        glm::vec3 ray_origin(5.0f, 0.5f, 0.5f);
        glm::vec3 ray_dir(-1.0f, 0.0f, 0.0f);
        sl::VoxelRaycastResult res = world.raycast(ray_origin, ray_dir, 10.0f);

        assert(res.hit);
        assert(res.hit_block == glm::ivec3(0, 0, 0));
        assert(res.place_block == glm::ivec3(1, 0, 0));
        assert(res.hit_face == sl::VoxelFace::east);
        assert(std::abs(res.distance - 4.0f) < 0.001f);
    }

    // Raycast: all 6 directions hitting a single block at (10, 10, 10)
    {
        world.set_block(10, 10, 10, 4);

        // 1. Ray from +Y (pointing down -Y) -> hits top face, place at (10, 11, 10)
        {
            sl::VoxelRaycastResult res = world.raycast({10.5f, 15.0f, 10.5f}, {0.0f, -1.0f, 0.0f}, 10.0f);
            assert(res.hit);
            assert(res.hit_block == glm::ivec3(10, 10, 10));
            assert(res.hit_face == sl::VoxelFace::top);
            assert(res.place_block == glm::ivec3(10, 11, 10));
            assert(std::abs(res.distance - 4.0f) < 0.001f);
        }

        // 2. Ray from -Y (pointing up +Y) -> hits bottom face, place at (10, 9, 10)
        {
            sl::VoxelRaycastResult res = world.raycast({10.5f, 5.0f, 10.5f}, {0.0f, 1.0f, 0.0f}, 10.0f);
            assert(res.hit);
            assert(res.hit_block == glm::ivec3(10, 10, 10));
            assert(res.hit_face == sl::VoxelFace::bottom);
            assert(res.place_block == glm::ivec3(10, 9, 10));
            assert(std::abs(res.distance - 5.0f) < 0.001f);
        }

        // 3. Ray from +Z (pointing -Z) -> hits north face (+Z face), place at (10, 10, 11)
        {
            sl::VoxelRaycastResult res = world.raycast({10.5f, 10.5f, 15.0f}, {0.0f, 0.0f, -1.0f}, 10.0f);
            assert(res.hit);
            assert(res.hit_block == glm::ivec3(10, 10, 10));
            assert(res.hit_face == sl::VoxelFace::north);
            assert(res.place_block == glm::ivec3(10, 10, 11));
            assert(std::abs(res.distance - 4.0f) < 0.001f);
        }

        // 4. Ray from -Z (pointing +Z) -> hits south face (-Z face), place at (10, 10, 9)
        {
            sl::VoxelRaycastResult res = world.raycast({10.5f, 10.5f, 5.0f}, {0.0f, 0.0f, 1.0f}, 10.0f);
            assert(res.hit);
            assert(res.hit_block == glm::ivec3(10, 10, 10));
            assert(res.hit_face == sl::VoxelFace::south);
            assert(res.place_block == glm::ivec3(10, 10, 9));
            assert(std::abs(res.distance - 5.0f) < 0.001f);
        }

        // 5. Ray from +X (pointing -X) -> hits east face (+X face), place at (11, 10, 10)
        {
            sl::VoxelRaycastResult res = world.raycast({15.0f, 10.5f, 10.5f}, {-1.0f, 0.0f, 0.0f}, 10.0f);
            assert(res.hit);
            assert(res.hit_block == glm::ivec3(10, 10, 10));
            assert(res.hit_face == sl::VoxelFace::east);
            assert(res.place_block == glm::ivec3(11, 10, 10));
            assert(std::abs(res.distance - 4.0f) < 0.001f);
        }

        // 6. Ray from -X (pointing +X) -> hits west face (-X face), place at (9, 10, 10)
        {
            sl::VoxelRaycastResult res = world.raycast({5.0f, 10.5f, 10.5f}, {1.0f, 0.0f, 0.0f}, 10.0f);
            assert(res.hit);
            assert(res.hit_block == glm::ivec3(10, 10, 10));
            assert(res.hit_face == sl::VoxelFace::west);
            assert(res.place_block == glm::ivec3(9, 10, 10));
            assert(std::abs(res.distance - 5.0f) < 0.001f);
        }
    }

    // Raycast: ray that misses
    {
        glm::vec3 ray_origin(0.0f, 10.0f, 0.0f);
        glm::vec3 ray_dir(0.0f, 1.0f, 0.0f); // pointing up away from ground
        sl::VoxelRaycastResult res = world.raycast(ray_origin, ray_dir, 20.0f);

        assert(!res.hit);
    }
}

void test_graphics_mesh_if_available()
{
    std::cout << "Testing GPU mesh creation if graphics backend is available..." << std::endl;

    sl::set_graphics_backend(sl::GraphicsBackend::opengl);
    if (!sl::graphics_backend_available(sl::GraphicsBackend::opengl))
    {
        std::cout << "  OpenGL backend not available, skipping GPU mesh test." << std::endl;
        return;
    }

    if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 320, 240))
    {
        std::cout << "  Failed to initialize graphics mode (" << sl::last_error() << "), skipping GPU mesh test." << std::endl;
        return;
    }

    sl::VoxelChunk chunk(0, 0, 0);
    chunk.set_block(0, 0, 0, 1);
    chunk.rebuild_mesh();

    assert(chunk.mesh() != nullptr);
    assert(chunk.vertex_count() == 24); // 6 faces * 4 vertices
    assert(chunk.triangle_count() == 12); // 6 faces * 2 triangles

    sl::Surface3D *surface = sl::create_surface_3d(128, 128);
    if (surface)
    {
        sl::Camera camera;
        camera.position = {2.0f, 2.0f, 2.0f};
        camera.target = {0.0f, 0.0f, 0.0f};

        assert(sl::begin_surface_3d(surface, camera));
        sl::clear_surface_3d(sl::Colour{20, 30, 40, 255});
        assert(chunk.draw());
        sl::end_surface_3d();

        sl::destroy_surface_3d(surface);
    }

    sl::display_shutdown();
}

int main()
{
    test_block_definitions();
    test_voxel_atlas();
    test_chunk_storage_and_lifecycle();
    test_face_culling();
    test_world_coordinates();
    test_world_management_and_raycast();
    test_graphics_mesh_if_available();

    std::cout << "All voxel unit tests passed successfully!" << std::endl;
    return 0;
}

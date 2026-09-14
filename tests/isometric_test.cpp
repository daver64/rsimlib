#include "sl.h"

#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    sl::IsometricTransform iso;
    iso.config.tile_width = 64.0f;
    iso.config.tile_height = 32.0f;
    iso.config.elevation_height = 16.0f;
    iso.origin_x = 400.0f;
    iso.origin_y = 300.0f;
    iso.zoom = 1.0f;

    // Test 1: Origin projection
    {
        sl::IsoPoint screen = iso.world_to_screen(0.0f, 0.0f, 0.0f);
        assert(std::abs(screen.x - 400.0f) < 0.001f);
        assert(std::abs(screen.y - 300.0f) < 0.001f);

        sl::IsoPoint world = iso.screen_to_world(400.0f, 300.0f, 0.0f);
        assert(std::abs(world.x - 0.0f) < 0.001f);
        assert(std::abs(world.y - 0.0f) < 0.001f);
    }

    // Test 2: Round-trip transformation on multiple coordinates and elevations
    {
        const float test_points[5][3] = {
            {5.0f, 3.0f, 0.0f},
            {-2.5f, 7.2f, 1.5f},
            {12.0f, 12.0f, 4.0f},
            {0.5f, 0.5f, -1.0f},
            {100.0f, -50.0f, 8.0f}
        };

        for (const auto &point : test_points)
        {
            const float wx = point[0];
            const float wy = point[1];
            const float wz = point[2];

            sl::IsoPoint screen = iso.world_to_screen(wx, wy, wz);
            sl::IsoPoint world = iso.screen_to_world(screen.x, screen.y, wz);

            assert(std::abs(world.x - wx) < 0.001f);
            assert(std::abs(world.y - wy) < 0.001f);
        }
    }

    // Test 3: Grid coordinate picking (screen_to_grid)
    {
        // Center of tile (3, 4) at z = 0 is (3.5, 4.5)
        sl::IsoPoint center_screen = iso.world_to_screen(3.5f, 4.5f, 0.0f);
        sl::IsoGridPoint grid = iso.screen_to_grid(center_screen.x, center_screen.y, 0.0f);
        assert(grid.x == 3);
        assert(grid.y == 4);

        // Near corners of tile (3, 4)
        sl::IsoPoint near_top = iso.world_to_screen(3.1f, 4.1f, 0.0f);
        grid = iso.screen_to_grid(near_top.x, near_top.y, 0.0f);
        assert(grid.x == 3);
        assert(grid.y == 4);
    }

    // Test 4: Diamond vertices
    {
        sl::IsoTileDiamond diamond = iso.tile_diamond(0.0f, 0.0f, 0.0f);
        // Center is at (0.5, 0.5) -> (0, 16) from origin (400, 316)
        // Top: (400, 300), Right: (432, 316), Bottom: (400, 332), Left: (368, 316)
        assert(std::abs(diamond.top.x - 400.0f) < 0.001f);
        assert(std::abs(diamond.top.y - 300.0f) < 0.001f);
        assert(std::abs(diamond.right.x - 432.0f) < 0.001f);
        assert(std::abs(diamond.right.y - 316.0f) < 0.001f);
        assert(std::abs(diamond.bottom.x - 400.0f) < 0.001f);
        assert(std::abs(diamond.bottom.y - 332.0f) < 0.001f);
        assert(std::abs(diamond.left.x - 368.0f) < 0.001f);
        assert(std::abs(diamond.left.y - 316.0f) < 0.001f);
    }

    // Test 5: Viewport visible bounds calculation
    {
        sl::IsoBounds bounds = iso.visible_bounds(0.0f, 0.0f, 800.0f, 600.0f, 0.0f, 2.0f);
        assert(bounds.min_x < 0);
        assert(bounds.min_y < 0);
        assert(bounds.max_x > 0);
        assert(bounds.max_y > 0);
    }

    // Test 6: Depth sort key
    {
        // Lower X+Y is behind higher X+Y
        float depth_a = sl::IsometricTransform::depth_sort_key(1.0f, 2.0f, 0.0f);
        float depth_b = sl::IsometricTransform::depth_sort_key(2.0f, 2.0f, 0.0f);
        float depth_c = sl::IsometricTransform::depth_sort_key(2.0f, 2.0f, 1.0f);

        assert(depth_a < depth_b);
        assert(depth_b < depth_c);
    }

    // Test 7: Zoom transformation
    {
        iso.zoom = 2.0f;
        sl::IsoPoint screen = iso.world_to_screen(4.0f, 2.0f, 1.0f);
        sl::IsoPoint world = iso.screen_to_world(screen.x, screen.y, 1.0f);
        assert(std::abs(world.x - 4.0f) < 0.001f);
        assert(std::abs(world.y - 2.0f) < 0.001f);
    }

    std::cout << "All isometric unit tests passed successfully.\n";
    return 0;
}

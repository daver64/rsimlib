#define PATHFINDING_IMPLEMENTATION
#include "pathfinding.h"

#include "animated_sprite.h"

#include <cassert>
#include <iostream>
#include <vector>

namespace
{
    constexpr int columns = 12;
    constexpr int rows = 8;
    std::vector<unsigned char> walls(columns *rows, 0);

    bool blocked(float x, float y, float, float)
    {
        const int col = static_cast<int>(x / 10.0f);
        const int row = static_cast<int>(y / 10.0f);
        return col < 0 || row < 0 || col >= columns || row >= rows || walls[row * columns + col];
    }

    void test_pathfinder()
    {
        // A wall column at x=5 with a gap at y=6 splits the map.
        for (int r = 0; r < rows; ++r)
            walls[r * columns + 5] = 1;
        walls[6 * columns + 5] = 0;

        Pathfinder pf;
        pf.resize(columns, rows, 10.0f, 10.0f, blocked);
        pf.update(1, 1, false);
        assert(pf.value(PathLayer::chase, 1, 1) == 0.0f);
        assert(pf.value(PathLayer::chase, 2, 1) == 1.0f);
        assert(!pf.open(5, 2) && pf.open(5, 6));

        // An enemy on the far side must walk through the gap: following the chase field always reaches the player.
        int col = 10, row = 1;
        bool through_gap = false;
        for (int steps = 0; steps < 100 && !(col == 1 && row == 1); ++steps)
        {
            TileStep step;
            assert(pf.next_step(PathLayer::chase, col, row, step));
            col += step.dx;
            row += step.dy;
            assert(pf.open(col, row));
            through_gap = through_gap || (col == 5 && row == 6);
        }
        assert(col == 1 && row == 1 && through_gap);

        // At the target there is nowhere better to go.
        TileStep step;
        assert(!pf.next_step(PathLayer::chase, 1, 1, step));

        // Sealing the gap makes the far side unreachable: no chase value and no step.
        walls[6 * columns + 5] = 1;
        pf.update(1, 1, false);
        assert(pf.value(PathLayer::chase, 10, 1) < 0.0f);
        assert(!pf.next_step(PathLayer::chase, 10, 1, step));

        // Scent follows the player's trail and fades; sound only appears when the player is noisy and stops at walls.
        pf.update(2, 2, false);
        assert(pf.value(PathLayer::scent, 2, 2) == 1.0f && pf.value(PathLayer::scent, 1, 1) > 0.9f);
        assert(pf.value(PathLayer::sound, 3, 2) == 0.0f);
        pf.update(2, 2, true);
        assert(pf.value(PathLayer::sound, 3, 2) > 0.0f);
        assert(pf.value(PathLayer::sound, 8, 2) == 0.0f);
        assert(pf.value(PathLayer::chase, -1, 0) == 0.0f); // out of range is harmless
    }

    void test_animation()
    {
        sl::Atlas atlas;
        sl::AnimatedSprite sprite = sl::create_animated_sprite(atlas, 4, 3, 10, 20, 32, 32, 0.25f, true, 2);
        assert(sprite.current_frame == 0 && sprite.frames.size() == 3);
        assert(sprite.frames[0].tile_id == 4 && sprite.frames[2].tile_id == 6);
        assert(sprite.frames[1].duration == 0.25f && sprite.one_shot && sprite.one_shot_counts == 2);

        // A default-constructed sprite starts at frame 0 rather than an indeterminate value.
        sl::AnimatedSprite blank;
        assert(blank.current_frame == 0);

        sl::AnimationManager manager;
        const int first = manager.add_animated_sprite(sprite);
        const int second = manager.add_animated_sprite(sprite);
        assert(first == 0 && second == 1);

        assert(manager.set_sprite_position(second, 5, 6));
        int x = 0, y = 0;
        assert(manager.get_sprite_position(second, x, y) && x == 5 && y == 6);
        assert(manager.set_sprite_rotation(first, 725.0f));
        sl::AnimatedSprite *found = nullptr;
        assert(manager.get_sprite(first, &found) && found->rotation_angle == 5.0f);

        std::vector<int> tiles = {9, 8, 7};
        assert(manager.set_sprite_tiles(first, tiles) && found->frames[0].tile_id == 9);
        std::vector<int> wrong = {1};
        assert(!manager.set_sprite_tiles(first, wrong));

        manager.toggle_pause(first);
        assert(found->paused);
        manager.toggle_pause(first);
        assert(!found->paused);

        assert(!manager.get_sprite(5, &found) && found == nullptr);
        assert(!manager.set_sprite_position(-1, 0, 0) && !manager.remove_animated_sprite(2));
        assert(manager.remove_animated_sprite(first));
        // Removing shifts later sprites down.
        assert(manager.get_sprite_position(0, x, y) && x == 5 && y == 6);
        assert(!manager.get_sprite_position(1, x, y));
    }
}

int main()
{
    test_pathfinder();
    test_animation();
    std::cout << "pathfinder_test passed\n";
    return 0;
}

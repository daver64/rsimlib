#include "sl.h"

#include <cassert>
#include <iostream>

namespace
{
    using sl::FluidElement;

    // A closed box: solid floor and walls on the outer layer, empty interior.
    sl::FluidSimulation3D *make_box(int w, int h, int d)
    {
        sl::FluidSimulation3D *sim = sl::create_fluid_simulation_3d(w, h, d);
        assert(sim);
        for (int y = 0; y < h; ++y)
            for (int z = 0; z < d; ++z)
                for (int x = 0; x < w; ++x)
                    if (x == 0 || y == 0 || z == 0 || x == w - 1 || y == h - 1 || z == d - 1)
                        sl::fluid3d_set_cell(sim, x, y, z, FluidElement::solid);
        return sim;
    }

    int count_of(const sl::FluidSimulation3D *sim, FluidElement type)
    {
        int n = 0;
        for (const sl::FluidCell &c : sim->cells)
            n += c.type == type;
        return n;
    }

    void test_creation()
    {
        assert(!sl::create_fluid_simulation_3d(2, 8, 8));
        sl::FluidSimulation3D *sim = sl::create_fluid_simulation_3d(10, 12, 14);
        assert(sim && sim->width == 10 && sim->height == 12 && sim->depth == 14);
        assert(sim->cells.size() == 10u * 12u * 14u);
        assert(sl::fluid3d_set_cell(sim, 3, 4, 5, FluidElement::water));
        assert(sl::fluid3d_get_cell(sim, 3, 4, 5).type == FluidElement::water);
        assert(!sl::fluid3d_set_cell(sim, 10, 0, 0, FluidElement::water));
        assert(sl::fluid3d_get_cell(sim, -1, 0, 0).type == FluidElement::empty);
        sl::destroy_fluid_simulation_3d(sim);
    }

    void test_gravity_and_conservation()
    {
        sl::FluidSimulation3D *sim = make_box(12, 12, 12);
        sl::fluid3d_set_cell(sim, 6, 9, 6, FluidElement::sand);
        sl::fluid3d_set_cell(sim, 3, 9, 3, FluidElement::water);
        for (int i = 0; i < 40; ++i)
            sl::fluid3d_step(sim);
        assert(sl::fluid3d_get_cell(sim, 6, 1, 6).type == FluidElement::sand);
        assert(sl::fluid3d_get_cell(sim, 3, 1, 3).type == FluidElement::water);
        assert(count_of(sim, FluidElement::sand) == 1);
        assert(count_of(sim, FluidElement::water) == 1);
        sl::destroy_fluid_simulation_3d(sim);
    }

    void test_water_spreads_and_levels()
    {
        sl::FluidSimulation3D *sim = make_box(12, 14, 12);
        // A 3x3x3 block of water dropped near the middle.
        for (int y = 6; y < 9; ++y)
            for (int z = 4; z < 7; ++z)
                for (int x = 4; x < 7; ++x)
                    sl::fluid3d_set_cell(sim, x, y, z, FluidElement::water);
        for (int i = 0; i < 400; ++i)
            sl::fluid3d_step(sim);
        assert(count_of(sim, FluidElement::water) == 27);
        // All water must have settled to the floor layers (100 floor cells, 27 water cells fit in layer 1).
        int on_floor = 0;
        for (int z = 1; z < 11; ++z)
            for (int x = 1; x < 11; ++x)
                on_floor += sl::fluid3d_get_cell(sim, x, 1, z).type == FluidElement::water;
        assert(on_floor >= 24);
        sl::destroy_fluid_simulation_3d(sim);
    }

    void test_density_ordering()
    {
        sl::FluidSimulation3D *sim = make_box(5, 10, 5);
        sl::fluid3d_set_cell(sim, 2, 1, 2, FluidElement::oil);
        sl::fluid3d_set_cell(sim, 2, 2, 2, FluidElement::water);
        sl::fluid3d_step(sim);
        // Water is denser than oil, so the two swap in a single step.
        assert(sl::fluid3d_get_cell(sim, 2, 1, 2).type == FluidElement::water);
        assert(sl::fluid3d_get_cell(sim, 2, 2, 2).type == FluidElement::oil);
        for (int i = 0; i < 60; ++i)
            sl::fluid3d_step(sim);
        assert(count_of(sim, FluidElement::oil) == 1 && count_of(sim, FluidElement::water) == 1);
        sl::destroy_fluid_simulation_3d(sim);
    }

    void test_lava_and_water()
    {
        sl::FluidSimulation3D *sim = make_box(7, 8, 7);
        sl::fluid3d_set_cell(sim, 3, 1, 3, FluidElement::lava, 1.0f, 1200.0f);
        sl::fluid3d_set_cell(sim, 3, 2, 3, FluidElement::water);
        for (int i = 0; i < 5; ++i)
            sl::fluid3d_step(sim);
        assert(count_of(sim, FluidElement::lava) == 0);
        assert(sl::fluid3d_get_cell(sim, 3, 1, 3).type == FluidElement::solid);
        sl::destroy_fluid_simulation_3d(sim);
    }

    void test_fire_spreads_through_plants_and_lava_ignites_wood()
    {
        sl::FluidSimulation3D *sim = make_box(14, 8, 5);
        for (int x = 2; x < 12; ++x)
            sl::fluid3d_set_cell(sim, x, 1, 2, FluidElement::plant);
        sl::fluid3d_set_cell(sim, 2, 2, 2, FluidElement::fire, 0.0f, 800.0f, 90);
        for (int i = 0; i < 100; ++i)
            sl::fluid3d_step(sim);
        // Plants always ignite from adjacent fire, so the whole row burns.
        assert(count_of(sim, FluidElement::plant) == 0);
        sl::destroy_fluid_simulation_3d(sim);

        // Lava ignites the wood it rests on.
        sim = make_box(5, 6, 5);
        sl::fluid3d_set_cell(sim, 2, 1, 2, FluidElement::wood);
        sl::fluid3d_set_cell(sim, 2, 2, 2, FluidElement::lava, 1.0f, 1200.0f);
        sl::fluid3d_step(sim);
        assert(sl::fluid3d_get_cell(sim, 2, 1, 2).type == FluidElement::fire);
        sl::destroy_fluid_simulation_3d(sim);
    }

    void test_gas_rises()
    {
        sl::FluidSimulation3D *sim = make_box(5, 12, 5);
        sl::fluid3d_set_cell(sim, 2, 1, 2, FluidElement::steam, 0.0f, 100.0f, 200);
        for (int i = 0; i < 20; ++i)
            sl::fluid3d_step(sim);
        // It reaches the ceiling layer, then drifts sideways along it.
        int at_ceiling = 0;
        for (int z = 1; z < 4; ++z)
            for (int x = 1; x < 4; ++x)
                at_ceiling += sl::fluid3d_get_cell(sim, x, 10, z).type == FluidElement::steam;
        assert(at_ceiling == 1);
        sl::destroy_fluid_simulation_3d(sim);
    }

    void test_determinism_and_sleeping()
    {
        auto run = []()
        {
            sl::FluidSimulation3D *sim = make_box(16, 16, 16);
            for (int y = 8; y < 12; ++y)
                for (int x = 4; x < 9; ++x)
                    sl::fluid3d_set_cell(sim, x, y, 6, (x + y) % 2 ? FluidElement::sand : FluidElement::water);
            for (int i = 0; i < 120; ++i)
                sl::fluid3d_step(sim);
            return sim;
        };
        sl::FluidSimulation3D *a = run();
        sl::FluidSimulation3D *b = run();
        for (std::size_t i = 0; i < a->cells.size(); ++i)
            assert(a->cells[i].type == b->cells[i].type);

        // Settled content goes to sleep, and writing a cell wakes its block.
        bool any_awake = false;
        for (std::uint8_t f : a->awake)
            any_awake = any_awake || f;
        assert(!any_awake);
        sl::fluid3d_set_cell(a, 8, 12, 8, FluidElement::sand);
        any_awake = false;
        for (std::uint8_t f : a->awake)
            any_awake = any_awake || f;
        assert(any_awake);
        sl::destroy_fluid_simulation_3d(a);
        sl::destroy_fluid_simulation_3d(b);
    }

    void test_block_budget_still_converges()
    {
        sl::FluidSimulation3D *sim = make_box(20, 20, 20);
        sim->max_blocks_per_step = 2;
        for (int y = 10; y < 14; ++y)
            sl::fluid3d_set_cell(sim, 10, y, 10, FluidElement::sand);
        for (int i = 0; i < 400; ++i)
            sl::fluid3d_step(sim);
        assert(count_of(sim, FluidElement::sand) == 4);
        assert(sl::fluid3d_get_cell(sim, 10, 1, 10).type == FluidElement::sand);
        sl::destroy_fluid_simulation_3d(sim);
    }

    void test_voxel_bridge()
    {
        const sl::FluidVoxelBlocks blocks;
        sl::fluid3d_register_blocks(blocks);
        assert(!sl::get_block_definition(blocks.water).is_solid);
        assert(sl::get_block_definition(blocks.water).is_transparent);
        assert(sl::get_block_definition(blocks.sand).is_solid);

        sl::VoxelWorld world;
        constexpr sl::BlockId STONE = 3;
        // A stone floor and a stone cup at world offset (100, 20, -30).
        const int ox = 100, oy = 20, oz = -30;
        for (int z = 0; z < 8; ++z)
            for (int x = 0; x < 8; ++x)
                world.set_block(ox + x, oy, oz + z, STONE);
        world.set_block(ox + 3, oy + 5, oz + 3, blocks.water);

        sl::FluidSimulation3D *sim = sl::create_fluid_simulation_3d(8, 8, 8);
        sl::fluid3d_set_origin(sim, ox, oy, oz);
        sl::fluid3d_import_from_voxels(sim, world, blocks);
        assert(sl::fluid3d_get_cell(sim, 3, 0, 3).type == FluidElement::solid);
        assert(sl::fluid3d_get_cell(sim, 3, 5, 3).type == FluidElement::water);
        assert(sl::fluid3d_get_cell(sim, 3, 3, 3).type == FluidElement::empty);

        // Nothing changed yet: export writes nothing.
        assert(sl::fluid3d_export_to_voxels(sim, world, blocks) == 0);

        for (int i = 0; i < 30; ++i)
            sl::fluid3d_step(sim);
        const std::size_t changed = sl::fluid3d_export_to_voxels(sim, world, blocks);
        assert(changed == 2); // water left its start voxel and landed on the floor
        assert(world.get_block(ox + 3, oy + 5, oz + 3) == sl::BLOCK_AIR);
        assert(world.get_block(ox + 3, oy + 1, oz + 3) == blocks.water);
        // Solids imported from the world are never rewritten.
        assert(world.get_block(ox + 3, oy, oz + 3) == STONE);
        assert(sl::fluid3d_export_to_voxels(sim, world, blocks) == 0);

        // Moving the region freezes the fluid in the world and reads it back as water.
        sl::fluid3d_move_region(sim, world, ox + 1, oy, oz + 1, blocks);
        assert(sim->origin_x == ox + 1);
        assert(sl::fluid3d_get_cell(sim, 2, 1, 2).type == FluidElement::water);
        sl::destroy_fluid_simulation_3d(sim);
    }
}

int main()
{
    test_creation();
    test_gravity_and_conservation();
    test_water_spreads_and_levels();
    test_density_ordering();
    test_lava_and_water();
    test_fire_spreads_through_plants_and_lava_ignites_wood();
    test_gas_rises();
    test_determinism_and_sleeping();
    test_block_budget_still_converges();
    test_voxel_bridge();
    std::cout << "All 3D fluid tests passed successfully!" << std::endl;
    return 0;
}

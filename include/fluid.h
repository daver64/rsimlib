#pragma once

#include <cstdint>
#include <vector>

namespace sl
{
    enum class FluidElement : std::uint8_t
    {
        empty = 0,
        solid,
        wood,
        sand,
        water,
        oil,
        acid,
        lava,
        gunpowder,
        plant,
        pump,
        fire,
        smoke,
        steam
    };

    struct FluidCell
    {
        FluidElement type = FluidElement::empty;
        float mass = 0.0f;
        float temp = 20.0f;
        std::uint8_t life = 0;
        std::int8_t variation = 0;
        std::uint32_t turn = 0;
    };

    struct FluidSimulation
    {
        int width = 0;
        int height = 0;
        std::vector<FluidCell> cells;
        std::uint32_t turn = 0;
    };

    /** Create a fixed-size cellular automata simulation grid. */
    FluidSimulation *create_fluid_simulation(int width = 200, int height = 150);
    /** Destroy a fluid simulation and release all state owned by it. */
    void destroy_fluid_simulation(FluidSimulation *simulation);

    /** Clear all cells to empty and restore the default solid border. */
    void fluid_clear(FluidSimulation *simulation);
    /** Restore the default solid border without clearing remaining cells. */
    void fluid_reset_bounds(FluidSimulation *simulation);
    /** Advance the simulation by one update tick. */
    void fluid_step(FluidSimulation *simulation);

    /** Fill a cell with an explicit fluid state. Returns false when coordinates are out of range. */
    bool fluid_set_cell(FluidSimulation *simulation, int x, int y, const FluidCell &cell);
    /** Convenience overload for setting a cell by element type and simple scalar state. */
    bool fluid_set_cell(FluidSimulation *simulation, int x, int y, FluidElement type,
                        float mass = 0.0f, float temp = 20.0f,
                        std::uint8_t life = 0, std::int8_t variation = 0);
    /** Read the current cell value. Returns an empty/default cell for invalid coordinates. */
    FluidCell fluid_get_cell(const FluidSimulation *simulation, int x, int y);

    /** Return whether a coordinate lies within the active simulation bounds. */
    bool fluid_in_bounds(const FluidSimulation *simulation, int x, int y);
    /** Return the simulation dimensions in cells. */
    int fluid_width(const FluidSimulation *simulation);
    int fluid_height(const FluidSimulation *simulation);
    /** Return the current step index for the simulation. */
    std::uint32_t fluid_turn(const FluidSimulation *simulation);

} // namespace sl

/** @file
 * @brief Cellular Automata Fluid & Falling Sand physics simulation with Box2D rigid body integration.
 * Based on principles from Tom Forsyth's "Cellular Automata for Physical Modelling".
 */

#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

namespace
{
    constexpr int SCREEN_W = 800;
    constexpr int SCREEN_H = 600;
    constexpr int GRID_W = 200;
    constexpr int GRID_H = 150;
    constexpr float CELL_SCALE = 4.0f; // 4 screen pixels per grid cell

    enum class ElementType : std::uint8_t
    {
        Empty = 0,
        Solid,       // Indestructible wall/bedrock
        Wood,        // Flammable building material
        Sand,        // Granular falling powder
        Water,       // Incompressible / slightly compressible liquid (density ~1.0)
        Oil,         // Flammable lighter liquid (density ~0.6, floats on water)
        Acid,        // Corrosive liquid, dissolves organic matter
        Fire,        // Active flame emitting heat, ignites fuel
        Smoke,       // Rising gas, dissipates
        Steam        // Evaporated water from fire
    };

    struct ElementProps
    {
        const char *name;
        float density;       // For fluid/granular buoyancy & sinking order
        bool is_liquid;
        bool is_gas;
        bool is_solid;
        bool flammable;
        float flashpoint;    // Deg C to catch fire
        sl::Colour base_colour;
    };

    const ElementProps ELEMENT_PROPERTIES[] = {
        {"Empty", 0.0f, false, false, false, false, 9999.0f, {16, 20, 28, 255}},
        {"Solid", 999.0f, false, false, true, false, 9999.0f, {120, 128, 142, 255}},
        {"Wood", 0.7f, false, false, true, true, 220.0f, {142, 92, 54, 255}},
        {"Sand", 1.8f, false, false, false, false, 9999.0f, {235, 195, 95, 255}},
        {"Water", 1.0f, true, false, false, false, 9999.0f, {45, 130, 240, 235}},
        {"Oil", 0.65f, true, false, false, true, 75.0f, {130, 95, 35, 245}},
        {"Acid", 1.25f, true, false, false, false, 9999.0f, {80, 240, 65, 240}},
        {"Fire", -0.2f, false, true, false, false, 0.0f, {255, 120, 30, 255}},
        {"Smoke", -0.1f, false, true, false, false, 9999.0f, {90, 95, 105, 180}},
        {"Steam", -0.15f, false, true, false, false, 9999.0f, {190, 210, 235, 170}}
    };

    struct Cell
    {
        ElementType type = ElementType::Empty;
        float mass = 0.0f;       // Fluid mass/pressure: 0.0 - 1.5 (Tom Forsyth liquid model)
        float temp = 20.0f;      // Temperature in Celsius (ambient = 20C)
        std::uint8_t life = 0;   // Lifetime timer for dynamic particles (fire, smoke, steam, burning wood)
        std::int8_t variation = 0; // Visual noise/shade variation (-15 .. +15)
        std::uint32_t turn = 0;  // Update turn tracking to avoid updating twice per tick
    };

    struct DynamicBox
    {
        sl::PhysicsBody *body = nullptr;
        float width = 32.0f;
        float height = 32.0f;
        float density = 0.5f;    // Wood crate floats (< 1.0), Metal barrel sinks (> 1.0)
        bool is_wood = true;
        sl::Colour colour{185, 125, 75};
    };

    class SimulationGrid
    {
    public:
        SimulationGrid()
        {
            cells_.resize(GRID_W * GRID_H);
            rng_.seed(1337);
            init_bounds();
        }

        void clear()
        {
            for (int y = 0; y < GRID_H; ++y)
            {
                for (int x = 0; x < GRID_W; ++x)
                {
                    Cell &c = at(x, y);
                    c.type = ElementType::Empty;
                    c.mass = 0.0f;
                    c.temp = 20.0f;
                    c.life = 0;
                    c.variation = 0;
                    c.turn = 0;
                }
            }
            init_bounds();
        }

        void init_bounds()
        {
            // Outer bounding walls
            for (int x = 0; x < GRID_W; ++x)
            {
                set(x, 0, ElementType::Solid);
                set(x, GRID_H - 1, ElementType::Solid);
            }
            for (int y = 0; y < GRID_H; ++y)
            {
                set(0, y, ElementType::Solid);
                set(GRID_W - 1, y, ElementType::Solid);
            }
        }

        void load_scenario(int index)
        {
            clear();
            switch (index % 4)
            {
            case 0: // Preset 0: U-Tube / Hydraulic Equalization Tank (Forsyth compressible water demo)
            {
                // Left & right tanks joined by a bottom pipe
                for (int y = 30; y < 120; ++y)
                {
                    set(70, y, ElementType::Solid);
                    set(130, y, ElementType::Solid);
                }
                // Central divider forming the U-tube
                for (int y = 30; y < 105; ++y)
                {
                    set(100, y, ElementType::Solid);
                }
                // Fill left side with water to demonstrate hydrostatic pressure flow through the bottom
                for (int y = 40; y < 118; ++y)
                {
                    for (int x = 72; x < 98; ++x)
                    {
                        set(x, y, ElementType::Water, 1.0f);
                    }
                }
                // Floating oil layer on top of left water
                for (int y = 36; y < 40; ++y)
                {
                    for (int x = 72; x < 98; ++x)
                    {
                        set(x, y, ElementType::Oil, 0.8f);
                    }
                }
                break;
            }
            case 1: // Preset 1: Sand Funnel & Hourglass
            {
                // Funnel walls
                for (int i = 0; i < 45; ++i)
                {
                    set(50 + i, 30 + i, ElementType::Solid);
                    set(150 - i, 30 + i, ElementType::Solid);
                }
                // Sand pool in the top hopper
                for (int y = 20; y < 65; ++y)
                {
                    for (int x = 60; x < 140; ++x)
                    {
                        if (get(x, y).type == ElementType::Empty)
                            set(x, y, ElementType::Sand);
                    }
                }
                // Catch basin below
                for (int x = 40; x < 160; ++x)
                {
                    set(x, 130, ElementType::Solid);
                }
                for (int y = 100; y < 130; ++y)
                {
                    set(40, y, ElementType::Solid);
                    set(160, y, ElementType::Solid);
                }
                break;
            }
            case 2: // Preset 2: Oil Refinery & Fire Inferno
            {
                // Multi-tier oil tanks and wooden scaffolding
                for (int y = 40; y < 70; ++y)
                {
                    set(30, y, ElementType::Wood);
                    set(85, y, ElementType::Wood);
                }
                for (int x = 30; x <= 85; ++x)
                {
                    set(x, 70, ElementType::Wood);
                }
                // Oil inside the top tank
                for (int y = 45; y < 69; ++y)
                {
                    for (int x = 32; x < 84; ++x)
                    {
                        set(x, y, ElementType::Oil, 1.0f);
                    }
                }
                // Wooden barrier holding back water
                for (int y = 80; y < 130; ++y)
                {
                    set(130, y, ElementType::Wood);
                }
                for (int y = 90; y < 130; ++y)
                {
                    for (int x = 132; x < 185; ++x)
                    {
                        set(x, y, ElementType::Water, 1.0f);
                    }
                }
                // Spark of fire to start combustion
                set(35, 38, ElementType::Fire, 0.0f, 600.0f, 180);
                break;
            }
            case 3: // Preset 3: Acid Chamber & Complex Terrain
            {
                // Staggered slopes
                for (int i = 0; i < 40; ++i)
                {
                    set(20 + i, 40 + i / 2, ElementType::Solid);
                    set(180 - i, 70 + i / 2, ElementType::Solid);
                    set(20 + i, 100 + i / 2, ElementType::Wood);
                }
                // Acid reservoir
                for (int y = 20; y < 38; ++y)
                {
                    for (int x = 25; x < 50; ++x)
                    {
                        set(x, y, ElementType::Acid, 1.0f);
                    }
                }
                // Water & Sand beds below
                for (int y = 115; y < 145; ++y)
                {
                    for (int x = 60; x < 110; ++x)
                    {
                        set(x, y, ElementType::Water, 1.0f);
                    }
                    for (int x = 115; x < 170; ++x)
                    {
                        set(x, y, ElementType::Sand);
                    }
                }
                break;
            }
            }
        }

        Cell &at(int x, int y)
        {
            return cells_[y * GRID_W + x];
        }

        const Cell &get(int x, int y) const
        {
            if (x < 0 || x >= GRID_W || y < 0 || y >= GRID_H)
            {
                static const Cell solid_wall{ElementType::Solid, 0.0f, 20.0f, 0, 0, 0};
                return solid_wall;
            }
            return cells_[y * GRID_W + x];
        }

        void set(int x, int y, ElementType type, float mass = 0.0f, float temp = 20.0f, std::uint8_t life = 0)
        {
            if (x <= 0 || x >= GRID_W - 1 || y <= 0 || y >= GRID_H - 1) return;
            Cell &c = at(x, y);
            c.type = type;
            c.mass = (mass > 0.0f) ? mass : (ELEMENT_PROPERTIES[static_cast<std::size_t>(type)].is_liquid ? 1.0f : 0.0f);
            c.temp = temp;
            c.life = life > 0 ? life : (type == ElementType::Fire ? 50 : (type == ElementType::Smoke || type == ElementType::Steam ? 60 : 0));
            c.variation = static_cast<std::int8_t>((rng_() % 21) - 10);
            c.turn = current_turn_;
        }

        void paint(int cx, int cy, ElementType type, int radius)
        {
            for (int dy = -radius; dy <= radius; ++dy)
            {
                for (int dx = -radius; dx <= radius; ++dx)
                {
                    if (dx * dx + dy * dy <= radius * radius)
                    {
                        int x = cx + dx;
                        int y = cy + dy;
                        if (x > 0 && x < GRID_W - 1 && y > 0 && y < GRID_H - 1)
                        {
                            if (type == ElementType::Empty || get(x, y).type != ElementType::Solid)
                            {
                                set(x, y, type);
                            }
                        }
                    }
                }
            }
        }

        void update()
        {
            ++current_turn_;

            // Scan bottom-to-top so falling gravity executes naturally without multi-stepping in a single frame.
            // Alternating left/right scan direction eliminates directional bias (Tom Forsyth's core principle).
            const bool scan_left_to_right = (current_turn_ % 2 == 0);

            for (int y = GRID_H - 2; y >= 1; --y)
            {
                const int x_start = scan_left_to_right ? 1 : GRID_W - 2;
                const int x_end = scan_left_to_right ? GRID_W - 1 : 0;
                const int x_step = scan_left_to_right ? 1 : -1;

                for (int x = x_start; x != x_end; x += x_step)
                {
                    Cell &cell = at(x, y);
                    if (cell.type == ElementType::Empty || cell.type == ElementType::Solid || cell.turn == current_turn_)
                    {
                        continue;
                    }

                    // 1. Thermal conduction & convection simulation
                    update_heat(x, y);

                    // 2. Element specific physical movement and chemical reaction
                    switch (cell.type)
                    {
                    case ElementType::Sand:
                        update_sand(x, y);
                        break;
                    case ElementType::Water:
                    case ElementType::Oil:
                    case ElementType::Acid:
                        update_liquid(x, y);
                        break;
                    case ElementType::Fire:
                        update_fire(x, y);
                        break;
                    case ElementType::Smoke:
                    case ElementType::Steam:
                        update_gas(x, y);
                        break;
                    case ElementType::Wood:
                        update_wood(x, y);
                        break;
                    default:
                        break;
                    }
                }
            }
        }

        void render_to_bitmap(sl::Bitmap *bitmap) const
        {
            if (!bitmap || bitmap->pixels.size() < static_cast<std::size_t>(GRID_W * GRID_H * 4))
            {
                return;
            }

            std::uint8_t *dest = bitmap->pixels.data();
            for (int y = 0; y < GRID_H; ++y)
            {
                for (int x = 0; x < GRID_W; ++x)
                {
                    const Cell &cell = get(x, y);
                    const ElementProps &props = ELEMENT_PROPERTIES[static_cast<std::size_t>(cell.type)];
                    sl::Colour c = props.base_colour;

                    // Apply visual shading based on mass, temperature, and cell variation
                    if (cell.type == ElementType::Water)
                    {
                        // Pressurized / deeper water is slightly darker/richer blue
                        const int depth_tint = std::clamp(static_cast<int>((cell.mass - 0.5f) * 40.0f), -20, 40);
                        c.red = static_cast<std::uint8_t>(std::clamp(c.red - depth_tint, 10, 255));
                        c.green = static_cast<std::uint8_t>(std::clamp(c.green + depth_tint / 2, 50, 255));
                        c.blue = static_cast<std::uint8_t>(std::clamp(c.blue + cell.variation, 180, 255));
                    }
                    else if (cell.type == ElementType::Sand || cell.type == ElementType::Wood || cell.type == ElementType::Solid)
                    {
                        // Subtle grain texture
                        c.red = static_cast<std::uint8_t>(std::clamp(static_cast<int>(c.red) + cell.variation * 2, 0, 255));
                        c.green = static_cast<std::uint8_t>(std::clamp(static_cast<int>(c.green) + cell.variation * 2, 0, 255));
                        c.blue = static_cast<std::uint8_t>(std::clamp(static_cast<int>(c.blue) + cell.variation, 0, 255));
                    }
                    else if (cell.type == ElementType::Fire)
                    {
                        // Dynamic flame color: hot yellow center fading to glowing red/orange
                        const float heat_ratio = std::clamp(static_cast<float>(cell.life) / 50.0f, 0.0f, 1.0f);
                        c.red = 255;
                        c.green = static_cast<std::uint8_t>(std::clamp(static_cast<int>(60.0f + heat_ratio * 180.0f) + cell.variation * 3, 40, 255));
                        c.blue = static_cast<std::uint8_t>(std::clamp(static_cast<int>(heat_ratio * 90.0f), 0, 255));
                    }
                    else if (cell.type == ElementType::Smoke)
                    {
                        const float alpha_fade = std::clamp(static_cast<float>(cell.life) / 60.0f, 0.2f, 1.0f);
                        c.red = static_cast<std::uint8_t>(c.red * alpha_fade);
                        c.green = static_cast<std::uint8_t>(c.green * alpha_fade);
                        c.blue = static_cast<std::uint8_t>(c.blue * alpha_fade);
                    }

                    // Background ambient color for empty cells
                    if (cell.type == ElementType::Empty)
                    {
                        // Faint gradient background
                        c.red = 16 + y / 18;
                        c.green = 20 + y / 16;
                        c.blue = 30 + y / 12;
                    }

                    const std::size_t offset = (static_cast<std::size_t>(y) * GRID_W + x) * 4;
                    dest[offset + 0] = c.red;
                    dest[offset + 1] = c.green;
                    dest[offset + 2] = c.blue;
                    dest[offset + 3] = c.alpha;
                }
            }
            bitmap->ram_dirty = true;
        }

        int count_elements(ElementType type) const
        {
            int count = 0;
            for (const Cell &c : cells_)
            {
                if (c.type == type) ++count;
            }
            return count;
        }

        std::uint32_t current_turn() const { return current_turn_; }

    private:
        void update_heat(int x, int y)
        {
            Cell &cell = at(x, y);
            if (cell.temp <= 20.0f && cell.type != ElementType::Fire) return;

            // Tom Forsyth heat conduction & upward convection hack:
            // Conduction transfers heat energy to 4 neighbors with a 3x upward bias
            const int dxs[] = {0, -1, 1, 0};
            const int dys[] = {-1, 0, 0, 1}; // -1 is UP
            const float weights[] = {0.40f, 0.15f, 0.15f, 0.05f}; // Convection hack: up gets higher weight!

            for (int i = 0; i < 4; ++i)
            {
                const int nx = x + dxs[i];
                const int ny = y + dys[i];
                if (nx <= 0 || nx >= GRID_W - 1 || ny <= 0 || ny >= GRID_H - 1) continue;

                Cell &neigh = at(nx, ny);
                if (neigh.type == ElementType::Solid) continue;

                const float diff = cell.temp - neigh.temp;
                if (diff > 0.0f)
                {
                    const float transfer = diff * weights[i] * 0.5f;
                    cell.temp -= transfer;
                    neigh.temp += transfer;

                    // Flammable check (e.g. wood or oil catching fire from hot air/neighbors)
                    if (ELEMENT_PROPERTIES[static_cast<std::size_t>(neigh.type)].flammable &&
                        neigh.temp >= ELEMENT_PROPERTIES[static_cast<std::size_t>(neigh.type)].flashpoint)
                    {
                        neigh.type = ElementType::Fire;
                        neigh.life = 60;
                        neigh.temp = std::max(neigh.temp, 500.0f);
                    }
                }
            }

            // Ambient heat dissipation
            if (cell.type != ElementType::Fire)
            {
                cell.temp += (20.0f - cell.temp) * 0.02f;
            }
        }

        void update_sand(int x, int y)
        {
            Cell &cell = at(x, y);
            cell.turn = current_turn_;

            const int down_y = y + 1;
            if (down_y >= GRID_H - 1) return;

            // 1. Direct fall straight down (displacing empty air or lighter liquids)
            Cell &below = at(x, down_y);
            if (can_sand_displace(below.type))
            {
                swap_cells(x, y, x, down_y);
                return;
            }

            // 2. Roll diagonally (angle of repose)
            const bool try_left_first = (rng_() % 2 == 0);
            const int dx1 = try_left_first ? -1 : 1;
            const int dx2 = -dx1;

            if (can_sand_displace(get(x + dx1, down_y).type) && can_sand_pass_through(get(x + dx1, y).type))
            {
                swap_cells(x, y, x + dx1, down_y);
                return;
            }
            if (can_sand_displace(get(x + dx2, down_y).type) && can_sand_pass_through(get(x + dx2, y).type))
            {
                swap_cells(x, y, x + dx2, down_y);
                return;
            }
        }

        bool can_sand_displace(ElementType target) const
        {
            return target == ElementType::Empty || target == ElementType::Water ||
                   target == ElementType::Oil || target == ElementType::Smoke ||
                   target == ElementType::Steam || target == ElementType::Fire;
        }

        bool can_sand_pass_through(ElementType target) const
        {
            return target == ElementType::Empty || target == ElementType::Water ||
                   target == ElementType::Oil || target == ElementType::Smoke ||
                   target == ElementType::Steam || target == ElementType::Fire;
        }

        void update_liquid(int x, int y)
        {
            Cell &cell = at(x, y);
            cell.turn = current_turn_;

            // Special chemistry: Acid dissolves solids/wood/sand
            if (cell.type == ElementType::Acid)
            {
                const int ncoords[4][2] = {{x, y + 1}, {x - 1, y}, {x + 1, y}, {x, y - 1}};
                for (const auto &coord : ncoords)
                {
                    Cell &n = at(coord[0], coord[1]);
                    if (n.type == ElementType::Wood || n.type == ElementType::Sand ||
                        (n.type == ElementType::Solid && coord[0] > 1 && coord[0] < GRID_W - 2 && coord[1] > 1 && coord[1] < GRID_H - 2))
                    {
                        if (rng_() % 4 == 0)
                        {
                            n.type = ElementType::Smoke;
                            n.life = 35;
                            cell.type = ElementType::Empty;
                            cell.mass = 0.0f;
                            return;
                        }
                    }
                }
            }

            // Tom Forsyth Compressible Liquid Model:
            // Downward flow -> Diagonal roll -> Hydrostatic horizontal & upward pressure equalization
            const int down_y = y + 1;
            if (down_y >= GRID_H - 1) return;

            // Step A: Fall down into empty air or displace lighter liquid (e.g. water sinks under oil)
            Cell &below = at(x, down_y);
            if (below.type == ElementType::Empty)
            {
                swap_cells(x, y, x, down_y);
                return;
            }
            if (below.type == ElementType::Fire)
            {
                // Water/Acid extinguishes fire creating steam
                below.type = ElementType::Steam;
                below.life = 40;
                cell.type = ElementType::Empty;
                cell.mass = 0.0f;
                return;
            }
            if (ELEMENT_PROPERTIES[static_cast<std::size_t>(below.type)].is_liquid &&
                ELEMENT_PROPERTIES[static_cast<std::size_t>(below.type)].density < ELEMENT_PROPERTIES[static_cast<std::size_t>(cell.type)].density)
            {
                // Heavy liquid sinks below lighter liquid (e.g. Water sinks below Oil)
                swap_cells(x, y, x, down_y);
                return;
            }

            // Step B: Diagonal flow
            const bool try_left_first = ((x + y + current_turn_) % 2 == 0);
            const int dx1 = try_left_first ? -1 : 1;
            const int dx2 = -dx1;

            if (at(x + dx1, down_y).type == ElementType::Empty && at(x + dx1, y).type == ElementType::Empty)
            {
                swap_cells(x, y, x + dx1, down_y);
                return;
            }
            if (at(x + dx2, down_y).type == ElementType::Empty && at(x + dx2, y).type == ElementType::Empty)
            {
                swap_cells(x, y, x + dx2, down_y);
                return;
            }

            // Step C: Horizontal spread / equalization (Liquid leveling out)
            if (at(x + dx1, y).type == ElementType::Empty)
            {
                swap_cells(x, y, x + dx1, y);
                return;
            }
            if (at(x + dx2, y).type == ElementType::Empty)
            {
                swap_cells(x, y, x + dx2, y);
                return;
            }

            // Step D: Hydrostatic pressure push through submerged pipes / U-tubes (Forsyth compression rule)
            // If liquid is boxed in and under pressure, push liquid sideways or up if neighbor has less pressure
            if (cell.type == ElementType::Water && cell.mass > 0.8f)
            {
                for (int dir : {dx1, dx2})
                {
                    Cell &side = at(x + dir, y);
                    if (side.type == ElementType::Water && side.mass < cell.mass - 0.05f)
                    {
                        const float flow = (cell.mass - side.mass) * 0.25f;
                        cell.mass -= flow;
                        side.mass += flow;
                        return;
                    }
                }
            }
        }

        void update_fire(int x, int y)
        {
            Cell &cell = at(x, y);
            cell.turn = current_turn_;
            cell.temp = std::max(cell.temp, 650.0f);

            if (cell.life == 0)
            {
                // Turn to smoke when burning out
                if (rng_() % 3 == 0)
                {
                    cell.type = ElementType::Smoke;
                    cell.life = 40;
                }
                else
                {
                    cell.type = ElementType::Empty;
                    cell.mass = 0.0f;
                }
                return;
            }
            --cell.life;

            // Ignite neighbors
            const int ncoords[4][2] = {{x, y - 1}, {x - 1, y}, {x + 1, y}, {x, y + 1}};
            for (const auto &coord : ncoords)
            {
                Cell &n = at(coord[0], coord[1]);
                if (n.type == ElementType::Oil)
                {
                    n.type = ElementType::Fire;
                    n.life = 70;
                    n.temp = 800.0f;
                }
                else if (n.type == ElementType::Wood)
                {
                    if (rng_() % 6 == 0)
                    {
                        n.type = ElementType::Fire;
                        n.life = 90;
                        n.temp = 500.0f;
                    }
                }
                else if (n.type == ElementType::Water)
                {
                    cell.type = ElementType::Steam;
                    cell.life = 50;
                    return;
                }
            }

            // Fire drifts slightly upward and flickers
            const int up_y = y - 1;
            if (up_y > 0 && rng_() % 3 == 0)
            {
                const int drift_x = x + ((rng_() % 3) - 1);
                if (drift_x > 0 && drift_x < GRID_W - 1 && at(drift_x, up_y).type == ElementType::Empty)
                {
                    swap_cells(x, y, drift_x, up_y);
                }
            }
        }

        void update_gas(int x, int y)
        {
            Cell &cell = at(x, y);
            cell.turn = current_turn_;

            if (cell.life == 0)
            {
                cell.type = ElementType::Empty;
                cell.mass = 0.0f;
                return;
            }
            --cell.life;

            const int up_y = y - 1;
            if (up_y <= 0)
            {
                cell.type = ElementType::Empty;
                return;
            }

            // Rise upward
            const int dx = (rng_() % 3) - 1;
            const int target_x = std::clamp(x + dx, 1, GRID_W - 2);

            if (at(target_x, up_y).type == ElementType::Empty)
            {
                swap_cells(x, y, target_x, up_y);
            }
            else if (at(x, up_y).type == ElementType::Empty)
            {
                swap_cells(x, y, x, up_y);
            }
            else if (at(target_x, y).type == ElementType::Empty)
            {
                swap_cells(x, y, target_x, y);
            }
        }

        void update_wood(int x, int y)
        {
            Cell &cell = at(x, y);
            if (cell.temp > 220.0f && (rng_() % 8 == 0))
            {
                cell.type = ElementType::Fire;
                cell.life = 80;
            }
        }

        void swap_cells(int x1, int y1, int x2, int y2)
        {
            Cell temp = at(x1, y1);
            at(x1, y1) = at(x2, y2);
            at(x2, y2) = temp;
            at(x1, y1).turn = current_turn_;
            at(x2, y2).turn = current_turn_;
        }

        std::vector<Cell> cells_;
        std::uint32_t current_turn_ = 0;
        std::mt19937 rng_;
    };
} // namespace

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, SCREEN_W, SCREEN_H))
    {
        return -1;
    }

    // 1. Initialize Simulation Grid
    SimulationGrid grid;
    grid.load_scenario(0);

    // 2. Initialize Box2D Physics World for interacting rigid bodies
    sl::PhysicsWorld *physics = sl::create_physics_world({0.0f, 600.0f});
    std::vector<DynamicBox> boxes;

    auto spawn_box = [&](float sx, float sy, bool is_wood) {
        if (!physics) return;
        DynamicBox box;
        box.body = sl::create_physics_body(physics, sl::BodyType::dynamic_body, {sx, sy});
        box.width = is_wood ? 36.0f : 28.0f;
        box.height = is_wood ? 36.0f : 28.0f;
        box.density = is_wood ? 0.45f : 2.2f; // Wood floats on water & oil; metal sinks!
        box.is_wood = is_wood;
        box.colour = is_wood ? sl::Colour{195, 135, 75} : sl::Colour{110, 135, 165};

        if (box.body)
        {
            sl::add_box_fixture(box.body, box.width, box.height, box.density, 0.4f, 0.2f);
            boxes.push_back(box);
        }
    };

    // Spawn 2 initial wooden crates into the U-tube tank
    spawn_box(340.0f, 150.0f, true);
    spawn_box(360.0f, 80.0f, true);

    // 3. Optional Bloom Post-processing for glowing fire & sparks
    sl::Bloom bloom;
    const bool bloom_ready = bloom.initialise();
    bloom.set_threshold(0.45f);
    bloom.set_intensity(0.85f);
    bloom.set_radius(1.5f);
    bool use_bloom = bloom_ready;

    // 4. Create simulation display bitmap
    sl::Bitmap *sim_bitmap = sl::create_bitmap(GRID_W, GRID_H);
    sl::Bitmap *scene_target = sl::create_render_target(SCREEN_W, SCREEN_H);

    ElementType selected_element = ElementType::Sand;
    int brush_radius = 3;
    bool paused = false;
    int current_scenario = 0;
    bool running = true;
    sl::set_fps(60);

    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
            {
                running = false;
            }
            else if (event.type() == sl::Event::Type::key_down && !event.key_repeat())
            {
                switch (event.key())
                {
                case sl::Event::Key::digit_1: selected_element = ElementType::Sand; break;
                case sl::Event::Key::digit_2: selected_element = ElementType::Water; break;
                case sl::Event::Key::digit_3: selected_element = ElementType::Oil; break;
                case sl::Event::Key::digit_4: selected_element = ElementType::Fire; break;
                case sl::Event::Key::digit_5: selected_element = ElementType::Wood; break;
                case sl::Event::Key::digit_6: selected_element = ElementType::Solid; break;
                case sl::Event::Key::digit_7: selected_element = ElementType::Acid; break;
                case sl::Event::Key::digit_8: selected_element = ElementType::Smoke; break;
                case sl::Event::Key::space: paused = !paused; break;
                case sl::Event::Key::letter_r: grid.clear(); break;
                case sl::Event::Key::letter_p:
                    ++current_scenario;
                    grid.load_scenario(current_scenario);
                    break;
                case sl::Event::Key::letter_c:
                    spawn_box(static_cast<float>(sl::mouse_x()), static_cast<float>(sl::mouse_y()), true);
                    break;
                case sl::Event::Key::letter_v:
                    spawn_box(static_cast<float>(sl::mouse_x()), static_cast<float>(sl::mouse_y()), false);
                    break;
                case sl::Event::Key::letter_b:
                    if (bloom_ready) use_bloom = !use_bloom;
                    break;
                case sl::Event::Key::left_bracket:
                    brush_radius = std::max(1, brush_radius - 1);
                    break;
                case sl::Event::Key::right_bracket:
                    brush_radius = std::min(10, brush_radius + 1);
                    break;
                default:
                    break;
                }
            }
            sl::display_handle_event(event);
        }

        // --- Mouse Painting ---
        const int mx = sl::mouse_x();
        const int my = sl::mouse_y();
        const int gx = mx / static_cast<int>(CELL_SCALE);
        const int gy = my / static_cast<int>(CELL_SCALE);
        const std::uint32_t buttons = sl::mouse_buttons();

        if (buttons & 1) // Left click: Paint selected element
        {
            grid.paint(gx, gy, selected_element, brush_radius);
        }
        else if (buttons & 4) // Right click: Erase / Vacuum
        {
            grid.paint(gx, gy, ElementType::Empty, brush_radius + 1);
        }

        // --- Physics & Simulation Step ---
        const float dt = std::min(0.033f, static_cast<float>(sl::get_frame_time()) / 1000.0f);
        if (!paused)
        {
            // 1. Advance Cellular Automata
            grid.update();

            // 2. Advance Box2D rigid bodies with fluid buoyancy & drag
            if (physics)
            {
                for (DynamicBox &box : boxes)
                {
                    if (!box.body) continue;
                    const sl::Vec2 pos = sl::physics_body_position(box.body);
                    const sl::Vec2 vel = sl::physics_body_velocity(box.body);

                    // Sample fluid cells overlapping the box to compute buoyancy
                    const int min_gx = std::clamp(static_cast<int>((pos.x - box.width * 0.5f) / CELL_SCALE), 1, GRID_W - 2);
                    const int max_gx = std::clamp(static_cast<int>((pos.x + box.width * 0.5f) / CELL_SCALE), 1, GRID_W - 2);
                    const int min_gy = std::clamp(static_cast<int>((pos.y - box.height * 0.5f) / CELL_SCALE), 1, GRID_H - 2);
                    const int max_gy = std::clamp(static_cast<int>((pos.y + box.height * 0.5f) / CELL_SCALE), 1, GRID_H - 2);

                    int submerged_cells = 0;
                    int total_cells = 0;
                    float fluid_density_sum = 0.0f;
                    bool in_fire = false;

                    for (int cy = min_gy; cy <= max_gy; ++cy)
                    {
                        for (int cx = min_gx; cx <= max_gx; ++cx)
                        {
                            ++total_cells;
                            const Cell &c = grid.get(cx, cy);
                            if (c.type == ElementType::Water || c.type == ElementType::Oil || c.type == ElementType::Acid)
                            {
                                ++submerged_cells;
                                fluid_density_sum += ELEMENT_PROPERTIES[static_cast<std::size_t>(c.type)].density;

                                // Fluid displacement: moving box pushes water sideways
                                if (std::abs(vel.x) > 20.0f && (grid.current_turn() % 3 == 0))
                                {
                                    const int push_dir = vel.x > 0.0f ? 1 : -1;
                                    if (grid.get(cx + push_dir, cy).type == ElementType::Empty)
                                    {
                                        grid.set(cx + push_dir, cy, c.type, c.mass);
                                    }
                                }
                            }
                            else if (c.type == ElementType::Fire)
                            {
                                in_fire = true;
                            }
                        }
                    }

                    if (total_cells > 0 && submerged_cells > 0)
                    {
                        const float submerged_ratio = static_cast<float>(submerged_cells) / static_cast<float>(total_cells);
                        const float avg_fluid_density = fluid_density_sum / static_cast<float>(submerged_cells);

                        // Archimedes Buoyancy Force: F = fluid_density * submerged_volume * gravity
                        const float mass_approx = box.width * box.height * box.density;
                        const float buoyancy = (avg_fluid_density / box.density) * mass_approx * 750.0f * submerged_ratio;
                        sl::apply_physics_force(box.body, {0.0f, -buoyancy});

                        // Fluid Drag / Damping
                        const float drag_x = -vel.x * 0.08f * submerged_ratio * mass_approx;
                        const float drag_y = -vel.y * 0.12f * submerged_ratio * mass_approx;
                        sl::apply_physics_force(box.body, {drag_x, drag_y});
                    }

                    // Flammable box catch fire
                    if (in_fire && box.is_wood)
                    {
                        box.colour = {230, 90, 40}; // Burning orange
                        if (std::rand() % 4 == 0)
                        {
                            grid.set(min_gx + std::rand() % std::max(1, max_gx - min_gx),
                                     std::max(1, min_gy - 1), ElementType::Fire, 0.0f, 600.0f, 40);
                        }
                    }
                }

                sl::step_physics_world(physics, dt);
            }
        }

        // --- Render Frame ---
        grid.render_to_bitmap(sim_bitmap);

        if (scene_target) sl::begin_render_target(scene_target);
        sl::clear_to_colour(sl::screen, {16, 20, 28});

        // 1. Draw Cellular Automata fluid/sand canvas stretched to full window
        sl::draw_sprite_stretched(sim_bitmap, 0.0f, 0.0f, SCREEN_W, SCREEN_H);

        // 2. Draw Box2D Rigid Bodies
        for (const DynamicBox &box : boxes)
        {
            if (!box.body) continue;
            const sl::Vec2 pos = sl::physics_body_position(box.body);
            const float angle = sl::physics_body_angle(box.body);
            const float half_w = box.width * 0.5f;
            const float half_h = box.height * 0.5f;

            const float cos_a = std::cos(angle);
            const float sin_a = std::sin(angle);
            auto transform = [&](float lx, float ly) {
                return sl::Vec2{pos.x + lx * cos_a - ly * sin_a, pos.y + lx * sin_a + ly * cos_a};
            };

            const sl::Vec2 p1 = transform(-half_w, -half_h);
            const sl::Vec2 p2 = transform(half_w, -half_h);
            const sl::Vec2 p3 = transform(half_w, half_h);
            const sl::Vec2 p4 = transform(-half_w, half_h);

            sl::trianglefill(sl::screen, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, box.colour);
            sl::trianglefill(sl::screen, p1.x, p1.y, p3.x, p3.y, p4.x, p4.y, box.colour);
            sl::line(sl::screen, p1.x, p1.y, p2.x, p2.y, {240, 245, 255});
            sl::line(sl::screen, p2.x, p2.y, p3.x, p3.y, {240, 245, 255});
            sl::line(sl::screen, p3.x, p3.y, p4.x, p4.y, {240, 245, 255});
            sl::line(sl::screen, p4.x, p4.y, p1.x, p1.y, {240, 245, 255});
        }

        // 3. Draw Brush Cursor
        if (mx > 0 && mx < SCREEN_W && my > 0 && my < SCREEN_H)
        {
            sl::circle(sl::screen, static_cast<float>(mx), static_cast<float>(my),
                       brush_radius * CELL_SCALE, {255, 255, 255, 160});
        }

        if (scene_target) sl::end_render_target();

        // 4. Output / Post-processing pass
        sl::clear_to_colour(sl::screen, {16, 20, 28});
        if (use_bloom && scene_target)
        {
            bloom.apply(scene_target, 0, 0, SCREEN_W, SCREEN_H);
        }
        else if (scene_target)
        {
            sl::draw_sprite(scene_target, 0.0f, 0.0f);
        }

        // 5. Draw HUD Overlay directly on screen for razor-sharp typography
        sl::gprintf(16, 16, {255, 235, 140}, "CA Fluid & Falling Sand (Tom Forsyth CA Model)");
        sl::gprintf(16, 38, {170, 190, 215},
                    "Selected: [ %s ] (Keys 1-8)   Brush Radius: %d ([ / ])   Bloom: %s (B)",
                    ELEMENT_PROPERTIES[static_cast<std::size_t>(selected_element)].name,
                    brush_radius, use_bloom ? "ON" : "OFF");
        sl::gprintf(16, 58, {150, 175, 200},
                    "Left Click: Draw | Right Click: Erase | C: Drop Wood Crate | V: Metal Barrel | P: Scenario (%d)",
                    current_scenario % 4 + 1);
        sl::gprintf(16, 78, {130, 155, 180},
                    "Sand: %d  Water: %d  Oil: %d  Fire: %d | Space: %s | R: Reset",
                    grid.count_elements(ElementType::Sand),
                    grid.count_elements(ElementType::Water),
                    grid.count_elements(ElementType::Oil),
                    grid.count_elements(ElementType::Fire),
                    paused ? "PAUSED" : "RUNNING");

        sl::show_video_bitmap();
        sl::end_frame();
    }

    // Cleanup
    sl::wait_for_graphics();
    bloom.shutdown();
    if (physics)
    {
        for (DynamicBox &box : boxes)
        {
            if (box.body) sl::destroy_physics_body(box.body);
        }
        sl::destroy_physics_world(physics);
    }
    sl::destroy_bitmap(sim_bitmap);
    sl::destroy_bitmap(scene_target);
    sl::shutdown();
    return 0;
}

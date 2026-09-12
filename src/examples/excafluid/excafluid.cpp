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
        Solid,       // Indestructible bedrock / wall
        Wood,        // Flammable building material
        Sand,        // Granular falling powder
        Water,       // Incompressible / slightly compressible liquid (density ~1.0)
        Oil,         // Flammable lighter liquid (density ~0.65, floats on water)
        Acid,        // Corrosive liquid, dissolves organic matter & metal
        Lava,        // Molten rock at 1000C, solidifies with water into stone
        Gunpowder,   // Explosive granular powder, detonates with fire/heat
        Plant,       // Organic plant, grows with water, burns quickly
        Pump,        // Active mechanical fan/pump that propels fluids upward
        Fire,        // Active flame emitting heat, ignites fuel
        Smoke,       // Rising gas, dissipates
        Steam        // Evaporated water from fire/lava
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
        {"Empty",     0.0f,  false, false, false, false, 9999.0f, {16, 20, 28, 255}},
        {"Solid",     999.0f,false, false, true,  false, 9999.0f, {120, 128, 142, 255}},
        {"Wood",      0.7f,  false, false, true,  true,  220.0f,  {142, 92, 54, 255}},
        {"Sand",      1.8f,  false, false, false, false, 9999.0f, {235, 195, 95, 255}},
        {"Water",     1.0f,  true,  false, false, false, 9999.0f, {45, 130, 240, 235}},
        {"Oil",       0.65f, true,  false, false, true,  75.0f,   {130, 95, 35, 245}},
        {"Acid",      1.25f, true,  false, false, false, 9999.0f, {80, 240, 65, 240}},
        {"Lava",      2.4f,  true,  false, false, false, 9999.0f, {255, 80, 20, 255}},
        {"Gunpowder", 1.6f,  false, false, false, true,  240.0f,  {70, 75, 82, 255}},
        {"Plant",     0.5f,  false, false, true,  true,  160.0f,  {45, 175, 60, 255}},
        {"Pump",      999.0f,false, false, true,  false, 9999.0f, {185, 195, 215, 255}},
        {"Fire",     -0.2f,  false, true,  false, false, 0.0f,    {255, 120, 30, 255}},
        {"Smoke",    -0.1f,  false, true,  false, false, 9999.0f, {90, 95, 105, 180}},
        {"Steam",    -0.15f, false, true,  false, false, 9999.0f, {190, 210, 235, 170}}
    };

    struct Cell
    {
        ElementType type = ElementType::Empty;
        float mass = 0.0f;       // Fluid mass/pressure: 0.0 - 1.5 (Tom Forsyth liquid model)
        float temp = 20.0f;      // Temperature in Celsius (ambient = 20C)
        std::uint8_t life = 0;   // Lifetime timer for dynamic particles (fire, smoke, steam, plant growth)
        std::int8_t variation = 0; // Visual noise/shade variation (-15 .. +15)
        std::uint32_t turn = 0;  // Update turn tracking to avoid updating twice per tick
    };

    enum class BoxKind
    {
        Wood,
        Metal,
        TNT,
        Seesaw
    };

    struct DynamicBox
    {
        sl::PhysicsBody *body = nullptr;
        float width = 32.0f;
        float height = 32.0f;
        float density = 0.5f;
        BoxKind kind = BoxKind::Wood;
        bool exploded = false;
        sl::Colour colour{185, 125, 75};
    };

    class SimulationGrid
    {
    public:
        SimulationGrid()
        {
            cells_.resize(GRID_W * GRID_H);
            simulation_ = sl::create_fluid_simulation(GRID_W, GRID_H);
            rng_.seed(1337);
            init_bounds();
        }

        ~SimulationGrid()
        {
            if (simulation_)
            {
                sl::destroy_fluid_simulation(simulation_);
                simulation_ = nullptr;
            }
        }

        void clear()
        {
            if (!simulation_) return;
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
            sl::fluid_clear(simulation_);
            init_bounds();
        }

        void init_bounds()
        {
            if (!simulation_) return;
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

        void load_scenario(int index, std::vector<DynamicBox> &boxes, sl::PhysicsWorld *physics)
        {
            clear();
            // Clear existing Box2D dynamic objects and static scenario bodies
            if (physics)
            {
                for (DynamicBox &b : boxes)
                {
                    if (b.body) sl::destroy_physics_body(b.body);
                }
                boxes.clear();

                for (sl::PhysicsBody *sb : static_bodies_)
                {
                    if (sb) sl::destroy_physics_body(sb);
                }
                static_bodies_.clear();
            }

            auto add_static_box = [&](float cx, float cy, float w, float h) {
                if (!physics) return;
                sl::PhysicsBody *body = sl::create_physics_body(physics, sl::BodyType::static_body, {cx, cy});
                if (body)
                {
                    sl::add_box_fixture(body, w, h, 0.0f, 0.5f, 0.1f);
                    static_bodies_.push_back(body);
                }
            };

            // Outer boundary collision walls
            add_static_box(400.0f, 596.0f, 800.0f, 16.0f); // Floor
            add_static_box(4.0f, 300.0f, 16.0f, 600.0f);   // Left wall
            add_static_box(796.0f, 300.0f, 16.0f, 600.0f); // Right wall
            add_static_box(400.0f, 4.0f, 800.0f, 16.0f);   // Ceiling

            auto spawn_box = [&](float sx, float sy, BoxKind kind) {
                if (!physics) return;
                DynamicBox box;
                box.kind = kind;
                if (kind == BoxKind::Wood)
                {
                    box.width = 36.0f;
                    box.height = 36.0f;
                    box.density = 0.45f;
                    box.colour = {195, 135, 75};
                }
                else if (kind == BoxKind::Metal)
                {
                    box.width = 28.0f;
                    box.height = 28.0f;
                    box.density = 2.4f;
                    box.colour = {110, 135, 165};
                }
                else if (kind == BoxKind::TNT)
                {
                    box.width = 32.0f;
                    box.height = 32.0f;
                    box.density = 0.8f;
                    box.colour = {230, 45, 45};
                }
                else if (kind == BoxKind::Seesaw)
                {
                    box.width = 160.0f;
                    box.height = 14.0f;
                    box.density = 1.0f;
                    box.colour = {200, 165, 110};
                }

                box.body = sl::create_physics_body(physics, sl::BodyType::dynamic_body, {sx, sy});
                if (box.body)
                {
                    sl::add_box_fixture(box.body, box.width, box.height, box.density, 0.4f, 0.2f);
                    boxes.push_back(box);
                }
            };

            switch (index % 5)
            {
            case 0: // Preset 0: U-Tube Hydraulic Equalization & Floating Crates (Forsyth compressible water demo)
            {
                for (int y = 30; y < 120; ++y)
                {
                    set(70, y, ElementType::Solid);
                    set(130, y, ElementType::Solid);
                }
                for (int y = 30; y < 105; ++y)
                {
                    set(100, y, ElementType::Solid);
                }
                for (int y = 40; y < 118; ++y)
                {
                    for (int x = 72; x < 98; ++x)
                        set(x, y, ElementType::Water, 1.0f);
                }
                for (int y = 36; y < 40; ++y)
                {
                    for (int x = 72; x < 98; ++x)
                        set(x, y, ElementType::Oil, 0.8f);
                }

                // Scenario static collision boundaries
                add_static_box(280.0f, 300.0f, 8.0f, 360.0f);
                add_static_box(520.0f, 300.0f, 8.0f, 360.0f);
                add_static_box(400.0f, 270.0f, 8.0f, 300.0f);

                spawn_box(340.0f, 150.0f, BoxKind::Wood);
                spawn_box(360.0f, 80.0f, BoxKind::Wood);
                break;
            }
            case 1: // Preset 1: Volcano & Steam Geysers (Lava meets Water)
            {
                // Volcanic mountain crater
                for (int i = 0; i < 50; ++i)
                {
                    set(20 + i, 80 - i / 2, ElementType::Solid);
                    set(120 - i, 80 - i / 2, ElementType::Solid);
                }
                // Molten lava pool inside volcano
                for (int y = 58; y < 75; ++y)
                {
                    for (int x = 40; x < 100; ++x)
                        set(x, y, ElementType::Lava, 1.0f, 1000.0f);
                }
                // Water lake next to the volcano
                for (int y = 80; y < 140; ++y)
                {
                    for (int x = 125; x < 185; ++x)
                        set(x, y, ElementType::Water, 1.0f);
                }
                // Wooden bridge over the lake
                for (int x = 115; x < 190; ++x)
                {
                    set(x, 78, ElementType::Wood);
                    set(x, 79, ElementType::Wood);
                }
                // Growing plants on the slopes
                for (int x = 122; x < 145; ++x) set(x, 75, ElementType::Plant);

                add_static_box(610.0f, 560.0f, 260.0f, 20.0f);
                spawn_box(600.0f, 260.0f, BoxKind::Wood);
                spawn_box(300.0f, 180.0f, BoxKind::Metal);
                break;
            }
            case 2: // Preset 2: Bomb Testing Ground & TNT Demolition
            {
                // Wooden fortress / towers
                for (int y = 60; y < 135; ++y)
                {
                    set(50, y, ElementType::Wood);
                    set(80, y, ElementType::Wood);
                    set(120, y, ElementType::Wood);
                    set(150, y, ElementType::Wood);
                }
                for (int x = 50; x <= 80; ++x) set(x, 60, ElementType::Wood);
                for (int x = 120; x <= 150; ++x) set(x, 60, ElementType::Wood);
                for (int x = 80; x <= 120; ++x) set(x, 95, ElementType::Wood);

                // Gunpowder caches inside the towers
                for (int y = 100; y < 135; ++y)
                {
                    for (int x = 52; x < 78; ++x) set(x, y, ElementType::Gunpowder);
                    for (int x = 122; x < 148; ++x) set(x, y, ElementType::Gunpowder);
                }

                // Oil reservoir on middle deck
                for (int y = 85; y < 95; ++y)
                {
                    for (int x = 85; x < 115; ++x) set(x, y, ElementType::Oil, 1.0f);
                }

                add_static_box(260.0f, 560.0f, 180.0f, 20.0f);
                add_static_box(540.0f, 560.0f, 180.0f, 20.0f);

                // TNT crates on top of structures
                spawn_box(260.0f, 180.0f, BoxKind::TNT);
                spawn_box(540.0f, 180.0f, BoxKind::TNT);
                spawn_box(400.0f, 320.0f, BoxKind::TNT);
                break;
            }
            case 3: // Preset 3: Hydroelectric Pump & Seesaw Balance
            {
                // Funnel hopper
                for (int i = 0; i < 35; ++i)
                {
                    set(65 + i, 30 + i, ElementType::Solid);
                    set(135 - i, 30 + i, ElementType::Solid);
                }
                // Water in upper funnel
                for (int y = 15; y < 45; ++y)
                {
                    for (int x = 75; x < 125; ++x)
                        set(x, y, ElementType::Water, 1.0f);
                }

                // Static fulcrum wedge for seesaw
                for (int i = 0; i < 8; ++i)
                {
                    for (int x = 96 - i; x <= 104 + i; ++x)
                        set(x, 95 + i, ElementType::Solid);
                }

                // Catch basin below
                for (int x = 30; x < 170; ++x) set(x, 140, ElementType::Solid);
                for (int y = 115; y < 140; ++y)
                {
                    set(30, y, ElementType::Solid);
                    set(170, y, ElementType::Solid);
                }

                // Active Pumps at the bottom basin that spray fluid up pipes to the top
                for (int x = 32; x < 38; ++x) set(x, 138, ElementType::Pump);
                for (int x = 162; x < 168; ++x) set(x, 138, ElementType::Pump);

                // Pipes leading from pumps back to top
                for (int y = 15; y < 138; ++y)
                {
                    set(28, y, ElementType::Solid);
                    set(40, y, ElementType::Solid);
                    set(160, y, ElementType::Solid);
                    set(172, y, ElementType::Solid);
                }

                // Fulcrum wedge physics fixture
                if (physics)
                {
                    sl::PhysicsBody *fulcrum = sl::create_physics_body(physics, sl::BodyType::static_body, {400.0f, 395.0f});
                    if (fulcrum)
                    {
                        const std::vector<sl::Vec2> wedge_pts = {{-24.0f, 20.0f}, {24.0f, 20.0f}, {0.0f, -20.0f}};
                        sl::add_polygon_fixture(fulcrum, wedge_pts, 0.0f, 0.6f, 0.0f);
                        static_bodies_.push_back(fulcrum);
                    }
                }
                add_static_box(400.0f, 560.0f, 560.0f, 16.0f);

                // Dynamic Seesaw plank
                spawn_box(400.0f, 360.0f, BoxKind::Seesaw);
                spawn_box(330.0f, 320.0f, BoxKind::Wood);
                spawn_box(470.0f, 320.0f, BoxKind::Metal);
                break;
            }
            case 4: // Preset 4: Overgrown Jungle & Corrosive Acid Rain
            {
                // Tiered platforms
                for (int x = 20; x < 90; ++x) set(x, 70, ElementType::Wood);
                for (int x = 110; x < 180; ++x) set(x, 90, ElementType::Wood);
                for (int x = 40; x < 160; ++x) set(x, 125, ElementType::Wood);

                // Lush vegetation everywhere
                for (int x = 22; x < 88; ++x) set(x, 68, ElementType::Plant);
                for (int x = 112; x < 178; ++x) set(x, 88, ElementType::Plant);
                for (int x = 42; x < 158; ++x) set(x, 123, ElementType::Plant);

                // Water reservoirs feeding the plants
                for (int y = 40; y < 55; ++y)
                {
                    for (int x = 30; x < 60; ++x) set(x, y, ElementType::Water, 1.0f);
                }

                // Acid tank high above ready to melt through
                for (int y = 15; y < 28; ++y)
                {
                    for (int x = 120; x < 160; ++x) set(x, y, ElementType::Acid, 1.0f);
                }

                add_static_box(220.0f, 280.0f, 280.0f, 12.0f);
                add_static_box(580.0f, 360.0f, 280.0f, 12.0f);

                spawn_box(240.0f, 220.0f, BoxKind::Wood);
                spawn_box(520.0f, 300.0f, BoxKind::Wood);
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
            if (type == ElementType::Lava) c.temp = 1000.0f;
            c.life = life > 0 ? life : (type == ElementType::Fire ? 50 : (type == ElementType::Smoke || type == ElementType::Steam ? 60 : 0));
            c.variation = static_cast<std::int8_t>((rng_() % 21) - 10);
            c.turn = current_turn_;

            // Sync to engine
            if (simulation_)
            {
                sl::fluid_set_cell(simulation_, x, y, static_cast<sl::FluidElement>(type), c.mass, c.temp, c.life, c.variation);
            }
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

        void trigger_explosion(int cx, int cy, int blast_radius, sl::PhysicsWorld *physics)
        {
            // Tom Forsyth's explosive air pressure model:
            // High sudden pressure/heat burst, line-of-sight propagation, destroys weak materials,
            // and applies radial physics impulses to Box2D rigid bodies
            for (int dy = -blast_radius; dy <= blast_radius; ++dy)
            {
                for (int dx = -blast_radius; dx <= blast_radius; ++dx)
                {
                    const float dist_sq = static_cast<float>(dx * dx + dy * dy);
                    if (dist_sq <= static_cast<float>(blast_radius * blast_radius))
                    {
                        int x = cx + dx;
                        int y = cy + dy;
                        if (x > 0 && x < GRID_W - 1 && y > 0 && y < GRID_H - 1)
                        {
                            const Cell &c = get(x, y);
                            if (c.type != ElementType::Solid)
                            {
                                if (dist_sq < static_cast<float>(blast_radius * blast_radius * 0.4f))
                                {
                                    set(x, y, ElementType::Fire, 0.0f, 1200.0f, 80);
                                }
                                else if (rng_() % 2 == 0)
                                {
                                    set(x, y, ElementType::Smoke, 0.0f, 400.0f, 60);
                                }
                                else
                                {
                                    set(x, y, ElementType::Empty, 0.0f, 20.0f, 0);
                                }
                            }
                        }
                    }
                }
            }

            // Radial impulse to Box2D rigid bodies
            if (physics)
            {
                const float blast_world_x = static_cast<float>(cx) * CELL_SCALE;
                const float blast_world_y = static_cast<float>(cy) * CELL_SCALE;
                const float blast_range_px = static_cast<float>(blast_radius) * CELL_SCALE * 2.0f;

                last_blast_x_ = blast_world_x;
                last_blast_y_ = blast_world_y;
                last_blast_force_ = 350.0f;
                last_blast_range_ = blast_range_px;
                blast_pending_ = true;
            }
        }

        void apply_pending_blast(std::vector<DynamicBox> &boxes)
        {
            if (!blast_pending_) return;
            blast_pending_ = false;

            for (DynamicBox &box : boxes)
            {
                if (!box.body) continue;
                const sl::Vec2 pos = sl::physics_body_position(box.body);
                const float dx = pos.x - last_blast_x_;
                const float dy = pos.y - last_blast_y_;
                const float dist = std::sqrt(dx * dx + dy * dy);

                if (dist > 0.001f && dist < last_blast_range_)
                {
                    const float factor = (1.0f - dist / last_blast_range_);
                    const float mass_px = (box.width * box.height * box.density) / 4096.0f;
                    const float impulse = last_blast_force_ * factor * mass_px * 600.0f;
                    sl::apply_physics_force(box.body, {dx / dist * impulse, dy / dist * impulse});

                    if (box.kind == BoxKind::TNT && !box.exploded && dist < last_blast_range_ * 0.6f)
                    {
                        box.exploded = true;
                    }
                }
            }
        }

        void update(sl::PhysicsWorld *physics)
        {
            if (!simulation_) return;

            // Run the core CA simulation from the engine
            sl::fluid_step(simulation_);

            // Sync engine state to local buffer for rendering only
            sync_from_engine();

            current_turn_ = sl::fluid_turn(simulation_);
        }

        void sync_from_engine()
        {
            if (!simulation_) return;
            for (int y = 0; y < GRID_H; ++y)
            {
                for (int x = 0; x < GRID_W; ++x)
                {
                    const sl::FluidCell &fc = sl::fluid_get_cell(simulation_, x, y);
                    Cell &c = at(x, y);
                    c.type = static_cast<ElementType>(fc.type);
                    c.mass = fc.mass;
                    c.temp = fc.temp;
                    c.life = fc.life;
                    c.variation = fc.variation;
                    c.turn = fc.turn;
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

                    if (cell.type == ElementType::Water)
                    {
                        const int depth_tint = std::clamp(static_cast<int>((cell.mass - 0.5f) * 40.0f), -20, 40);
                        c.red = static_cast<std::uint8_t>(std::clamp(c.red - depth_tint, 10, 255));
                        c.green = static_cast<std::uint8_t>(std::clamp(c.green + depth_tint / 2, 50, 255));
                        c.blue = static_cast<std::uint8_t>(std::clamp(c.blue + cell.variation, 180, 255));
                    }
                    else if (cell.type == ElementType::Lava)
                    {
                        const float heat_wave = std::sin(static_cast<float>(x + current_turn_) * 0.15f);
                        c.red = 255;
                        c.green = static_cast<std::uint8_t>(std::clamp(static_cast<int>(70.0f + heat_wave * 35.0f) + cell.variation * 2, 40, 255));
                        c.blue = static_cast<std::uint8_t>(std::clamp(static_cast<int>(15.0f + heat_wave * 15.0f), 0, 100));
                    }
                    else if (cell.type == ElementType::Fire)
                    {
                        const float heat_ratio = std::clamp(static_cast<float>(cell.life) / 50.0f, 0.0f, 1.0f);
                        c.red = 255;
                        c.green = static_cast<std::uint8_t>(std::clamp(static_cast<int>(60.0f + heat_ratio * 180.0f) + cell.variation * 3, 40, 255));
                        c.blue = static_cast<std::uint8_t>(std::clamp(static_cast<int>(heat_ratio * 90.0f), 0, 255));
                    }
                    else if (cell.type == ElementType::Plant)
                    {
                        c.green = static_cast<std::uint8_t>(std::clamp(static_cast<int>(c.green) + cell.variation * 3, 100, 255));
                    }
                    else if (cell.type == ElementType::Pump)
                    {
                        const bool stripe = ((y + current_turn_ / 2) % 6 < 3);
                        c = stripe ? sl::Colour{220, 230, 250, 255} : sl::Colour{90, 100, 120, 255};
                    }
                    else if (cell.type == ElementType::Smoke)
                    {
                        const float alpha_fade = std::clamp(static_cast<float>(cell.life) / 60.0f, 0.2f, 1.0f);
                        c.red = static_cast<std::uint8_t>(c.red * alpha_fade);
                        c.green = static_cast<std::uint8_t>(c.green * alpha_fade);
                        c.blue = static_cast<std::uint8_t>(c.blue * alpha_fade);
                    }
                    else if (cell.type == ElementType::Empty)
                    {
                        c.red = 16 + y / 18;
                        c.green = 20 + y / 16;
                        c.blue = 30 + y / 12;
                    }
                    else
                    {
                        c.red = static_cast<std::uint8_t>(std::clamp(static_cast<int>(c.red) + cell.variation * 2, 0, 255));
                        c.green = static_cast<std::uint8_t>(std::clamp(static_cast<int>(c.green) + cell.variation * 2, 0, 255));
                        c.blue = static_cast<std::uint8_t>(std::clamp(static_cast<int>(c.blue) + cell.variation, 0, 255));
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
        std::vector<Cell> cells_;
        sl::FluidSimulation *simulation_ = nullptr;
        std::uint32_t current_turn_ = 0;
        std::mt19937 rng_;
        std::vector<sl::PhysicsBody *> static_bodies_;

        // Physics explosion impulse feedback
        bool blast_pending_ = false;
        float last_blast_x_ = 0.0f;
        float last_blast_y_ = 0.0f;
        float last_blast_force_ = 0.0f;
        float last_blast_range_ = 0.0f;
    };
} // namespace

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, SCREEN_W, SCREEN_H))
    {
        return -1;
    }

    // 1. Initialize Box2D Physics World
    sl::PhysicsWorld *physics = sl::create_physics_world({0.0f, 600.0f});
    std::vector<DynamicBox> boxes;

    // 2. Initialize Simulation Grid & load first scenario
    SimulationGrid grid;
    grid.load_scenario(0, boxes, physics);

    auto spawn_box = [&](float sx, float sy, BoxKind kind) {
        if (!physics) return;
        DynamicBox box;
        box.kind = kind;
        if (kind == BoxKind::Wood)
        {
            box.width = 36.0f;
            box.height = 36.0f;
            box.density = 0.45f;
            box.colour = {195, 135, 75};
        }
        else if (kind == BoxKind::Metal)
        {
            box.width = 28.0f;
            box.height = 28.0f;
            box.density = 2.4f;
            box.colour = {110, 135, 165};
        }
        else if (kind == BoxKind::TNT)
        {
            box.width = 32.0f;
            box.height = 32.0f;
            box.density = 0.8f;
            box.colour = {230, 45, 45};
        }
        else if (kind == BoxKind::Seesaw)
        {
            box.width = 160.0f;
            box.height = 14.0f;
            box.density = 1.0f;
            box.colour = {200, 165, 110};
        }

        box.body = sl::create_physics_body(physics, sl::BodyType::dynamic_body, {sx, sy});
        if (box.body)
        {
            sl::add_box_fixture(box.body, box.width, box.height, box.density, 0.4f, 0.2f);
            boxes.push_back(box);
        }
    };

    // 3. Post-processing: Bloom for glowing fire/lava/acid
    sl::Bloom bloom;
    const bool bloom_ready = bloom.initialise();
    bloom.set_threshold(0.40f);
    bloom.set_intensity(0.95f);
    bloom.set_radius(1.8f);
    bool use_bloom = bloom_ready;

    // 4. Dynamic 2D Lighting pass: Spotlight Torch & glowing fire point lights!
    sl::LightingPass lighting;
    const bool lighting_ready = lighting.initialise();
    lighting.set_ambient(0.18f);
    bool use_lighting = false;

    // 5. Create display bitmaps
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
                case sl::Event::Key::digit_8: selected_element = ElementType::Lava; break;
                case sl::Event::Key::digit_9: selected_element = ElementType::Gunpowder; break;
                case sl::Event::Key::digit_0: selected_element = ElementType::Plant; break;
                case sl::Event::Key::minus:   selected_element = ElementType::Pump; break;
                case sl::Event::Key::space: paused = !paused; break;
                case sl::Event::Key::letter_r: grid.clear(); break;
                case sl::Event::Key::letter_p:
                    ++current_scenario;
                    grid.load_scenario(current_scenario, boxes, physics);
                    break;
                case sl::Event::Key::letter_c:
                    spawn_box(static_cast<float>(sl::mouse_x()), static_cast<float>(sl::mouse_y()), BoxKind::Wood);
                    break;
                case sl::Event::Key::letter_v:
                    spawn_box(static_cast<float>(sl::mouse_x()), static_cast<float>(sl::mouse_y()), BoxKind::Metal);
                    break;
                case sl::Event::Key::letter_t:
                    spawn_box(static_cast<float>(sl::mouse_x()), static_cast<float>(sl::mouse_y()), BoxKind::TNT);
                    break;
                case sl::Event::Key::letter_b:
                    if (bloom_ready) use_bloom = !use_bloom;
                    break;
                case sl::Event::Key::letter_l:
                    if (lighting_ready) use_lighting = !use_lighting;
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
            grid.update(physics);

            // 2. Advance Box2D rigid bodies with fluid buoyancy & drag
            if (physics)
            {
                for (std::size_t bi = 0; bi < boxes.size(); ++bi)
                {
                    DynamicBox &box = boxes[bi];
                    if (!box.body) continue;
                    const sl::Vec2 pos = sl::physics_body_position(box.body);
                    const sl::Vec2 vel = sl::physics_body_velocity(box.body);

                    // Check for TNT detonation
                    if (box.kind == BoxKind::TNT && box.exploded)
                    {
                        grid.trigger_explosion(static_cast<int>(pos.x / CELL_SCALE),
                                               static_cast<int>(pos.y / CELL_SCALE), 20, physics);
                        sl::destroy_physics_body(box.body);
                        box.body = nullptr;
                        continue;
                    }

                    // Sample fluid cells overlapping the box to compute buoyancy
                    const int min_gx = std::clamp(static_cast<int>((pos.x - box.width * 0.5f) / CELL_SCALE), 1, GRID_W - 2);
                    const int max_gx = std::clamp(static_cast<int>((pos.x + box.width * 0.5f) / CELL_SCALE), 1, GRID_W - 2);
                    const int min_gy = std::clamp(static_cast<int>((pos.y - box.height * 0.5f) / CELL_SCALE), 1, GRID_H - 2);
                    const int max_gy = std::clamp(static_cast<int>((pos.y + box.height * 0.5f) / CELL_SCALE), 1, GRID_H - 2);

                    int submerged_cells = 0;
                    int total_cells = 0;
                    float fluid_density_sum = 0.0f;
                    bool in_fire = false;
                    bool in_lava = false;

                    for (int cy = min_gy; cy <= max_gy; ++cy)
                    {
                        for (int cx = min_gx; cx <= max_gx; ++cx)
                        {
                            ++total_cells;
                            const Cell &c = grid.get(cx, cy);
                            if (c.type == ElementType::Water || c.type == ElementType::Oil ||
                                c.type == ElementType::Acid || c.type == ElementType::Lava)
                            {
                                ++submerged_cells;
                                fluid_density_sum += ELEMENT_PROPERTIES[static_cast<std::size_t>(c.type)].density;

                                if (std::abs(vel.x) > 20.0f && (grid.current_turn() % 3 == 0))
                                {
                                    const int push_dir = vel.x > 0.0f ? 1 : -1;
                                    if (grid.get(cx + push_dir, cy).type == ElementType::Empty)
                                    {
                                        grid.set(cx + push_dir, cy, c.type, c.mass);
                                    }
                                }

                                if (c.type == ElementType::Lava) in_lava = true;
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

                        // Archimedes Buoyancy Force in pixel force units:
                        // Accurately balanced with Box2D gravity (600.0 px/s^2)
                        const float buoyancy = submerged_ratio * avg_fluid_density * (box.width * box.height * 600.0f) / 4096.0f;
                        sl::apply_physics_force(box.body, {0.0f, -buoyancy});

                        // Fluid Drag / Damping:
                        const float mass_px = (box.width * box.height * box.density) / 4096.0f;
                        const float drag_x = -vel.x * 2.5f * submerged_ratio * mass_px;
                        const float drag_y = -vel.y * 3.5f * submerged_ratio * mass_px;
                        sl::apply_physics_force(box.body, {drag_x, drag_y});
                    }

                    // Ignitions
                    if (in_fire || in_lava)
                    {
                        if (box.kind == BoxKind::TNT)
                        {
                            box.exploded = true;
                        }
                        else if (box.kind == BoxKind::Wood)
                        {
                            box.colour = {230, 90, 40};
                            if (std::rand() % 4 == 0)
                            {
                                grid.set(min_gx + std::rand() % std::max(1, max_gx - min_gx),
                                         std::max(1, min_gy - 1), ElementType::Fire, 0.0f, 600.0f, 40);
                            }
                        }
                    }
                }

                grid.apply_pending_blast(boxes);
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

            if (box.kind == BoxKind::TNT)
            {
                sl::gprintf(static_cast<int>(pos.x - 14.0f), static_cast<int>(pos.y - 6.0f), {255, 255, 255}, "TNT");
            }
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

        if (use_lighting && lighting_ready && scene_target)
        {
            // Torch spotlight following the mouse
            sl::Light torch;
            torch.x = static_cast<float>(mx);
            torch.y = static_cast<float>(my);
            torch.radius = 320.0f;
            torch.intensity = 1.3f;
            torch.direction_x = 0.0f;
            torch.direction_y = 1.0f;
            torch.inner_angle = 20.0f;
            torch.outer_angle = 45.0f;
            torch.colour = {255, 240, 200};

            // Ambient warmth light in center of room
            sl::Light ambient_glow;
            ambient_glow.x = 400.0f;
            ambient_glow.y = 300.0f;
            ambient_glow.radius = 450.0f;
            ambient_glow.intensity = 0.45f;
            ambient_glow.colour = {120, 160, 220};

            const std::vector<sl::Light> scene_lights = {torch, ambient_glow};
            lighting.apply(scene_target, scene_lights, 0, 0, SCREEN_W, SCREEN_H);
        }
        else if (use_bloom && bloom_ready && scene_target)
        {
            bloom.apply(scene_target, 0, 0, SCREEN_W, SCREEN_H);
        }
        else if (scene_target)
        {
            sl::draw_sprite(scene_target, 0.0f, 0.0f);
        }

        // 5. Draw HUD Overlay directly on screen for razor-sharp typography
        sl::gprintf(16, 16, {255, 235, 140}, "CA Fluid & Falling Sand Sandbox (Tom Forsyth CA Model)");
        sl::gprintf(16, 38, {170, 190, 215},
                    "Selected: [ %s ] (Keys 1-9, 0, -)   Radius: %d ([ / ])   Bloom: %s (B)   Light: %s (L)",
                    ELEMENT_PROPERTIES[static_cast<std::size_t>(selected_element)].name,
                    brush_radius, use_bloom ? "ON" : "OFF", use_lighting ? "ON" : "OFF");
        sl::gprintf(16, 58, {150, 175, 200},
                    "Left Click: Draw | Right Click: Erase | C: Crate | V: Barrel | T: TNT | P: Scenario (%d/5)",
                    current_scenario % 5 + 1);
        sl::gprintf(16, 78, {130, 155, 180},
                    "Sand:%d Water:%d Oil:%d Lava:%d Powder:%d Fire:%d | Space: %s | R: Clear",
                    grid.count_elements(ElementType::Sand),
                    grid.count_elements(ElementType::Water),
                    grid.count_elements(ElementType::Oil),
                    grid.count_elements(ElementType::Lava),
                    grid.count_elements(ElementType::Gunpowder),
                    grid.count_elements(ElementType::Fire),
                    paused ? "PAUSED" : "RUNNING");

        sl::show_video_bitmap();
        sl::end_frame();
    }

    // Cleanup
    sl::wait_for_graphics();
    bloom.shutdown();
    lighting.shutdown();
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

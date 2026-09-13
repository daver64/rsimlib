#include "sl.h"
#include "recs.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>


// ============================================================================
// RECS + simlib
//
// Interactive ECS demonstration.
//
// Controls:
//
//   Left mouse       Spawn Wanderers
//   Right mouse      Spawn Seekers
//
//   1                Spawn 100 Wanderers
//   2                Spawn 100 Seekers
//   3                Spawn 500 Particles
//
//   C                for_each_chunk
//   P                parallel_for_each
//   K                parallel_for_each_chunk
//   R                Reset
//
//   Escape           Exit
//
// This example deliberately keeps simlib and RECS independent:
//
//     RECS
//       Entities
//       Components
//       Archetypes
//       Resources
//       Iteration
//
//     simlib
//       Window
//       Events
//       Input
//       Timing
//       Rendering
//       Text
//
// ============================================================================


namespace
{


// ============================================================================
// Constants
// ============================================================================

constexpr int WINDOW_WIDTH = 1200;
constexpr int WINDOW_HEIGHT = 800;

constexpr float ARENA_LEFT = 20.0f;
constexpr float ARENA_TOP = 90.0f;
constexpr float ARENA_RIGHT = 900.0f;
constexpr float ARENA_BOTTOM = 780.0f;

constexpr float PI = 3.14159265358979323846f;


// ============================================================================
// Components
// ============================================================================

struct Position
{
    float x;
    float y;
};


struct Velocity
{
    float x;
    float y;
};


struct Colour
{
    sl::Colour value;
};


struct Circle
{
    float radius;
};


// Entities with this component wander randomly.

struct Wanderer
{
    float turn_speed;
};


// Entities with this component move towards the mouse.

struct Seeker
{
    float speed;
};


// Temporary entities.

struct Lifetime
{
    float remaining;
};


// Tag component.

struct Particle
{
};


// ============================================================================
// Resources
// ============================================================================

struct GameTime
{
    float delta = 0.0f;
    float total = 0.0f;
};


struct MouseState
{
    float x = 0.0f;
    float y = 0.0f;
};


// 0 = for_each
// 1 = for_each_chunk
// 2 = parallel_for_each
// 3 = parallel_for_each_chunk

struct Settings
{
    int mode = 0;
};


struct Stats
{
    float update_ms = 0.0f;
    float render_ms = 0.0f;
};


// RECS's for_each callbacks operate on components.
//
// We keep particle entity handles here so lifetime processing can
// safely destroy expired particles.

struct ParticleEntities
{
    std::vector<recs::Entity> entities;
};


// ============================================================================
// Utility
// ============================================================================

float random_float(
    float minimum,
    float maximum)
{
    return minimum +
        (maximum - minimum) *
        (
            static_cast<float>(std::rand()) /
            static_cast<float>(RAND_MAX)
        );
}


float length(
    float x,
    float y)
{
    return std::sqrt(x * x + y * y);
}


void normalise(
    float& x,
    float& y)
{
    const float value =
        length(x, y);

    if (value > 0.0001f)
    {
        x /= value;
        y /= value;
    }
}


sl::Colour random_wanderer_colour()
{
    switch (std::rand() % 4)
    {
        case 0:
            return {80, 220, 150};

        case 1:
            return {70, 180, 255};

        case 2:
            return {180, 120, 255};

        default:
            return {255, 180, 70};
    }
}


// ============================================================================
// Entity creation
// ============================================================================

void spawn_wanderers(
    recs::World& world,
    int count,
    float centre_x,
    float centre_y)
{
    auto entities =
        world.create_batch(
            static_cast<size_t>(count)
        );

    for (recs::Entity entity : entities)
    {
        const float angle =
            random_float(
                0.0f,
                PI * 2.0f
            );

        const float speed =
            random_float(
                35.0f,
                100.0f
            );


        world.add<Position>(
            entity,
            centre_x +
                random_float(-40.0f, 40.0f),
            centre_y +
                random_float(-40.0f, 40.0f)
        );


        world.add<Velocity>(
            entity,
            std::cos(angle) * speed,
            std::sin(angle) * speed
        );


        world.add<Colour>(
            entity,
            random_wanderer_colour()
        );


        world.add<Circle>(
            entity,
            random_float(3.0f, 6.0f)
        );


        world.add<Wanderer>(
            entity,
            random_float(1.0f, 4.0f)
        );
    }
}


void spawn_seekers(
    recs::World& world,
    int count,
    float centre_x,
    float centre_y)
{
    auto entities =
        world.create_batch(
            static_cast<size_t>(count)
        );

    for (recs::Entity entity : entities)
    {
        const float angle =
            random_float(
                0.0f,
                PI * 2.0f
            );

        const float speed =
            random_float(
                20.0f,
                60.0f
            );


        world.add<Position>(
            entity,
            centre_x +
                random_float(-60.0f, 60.0f),
            centre_y +
                random_float(-60.0f, 60.0f)
        );


        world.add<Velocity>(
            entity,
            std::cos(angle) * speed,
            std::sin(angle) * speed
        );


        world.add<Colour>(
            entity,
            sl::Colour{
                255,
                210,
                70
            }
        );


        world.add<Circle>(
            entity,
            random_float(4.0f, 7.0f)
        );


        world.add<Seeker>(
            entity,
            random_float(
                80.0f,
                180.0f
            )
        );
    }
}


void spawn_particles(
    recs::World& world,
    int count,
    float x,
    float y)
{
    ParticleEntities& particle_entities =
        world.get_resource<
            ParticleEntities
        >();


    auto entities =
        world.create_batch(
            static_cast<size_t>(count)
        );


    for (recs::Entity entity : entities)
    {
        const float angle =
            random_float(
                0.0f,
                PI * 2.0f
            );

        const float speed =
            random_float(
                50.0f,
                280.0f
            );


        world.add<Position>(
            entity,
            x,
            y
        );


        world.add<Velocity>(
            entity,
            std::cos(angle) * speed,
            std::sin(angle) * speed
        );


        world.add<Colour>(
            entity,
            sl::Colour{
                255,
                static_cast<Uint8>(
                    random_float(
                        100.0f,
                        220.0f
                    )
                ),
                80
            }
        );


        world.add<Circle>(
            entity,
            random_float(2.0f, 4.0f)
        );


        world.add<Lifetime>(
            entity,
            random_float(0.5f, 2.0f)
        );


        world.add<Particle>(
            entity
        );


        particle_entities.entities.push_back(
            entity
        );
    }
}


// ============================================================================
// Systems
// ============================================================================


// ----------------------------------------------------------------------------
// Wander
//
// Demonstrates:
//
//     world.for_each<Velocity, Wanderer>()
//
// ----------------------------------------------------------------------------

void wander_system(
    recs::World& world)
{
    const float delta =
        world.get_resource<
            GameTime
        >().delta;


    world.for_each<
        Velocity,
        Wanderer
    >(
        [&](Velocity& velocity,
            Wanderer& wanderer)
        {
            const float angle =
                random_float(
                    -wanderer.turn_speed,
                    wanderer.turn_speed
                ) *
                delta;


            const float old_x =
                velocity.x;

            const float old_y =
                velocity.y;


            velocity.x =
                old_x * std::cos(angle) -
                old_y * std::sin(angle);

            velocity.y =
                old_x * std::sin(angle) +
                old_y * std::cos(angle);
        }
    );
}


// ----------------------------------------------------------------------------
// Seeker
//
// Demonstrates:
//
//     world.for_each<Position, Velocity, Seeker>()
//
// ----------------------------------------------------------------------------

void seeker_system(
    recs::World& world)
{
    const MouseState mouse =
        world.get_resource<
            MouseState
        >();

    const float delta =
        world.get_resource<
            GameTime
        >().delta;


    world.for_each<
        Position,
        Velocity,
        Seeker
    >(
        [&](Position& position,
            Velocity& velocity,
            Seeker& seeker)
        {
            float direction_x =
                mouse.x -
                position.x;

            float direction_y =
                mouse.y -
                position.y;


            if (
                length(
                    direction_x,
                    direction_y
                ) > 20.0f
            )
            {
                normalise(
                    direction_x,
                    direction_y
                );


                const float response =
                    std::min(
                        1.0f,
                        delta * 3.0f
                    );


                velocity.x +=
                    (
                        direction_x *
                        seeker.speed -
                        velocity.x
                    ) *
                    response;


                velocity.y +=
                    (
                        direction_y *
                        seeker.speed -
                        velocity.y
                    ) *
                    response;
            }
        }
    );
}


// ----------------------------------------------------------------------------
// Movement
//
// Switches between:
//
//     for_each
//     for_each_chunk
//     parallel_for_each
//     parallel_for_each_chunk
//
// ----------------------------------------------------------------------------

void movement_system(
    recs::World& world)
{
    const float delta =
        world.get_resource<
            GameTime
        >().delta;


    const int mode =
        world.get_resource<
            Settings
        >().mode;


    // ------------------------------------------------------------------------
    // Chunk iteration
    // ------------------------------------------------------------------------

    if (mode == 1)
    {
        world.for_each_chunk<
            Position,
            Velocity
        >(
            [&](Position* positions,
                Velocity* velocities,
                size_t count)
            {
                for (
                    size_t i = 0;
                    i < count;
                    ++i
                )
                {
                    positions[i].x +=
                        velocities[i].x *
                        delta;

                    positions[i].y +=
                        velocities[i].y *
                        delta;
                }
            }
        );

        return;
    }


    // ------------------------------------------------------------------------
    // Parallel entity iteration
    // ------------------------------------------------------------------------

    if (mode == 2)
    {
        world.parallel_for_each<
            Position,
            Velocity
        >(
            [&](Position& position,
                Velocity& velocity)
            {
                position.x +=
                    velocity.x *
                    delta;

                position.y +=
                    velocity.y *
                    delta;
            }
        );

        return;
    }


    // ------------------------------------------------------------------------
    // Parallel chunk iteration
    // ------------------------------------------------------------------------

    if (mode == 3)
    {
        world.parallel_for_each_chunk<
            Position,
            Velocity
        >(
            [&](Position* positions,
                Velocity* velocities,
                size_t count)
            {
                for (
                    size_t i = 0;
                    i < count;
                    ++i
                )
                {
                    positions[i].x +=
                        velocities[i].x *
                        delta;

                    positions[i].y +=
                        velocities[i].y *
                        delta;
                }
            }
        );

        return;
    }


    // ------------------------------------------------------------------------
    // Normal entity iteration
    // ------------------------------------------------------------------------

    world.for_each<
        Position,
        Velocity
    >(
        [&](Position& position,
            Velocity& velocity)
        {
            position.x +=
                velocity.x *
                delta;

            position.y +=
                velocity.y *
                delta;
        }
    );
}


// ----------------------------------------------------------------------------
// Boundary
// ----------------------------------------------------------------------------

void boundary_system(
    recs::World& world)
{
    world.for_each<
        Position,
        Velocity
    >(
        [](Position& position,
           Velocity& velocity)
        {
            if (position.x < ARENA_LEFT)
            {
                position.x =
                    ARENA_LEFT;

                velocity.x =
                    std::abs(
                        velocity.x
                    );
            }


            if (position.x > ARENA_RIGHT)
            {
                position.x =
                    ARENA_RIGHT;

                velocity.x =
                    -std::abs(
                        velocity.x
                    );
            }


            if (position.y < ARENA_TOP)
            {
                position.y =
                    ARENA_TOP;

                velocity.y =
                    std::abs(
                        velocity.y
                    );
            }


            if (position.y > ARENA_BOTTOM)
            {
                position.y =
                    ARENA_BOTTOM;

                velocity.y =
                    -std::abs(
                        velocity.y
                    );
            }
        }
    );
}


// ----------------------------------------------------------------------------
// Lifetime
//
// Uses explicit entity handles stored in ParticleEntities.
//
// ----------------------------------------------------------------------------

void lifetime_system(
    recs::World& world)
{
    const float delta =
        world.get_resource<
            GameTime
        >().delta;


    ParticleEntities& particles =
        world.get_resource<
            ParticleEntities
        >();


    std::vector<recs::Entity>
        alive_particles;


    alive_particles.reserve(
        particles.entities.size()
    );


    for (
        recs::Entity entity :
        particles.entities
    )
    {
        if (!world.alive(entity))
            continue;


        Lifetime* lifetime =
            world.get<Lifetime>(
                entity
            );


        if (!lifetime)
            continue;


        lifetime->remaining -=
            delta;


        if (
            lifetime->remaining <= 0.0f
        )
        {
            world.destroy(entity);
        }
        else
        {
            alive_particles.push_back(
                entity
            );
        }
    }


    particles.entities.swap(
        alive_particles
    );
}


// ============================================================================
// Rendering
// ============================================================================

const char* iteration_mode_name(
    int mode)
{
    static const char* names[] =
    {
        "for_each",
        "for_each_chunk",
        "parallel_for_each",
        "parallel_for_each_chunk"
    };

    return names[mode];
}


void render_arena()
{
    sl::rectfill(
        sl::screen,
        ARENA_LEFT,
        ARENA_TOP,
        ARENA_RIGHT,
        ARENA_BOTTOM,
        {
            18,
            28,
            42
        }
    );


    sl::rect(
        sl::screen,
        ARENA_LEFT,
        ARENA_TOP,
        ARENA_RIGHT,
        ARENA_BOTTOM,
        {
            70,
            105,
            135
        },
        2.0f
    );


    for (
        float x = ARENA_LEFT + 50.0f;
        x < ARENA_RIGHT;
        x += 50.0f
    )
    {
        sl::line(
            sl::screen,
            x,
            ARENA_TOP,
            x,
            ARENA_BOTTOM,
            {
                28,
                42,
                60
            }
        );
    }


    for (
        float y = ARENA_TOP + 50.0f;
        y < ARENA_BOTTOM;
        y += 50.0f
    )
    {
        sl::line(
            sl::screen,
            ARENA_LEFT,
            y,
            ARENA_RIGHT,
            y,
            {
                28,
                42,
                60
            }
        );
    }
}


void render_entities(
    recs::World& world)
{
    world.for_each<
        Position,
        Colour,
        Circle
    >(
        [](Position& position,
           Colour& colour,
           Circle& circle)
        {
            sl::circlefill(
                sl::screen,
                position.x,
                position.y,
                circle.radius,
                colour.value
            );
        }
    );
}


void render_mouse_target(
    recs::World& world)
{
    const MouseState& mouse =
        world.get_resource<
            MouseState
        >();


    sl::circle(
        sl::screen,
        mouse.x,
        mouse.y,
        12.0f,
        {
            255,
            230,
            100
        },
        2.0f
    );
}


void render_info_panel(
    recs::World& world)
{
    const Settings& settings =
        world.get_resource<
            Settings
        >();

    const Stats& stats =
        world.get_resource<
            Stats
        >();


    constexpr int PANEL_LEFT = 920;


    sl::rectfill(
        sl::screen,
        PANEL_LEFT,
        ARENA_TOP,
        1180,
        ARENA_BOTTOM,
        {
            24,
            34,
            48
        }
    );


    sl::rect(
        sl::screen,
        PANEL_LEFT,
        ARENA_TOP,
        1180,
        ARENA_BOTTOM,
        {
            70,
            105,
            135
        },
        2.0f
    );


    int y = 115;


    sl::gprintf(
        940,
        y,
        {
            120,
            210,
            255
        },
        "RECS WORLD"
    );


    y += 35;


    sl::gprintf(
        940,
        y,
        {
            210,
            220,
            230
        },
        "Entities: %d",
        static_cast<int>(
            world.get_entity_count()
        )
    );


    y += 25;


    sl::gprintf(
        940,
        y,
        {
            210,
            220,
            230
        },
        "Archetypes: %d",
        static_cast<int>(
            world.get_archetype_count()
        )
    );


    y += 45;


    sl::gprintf(
        940,
        y,
        {
            120,
            210,
            255
        },
        "Iteration"
    );


    y += 25;


    sl::gprintf(
        940,
        y,
        {
            210,
            220,
            230
        },
        "%s",
        iteration_mode_name(
            settings.mode
        )
    );


    y += 45;


    sl::gprintf(
        940,
        y,
        {
            120,
            210,
            255
        },
        "Timings"
    );


    y += 25;


    sl::gprintf(
        940,
        y,
        {
            155,
            170,
            190
        },
        "Update: %.3f ms",
        stats.update_ms
    );


    y += 22;


    sl::gprintf(
        940,
        y,
        {
            155,
            170,
            190
        },
        "Render: %.3f ms",
        stats.render_ms
    );


    y += 45;


    sl::gprintf(
        940,
        y,
        {
            155,
            170,
            190
        },
        "LMB: Wanderers"
    );


    y += 22;


    sl::gprintf(
        940,
        y,
        {
            155,
            170,
            190
        },
        "RMB: Seekers"
    );


    y += 22;


    sl::gprintf(
        940,
        y,
        {
            155,
            170,
            190
        },
        "1: +100 Wanderers"
    );


    y += 22;


    sl::gprintf(
        940,
        y,
        {
            155,
            170,
            190
        },
        "2: +100 Seekers"
    );


    y += 22;


    sl::gprintf(
        940,
        y,
        {
            155,
            170,
            190
        },
        "3: +500 Particles"
    );


    y += 22;


    sl::gprintf(
        940,
        y,
        {
            155,
            170,
            190
        },
        "C: Chunks"
    );


    y += 22;


    sl::gprintf(
        940,
        y,
        {
            155,
            170,
            190
        },
        "P: Parallel"
    );


    y += 22;


    sl::gprintf(
        940,
        y,
        {
            155,
            170,
            190
        },
        "K: Parallel chunks"
    );


    y += 22;


    sl::gprintf(
        940,
        y,
        {
            155,
            170,
            190
        },
        "R: Reset"
    );
}


void render_system(
    recs::World& world)
{
    sl::clear_to_colour(
        sl::screen,
        {
            8,
            12,
            20
        }
    );


    sl::gprintf(
        20,
        20,
        {
            130,
            220,
            255
        },
        "RECS + simlib ECS example"
    );


    sl::gprintf(
        20,
        45,
        {
            155,
            170,
            190
        },
        "Archetype ECS simulation using simlib rendering"
    );


    render_arena();

    render_entities(world);

    render_mouse_target(world);

    render_info_panel(world);
}


// ============================================================================
// Initialisation
// ============================================================================

void initialise_world(
    recs::World& world)
{
    world.set_resource<GameTime>();

    world.set_resource<MouseState>();

    world.set_resource<Settings>();

    world.set_resource<Stats>();

    world.set_resource<
        ParticleEntities
    >();


    spawn_wanderers(
        world,
        500,
        350.0f,
        430.0f
    );


    spawn_seekers(
        world,
        250,
        550.0f,
        430.0f
    );
}


// ============================================================================
// Main
// ============================================================================

}

int main(
    int argc,
    char* argv[])
{
    std::srand(
        static_cast<unsigned int>(
            std::time(nullptr)
        )
    );


    if (
        !sl::configure_graphics_backend_from_args(
            argc,
            argv
        ) ||

        !sl::set_gfx_mode(
            sl::GFX_AUTODETECT_WINDOWED,
            WINDOW_WIDTH,
            WINDOW_HEIGHT
        )
    )
    {
        return -1;
    }


    sl::set_fps(60);


    recs::World world;

    initialise_world(world);


    bool running = true;


    while (running)
    {
        // --------------------------------------------------------------------
        // Events
        // --------------------------------------------------------------------

        sl::Event event;


        while (
            sl::poll_event(&event)
        )
        {
            if (
                event.type() ==
                    sl::Event::Type::quit ||

                (
                    event.type() ==
                        sl::Event::Type::key_down &&

                    event.key() ==
                        sl::Event::Key::escape
                )
            )
            {
                running = false;
            }


            if (
                event.type() ==
                    sl::Event::Type::key_down &&

                !event.key_repeat()
            )
            {
                if (
                    event.key() ==
                        sl::Event::Key::digit_1
                )
                {
                    spawn_wanderers(
                        world,
                        100,
                        450.0f,
                        430.0f
                    );
                }


                else if (
                    event.key() ==
                        sl::Event::Key::digit_2
                )
                {
                    spawn_seekers(
                        world,
                        100,
                        450.0f,
                        430.0f
                    );
                }


                else if (
                    event.key() ==
                        sl::Event::Key::digit_3
                )
                {
                    spawn_particles(
                        world,
                        500,
                        450.0f,
                        430.0f
                    );
                }


                else if (
                    event.key() ==
                        sl::Event::Key::letter_c
                )
                {
                    world.get_resource<
                        Settings
                    >().mode = 1;
                }


                else if (
                    event.key() ==
                        sl::Event::Key::letter_p
                )
                {
                    world.get_resource<
                        Settings
                    >().mode = 2;
                }


                else if (
                    event.key() ==
                        sl::Event::Key::letter_k
                )
                {
                    world.get_resource<
                        Settings
                    >().mode = 3;
                }


                else if (
                    event.key() ==
                        sl::Event::Key::letter_r
                )
                {
                    world =
                        recs::World{};

                    initialise_world(world);
                }
            }


            sl::display_handle_event(
                event
            );
        }


        // --------------------------------------------------------------------
        // Time
        // --------------------------------------------------------------------

        GameTime& time =
            world.get_resource<
                GameTime
            >();


        time.delta =
            std::min(
                0.05f,
                static_cast<float>(
                    sl::get_frame_time()
                ) /
                1000.0f
            );


        time.total +=
            time.delta;


        // --------------------------------------------------------------------
        // Input resource
        // --------------------------------------------------------------------

        MouseState& mouse =
            world.get_resource<
                MouseState
            >();


        mouse.x =
            static_cast<float>(
                sl::mouse_x()
            );

        mouse.y =
            static_cast<float>(
                sl::mouse_y()
            );


        // --------------------------------------------------------------------
        // Mouse spawning
        // --------------------------------------------------------------------

        const bool inside_arena =
            mouse.x >= ARENA_LEFT &&
            mouse.x <= ARENA_RIGHT &&
            mouse.y >= ARENA_TOP &&
            mouse.y <= ARENA_BOTTOM;


        const std::uint32_t buttons =
            sl::mouse_buttons();


        if (
            inside_arena &&
            (buttons & 1)
        )
        {
            spawn_wanderers(
                world,
                3,
                mouse.x,
                mouse.y
            );
        }


        // SDL right mouse button mask.

        if (
            inside_arena &&
            (buttons & 4)
        )
        {
            spawn_seekers(
                world,
                3,
                mouse.x,
                mouse.y
            );
        }


        // --------------------------------------------------------------------
        // Update
        // --------------------------------------------------------------------

        const auto update_start =
            std::chrono::steady_clock::now();


        wander_system(world);

        seeker_system(world);

        movement_system(world);

        boundary_system(world);

        lifetime_system(world);


        const auto update_end =
            std::chrono::steady_clock::now();


        world.get_resource<
            Stats
        >().update_ms =
            std::chrono::duration<
                float,
                std::milli
            >(
                update_end -
                update_start
            ).count();


        // --------------------------------------------------------------------
        // Render
        // --------------------------------------------------------------------

        const auto render_start =
            std::chrono::steady_clock::now();


        render_system(world);


        const auto render_end =
            std::chrono::steady_clock::now();


        world.get_resource<
            Stats
        >().render_ms =
            std::chrono::duration<
                float,
                std::milli
            >(
                render_end -
                render_start
            ).count();


        sl::show_video_bitmap();

        sl::end_frame();
    }


    sl::wait_for_graphics();

    sl::shutdown();

    return 0;
}
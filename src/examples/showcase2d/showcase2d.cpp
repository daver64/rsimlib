#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    constexpr float screen_width = 960.0f;
    constexpr float screen_height = 640.0f;

    struct Wall
    {
        float x, y, width, height;
    };

    struct Crystal
    {
        float x, y;
        bool collected = false;
    };
}

int main(int argc, char *argv[])
{
    sl::set_text_panel_style({true});
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, static_cast<int>(screen_width), static_cast<int>(screen_height)))
    {
        std::fprintf(stderr, "Unable to start 2D showcase: %s\n", sl::last_error().c_str());
        return -1;
    }
    sl::set_window_title("simlib showcase - 2D lightkeeper");

    sl::PhysicsWorld *physics = sl::create_physics_world({0.0f, 0.0f});
    sl::PhysicsBody *player = sl::create_physics_body(physics, sl::BodyType::dynamic_body, {90.0f, 90.0f});
    const std::vector<Wall> walls = {
        {480.0f, 26.0f, 920.0f, 20.0f}, {480.0f, 614.0f, 920.0f, 20.0f},
        {26.0f, 320.0f, 20.0f, 600.0f}, {934.0f, 320.0f, 20.0f, 600.0f},
        {280.0f, 190.0f, 300.0f, 22.0f}, {590.0f, 330.0f, 22.0f, 280.0f},
        {350.0f, 470.0f, 360.0f, 22.0f}, {790.0f, 180.0f, 22.0f, 220.0f},
    };
    bool physics_ready = physics && player && sl::add_circle_fixture(player, 13.0f, 1.0f, 0.2f, 0.0f);
    if (physics_ready)
    {
        for (const Wall &wall : walls)
        {
            sl::PhysicsBody *body = sl::create_physics_body(physics, sl::BodyType::static_body, {wall.x, wall.y});
            if (!body || !sl::add_box_fixture(body, wall.width, wall.height))
            {
                physics_ready = false;
                break;
            }
        }
    }

    sl::Bitmap *scene = sl::create_render_target(static_cast<int>(screen_width), static_cast<int>(screen_height));
    sl::LightingPass lighting;
    const bool lighting_ready = physics_ready && scene && lighting.initialise();
    if (!physics_ready || !scene || !lighting_ready)
    {
        if (!physics_ready)
            std::fprintf(stderr, "Unable to initialize physics world: %s\n", sl::last_error().c_str());
        else if (!scene)
            std::fprintf(stderr, "Unable to create scene render target: %s\n", sl::last_error().c_str());
        else
            std::fprintf(stderr, "Unable to initialize lighting: %s\n", lighting.error().c_str());
        lighting.shutdown();
        sl::destroy_bitmap(scene);
        sl::destroy_physics_world(physics);
        sl::shutdown();
        return -1;
    }
    lighting.set_ambient(0.2f);

    sl::EmitterConfig sparks;
    sparks.emit_rate = 45.0f;
    sparks.particle_lifetime = 0.75f;
    sparks.min_speed = 12.0f;
    sparks.max_speed = 75.0f;
    sparks.start_size = 5.0f;
    sparks.end_size = 0.0f;
    sparks.start_colour = {110, 220, 255, 230};
    sparks.end_colour = {40, 120, 255, 0};
    sparks.max_particles = 180;
    sl::ParticleEmitter emitter(sparks, 90.0f, 90.0f);

    std::vector<Crystal> crystals = {
        {190.0f, 105.0f}, {410.0f, 110.0f}, {510.0f, 260.0f},
        {720.0f, 470.0f}, {845.0f, 520.0f}, {850.0f, 100.0f},
    };
    std::vector<sl::ShadowCaster> casters;
    for (const Wall &wall : walls)
        casters.push_back(sl::make_rectangle_shadow_caster(wall.x - wall.width * 0.5f, wall.y - wall.height * 0.5f,
                                                            wall.x + wall.width * 0.5f, wall.y + wall.height * 0.5f));

    sl::set_fps(60);
    bool running = true;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            else if (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::letter_r)
            {
                for (Crystal &crystal : crystals)
                    crystal.collected = false;
                sl::set_physics_body_transform(player, {90.0f, 90.0f});
                sl::set_physics_body_velocity(player, {0.0f, 0.0f});
                emitter.clear();
            }
            sl::display_handle_event(event);
        }

        const float dt = std::min(0.05f, static_cast<float>(sl::get_frame_time()) / 1000.0f);
        sl::Vec2 velocity{};
        if (sl::key_down(sl::Key::letter_a) || sl::key_down(sl::Key::arrow_left))
            velocity.x -= 190.0f;
        if (sl::key_down(sl::Key::letter_d) || sl::key_down(sl::Key::arrow_right))
            velocity.x += 190.0f;
        if (sl::key_down(sl::Key::letter_w) || sl::key_down(sl::Key::arrow_up))
            velocity.y -= 190.0f;
        if (sl::key_down(sl::Key::letter_s) || sl::key_down(sl::Key::arrow_down))
            velocity.y += 190.0f;
        if (velocity.x != 0.0f && velocity.y != 0.0f)
        {
            velocity.x *= 0.70710678f;
            velocity.y *= 0.70710678f;
        }
        sl::set_physics_body_velocity(player, velocity);
        sl::step_physics_world(physics, dt);
        const sl::Vec2 position = sl::physics_body_position(player);
        emitter.set_position(position.x, position.y);
        emitter.set_active(velocity.x != 0.0f || velocity.y != 0.0f);
        emitter.update(dt);

        int collected = 0;
        for (Crystal &crystal : crystals)
        {
            if (!crystal.collected && std::hypot(position.x - crystal.x, position.y - crystal.y) < 25.0f)
            {
                crystal.collected = true;
                emitter.clear();
            }
            collected += crystal.collected ? 1 : 0;
        }

        sl::begin_render_target(scene);
        sl::clear_render_target({13, 19, 34});
        for (const Wall &wall : walls)
            sl::rectfill(sl::screen, wall.x - wall.width * 0.5f, wall.y - wall.height * 0.5f,
                         wall.x + wall.width * 0.5f, wall.y + wall.height * 0.5f, {48, 68, 94});
        emitter.render();
        for (const Crystal &crystal : crystals)
        {
            if (!crystal.collected)
            {
                sl::circlefill(sl::screen, crystal.x, crystal.y, 9.0f, {95, 215, 255});
                sl::circle(sl::screen, crystal.x, crystal.y, 13.0f, {180, 240, 255});
            }
        }
        sl::circlefill(sl::screen, position.x, position.y, 13.0f, {255, 204, 95});
        sl::end_render_target();

        sl::Light player_light;
        player_light.x = position.x;
        player_light.y = position.y;
        player_light.radius = 260.0f;
        player_light.intensity = 1.25f;
        player_light.colour = {255, 190, 105};
        sl::Light crystal_light;
        crystal_light.x = 745.0f;
        crystal_light.y = 110.0f;
        crystal_light.radius = 170.0f;
        crystal_light.intensity = 0.75f;
        crystal_light.colour = {75, 155, 255};
        lighting.apply(scene, std::vector<sl::Light>{player_light, crystal_light}, 0, 0,
                       static_cast<int>(screen_width), static_cast<int>(screen_height), casters);

        sl::gprintf(18, 16, {232, 239, 255}, "LIGHTKEEPER  |  Crystals %d / %d", collected, static_cast<int>(crystals.size()));
        sl::gprintf(18, 40, {180, 198, 223}, "Move: WASD / arrows    Reset: R    Quit: Esc");
        if (collected == static_cast<int>(crystals.size()))
            sl::gprintf_center(84, {145, 235, 255}, "All crystals found - press R to explore again");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    lighting.shutdown();
    sl::destroy_bitmap(scene);
    sl::destroy_physics_world(physics);
    sl::wait_for_graphics();
    sl::shutdown();
    return 0;
}

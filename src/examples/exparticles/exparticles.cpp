#include "sl.h"

#include <algorithm>

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }

    sl::EmitterConfig config;
    config.emit_rate = 180.0f;
    config.particle_lifetime = 1.6f;
    config.min_speed = 40.0f;
    config.max_speed = 180.0f;
    config.direction_degrees = -90.0f;
    config.spread_degrees = 34.0f;
    config.start_size = 13.0f;
    config.end_size = 2.0f;
    config.start_colour = {255, 220, 105, 235};
    config.end_colour = {235, 75, 48, 0};
    config.gravity = 95.0f;
    config.max_particles = 900;

    sl::ParticleEmitter emitter(config, 400.0f, 500.0f);
    sl::Bitmap *scene = sl::create_render_target(800, 600);
    sl::Bitmap *particles = sl::create_render_target(800, 600);
    sl::Bitmap *composite = sl::create_render_target(800, 600);
    sl::Blur blur;
    sl::Blur glow;
    const bool blur_ready = blur.initialise();
    const bool glow_ready = glow.initialise();
        blur.set_radius(9.0f);
        blur.set_iterations(3);
        blur.set_opacity(1.0f);
        glow.set_radius(10.0f);
        glow.set_iterations(3);
        glow.set_opacity(0.9f);
    if (!scene || !particles || !composite || !blur_ready || !glow_ready)
    {
        sl::destroy_bitmap(scene);
        sl::destroy_bitmap(particles);
        sl::destroy_bitmap(composite);
        blur.shutdown();
        glow.shutdown();
        sl::shutdown();
        return -1;
    }

    bool running = true;
    bool gravity_enabled = true;
    bool use_blur = false;
    bool use_glow = false;
    sl::set_fps(60);

    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down &&
                 event.key() == sl::Event::Key::escape))
            {
                running = false;
            }
            if (event.type() == sl::Event::Type::key_down && !event.key_repeat())
            {
                if (event.key() == sl::Event::Key::space)
                    emitter.set_active(!emitter.is_active());
                else if (event.key() == sl::Event::Key::letter_g)
                {
                    gravity_enabled = !gravity_enabled;
                    config.gravity = gravity_enabled ? 95.0f : 0.0f;
                    emitter = sl::ParticleEmitter(config, emitter.x(), emitter.y());
                }
                else if (event.key() == sl::Event::Key::letter_c)
                    emitter.clear();
                else if (event.key() == sl::Event::Key::letter_b)
                    use_blur = !use_blur;
                else if (event.key() == sl::Event::Key::letter_o)
                    use_glow = !use_glow;
            }
            sl::display_handle_event(event);
        }

        const float elapsed = std::min(0.05f, static_cast<float>(sl::get_frame_time()) / 1000.0f);
        emitter.set_position(static_cast<float>(sl::mouse_x()), static_cast<float>(sl::mouse_y()));
        emitter.update(elapsed);

        sl::begin_render_target(scene);
        sl::clear_render_target({18, 23, 34});
        sl::end_render_target();

        sl::begin_render_target(particles);
        sl::clear_render_target({0, 0, 0, 0});
        emitter.render();
        sl::circlefill(sl::screen, emitter.x(), emitter.y(), 5.0f,
            emitter.is_active() ? sl::Colour{255, 240, 150} : sl::Colour{130, 145, 165});
        sl::end_render_target();

        sl::begin_render_target(composite);
        sl::clear_render_target({8, 11, 18});
        sl::draw_sprite(scene, 0.0f, 0.0f);
        if (use_glow)
        {
            for (int pass = 0; pass < 1; ++pass)
                glow.apply(particles, 0, 0, 800, 600);
        }
        sl::draw_sprite(particles, 0.0f, 0.0f);
        if (use_blur)
            blur.apply(particles, 0, 0, 800, 600);
        sl::end_render_target();

        sl::clear_to_colour(sl::screen, {8, 11, 18});
        sl::draw_sprite(composite, 0.0f, 0.0f);
        sl::gprintf_center(28, {235, 220, 155}, "Particle emitter example");
        sl::gprintf_center(56, {175, 190, 210}, "Move the mouse to place the emitter");
        sl::gprintf_center(80, {175, 190, 210}, "Space: emit %s    G: gravity %s    B: blur %s    O: glow %s",
            emitter.is_active() ? "on" : "off", gravity_enabled ? "on" : "off",
            use_blur ? "on" : "off", use_glow ? "on" : "off");
        sl::gprintf_center(104, {175, 190, 210}, "Escape: exit");
        sl::gprintf(16, 570, {145, 160, 178}, "Live particles: %d", static_cast<int>(emitter.particle_count()));
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    blur.shutdown();
    glow.shutdown();
    sl::destroy_bitmap(particles);
    sl::destroy_bitmap(composite);
    sl::destroy_bitmap(scene);
    sl::shutdown();
}
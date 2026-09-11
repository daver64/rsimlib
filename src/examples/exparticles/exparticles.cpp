#include "sl.h"
#include "particles.h"

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
    bool running = true;
    bool gravity_enabled = true;
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
            }
            sl::display_handle_event(event);
        }

        const float elapsed = std::min(0.05f, static_cast<float>(sl::get_frame_time()) / 1000.0f);
        emitter.set_position(static_cast<float>(sl::mouse_x()), static_cast<float>(sl::mouse_y()));
        emitter.update(elapsed);

        sl::clear_to_colour(sl::screen, {18, 23, 34});
        sl::gprintf_center(28, {235, 220, 155}, "Particle emitter example");
        sl::gprintf_center(56, {175, 190, 210}, "Move the mouse to place the emitter");
        sl::gprintf_center(80, {175, 190, 210}, "Space: emit %s    G: gravity %s    C: clear    Escape: exit",
            emitter.is_active() ? "on" : "off", gravity_enabled ? "on" : "off");
        emitter.render();
        sl::circlefill(sl::screen, emitter.x(), emitter.y(), 5.0f,
            emitter.is_active() ? sl::Colour{255, 240, 150} : sl::Colour{130, 145, 165});
        sl::gprintf(16, 570, {145, 160, 178}, "Live particles: %d", static_cast<int>(emitter.particle_count()));
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::shutdown();
    return 0;
}
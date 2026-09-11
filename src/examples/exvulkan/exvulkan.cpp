#include "sl.h"

int main(int argc, char *argv[])
{
    if (!sl::set_graphics_backend(sl::GraphicsBackend::vulkan) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }

    sl::Bitmap *balloon = sl::load_bitmap("assets/textures/balloon_red.png");
    if (!balloon)
    {
        sl::shutdown();
        return -1;
    }

    sl::Bloom bloom;
    if (!bloom.initialise())
    {
        sl::gprintf(0, 0, {255, 60, 60}, "Bloom init failed: %s", bloom.error().c_str());
    }
    bloom.set_threshold(0.4f);
    bloom.set_intensity(1.2f);

    // Bloom reads from and composites over a scene texture, so the scene must be
    // drawn into an offscreen render target rather than directly onto sl::screen.
    sl::Bitmap *scene = sl::create_render_target(sl::screen_width(), sl::screen_height());

    sl::set_fps(60);
    bool running = true;
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
            sl::display_handle_event(event);
        }

        const bool has_scene = scene && sl::begin_render_target(scene);
        if (has_scene) sl::clear_render_target({24, 30, 42});
        else sl::clear_to_colour(sl::screen, {24, 30, 42});
        sl::gprintf_center(32, {220, 230, 240}, "Vulkan textured quad smoke test - Escape to exit");
        sl::draw_sprite_rotated(balloon, 400.0f, 300.0f, static_cast<float>(sl::time_ms() % 3600) * 0.1f);
        sl::line(sl::screen, 80.0f, 500.0f, 220.0f, 500.0f, {255, 180, 80});
        sl::rect(sl::screen, 250.0f, 450.0f, 330.0f, 530.0f, {100, 210, 255});
        sl::trianglefill(sl::screen, 380.0f, 520.0f, 430.0f, 440.0f, 480.0f, 520.0f, {120, 230, 140});
        sl::circlefill(sl::screen, 600.0f, 480.0f, 42.0f, {220, 100, 150});
        if (has_scene) sl::end_render_target();

        if (has_scene && bloom.is_valid())
        {
            bloom.apply(scene, 0, 0, sl::screen_width(), sl::screen_height());
        }

        sl::show_video_bitmap();
        sl::end_frame();
    }

    bloom.shutdown();
    sl::destroy_bitmap(balloon);
    sl::shutdown();
    return 0;
}

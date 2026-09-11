#include "sl.h"

#include <cmath>
#include <cstdio>

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 900, 600))
    {
        return -1;
    }

    sl::Shader shader;
    const bool loaded = shader.load_files(
        "shaders/glsl/default_2d.vert",
        "shaders/glsl/tint.frag",
        {"uTexture"});
    sl::Bitmap *texture = sl::load_bitmap("assets/textures/balloon_red.png");
    const bool ready = loaded && texture;
    if (!loaded)
    {
        std::fprintf(stderr, "Shader error: %s\n", shader.error().c_str());
    }

    bool running = true;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down &&
                 event.key() == sl::Event::Key::escape))
                running = false;
            sl::display_handle_event(event);
        }
        sl::clear_to_colour(sl::screen, {27, 32, 43});
        if (ready)
        {
            const float time = static_cast<float>(sl::time_ms()) * 0.001f;
            const float x = 400.0f + std::cos(time) * 170.0f;
            const float y = 180.0f + std::sin(time * 1.3f) * 55.0f;
            shader.draw_textured_quad(texture, x - 96.0f, y, 192.0f, 192.0f);
        }
        sl::gprintf_center(120, {232, 236, 244}, "Shader file loading example");
        sl::gprintf_center(160, ready ? sl::Colour{130, 220, 150} : sl::Colour{240, 130, 120},
            ready ? "Tint shader is drawing the balloon" : "shader or texture loading failed");
        sl::gprintf_center(200, {160, 175, 195}, "Press Escape to exit");
        sl::show_video_bitmap();
        sl::end_frame();
    }
    sl::wait_for_graphics();
    shader.reset();
    sl::destroy_bitmap(texture);
    sl::shutdown();
    return 0;
}
#include "sl.h"

#include <cstdio>

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 360))
    {
        return -1;
    }

    sl::Shader shader;
    const bool loaded = shader.load_files(
        "shaders/glsl/default_2d.vert",
        "shaders/glsl/default_2d.frag",
        {"uTexture"});
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
        sl::gprintf_center(120, {232, 236, 244}, "Shader file loading example");
        sl::gprintf_center(160, loaded ? sl::Colour{130, 220, 150} : sl::Colour{240, 130, 120},
            loaded ? "default_2d shaders compiled" : "shader compilation failed");
        sl::gprintf_center(200, {160, 175, 195}, "Press Escape to exit");
        sl::show_video_bitmap();
        sl::end_frame();
    }

    shader.reset();
    sl::shutdown();
    return 0;
}
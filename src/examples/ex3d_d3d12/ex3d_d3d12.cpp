#include "sl.h"
#include "scene3d.h"
#include "renderer.h"

#ifdef _WIN32
#include "d3d12_context.h"
#include <d3d12.h>
#include <d3dcompiler.h>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>

namespace
{
    struct Vertex
    {
        float position[3];
        float colour[3];
    };

    void append_face(Vertex *vertices, int &offset, const float corners[8][3],
                     int a, int b, int c, int d, const float colour[3])
    {
        const int indices[] = {a, b, c, a, c, d};
        for (int index : indices)
        {
            for (int component = 0; component < 3; ++component)
            {
                vertices[offset].position[component] = corners[index][component];
                vertices[offset].colour[component] = colour[component];
            }
            ++offset;
        }
    }
}

int main(int argc, char *argv[])
{
    if (!sl::set_graphics_backend(sl::GraphicsBackend::d3d12) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        std::fprintf(stderr, "Failed to initialize D3D12 graphics backend.\n");
        return -1;
    }

    sl::Camera camera;
    bool running = true;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            sl::display_handle_event(event);
        }

        sl::clear_to_colour(sl::screen, {9, 13, 20});
        sl::gprintf_center(32, {232, 236, 244}, "Direct3D 12 3D example - Escape to exit");
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::shutdown();
    return 0;
}
#else
int main(int argc, char *argv[])
{
    std::fprintf(stderr, "ex3d_d3d12 is only supported on Windows.\n");
    return 0;
}
#endif

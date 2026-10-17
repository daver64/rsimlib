#include "sl.h"
#include "scene3d.h"
#include "surface3d.h"

#ifdef _WIN32
#include <cstdio>
#include <vector>

namespace
{
    void append_face(std::vector<sl::Vertex3D> &vertices, const float corners[8][3],
                     int a, int b, int c, int d, const float colour[3])
    {
        const int indices[] = {a, b, c, a, c, d};
        for (int index : indices)
        {
            sl::Vertex3D vertex;
            for (int component = 0; component < 3; ++component)
            {
                vertex.position[component] = corners[index][component];
                vertex.colour[component] = colour[component];
            }
            vertices.push_back(vertex);
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

    sl::Surface3D *surface = sl::create_surface_3d(800, 600, 1);
    const float corners[8][3] = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}};
    const float colours[6][3] = {
        {0.95f, 0.25f, 0.25f}, {0.25f, 0.75f, 1.0f}, {0.35f, 0.95f, 0.45f}, {1.0f, 0.75f, 0.2f}, {0.75f, 0.35f, 1.0f}, {0.2f, 0.9f, 0.85f}};
    std::vector<sl::Vertex3D> vertices;
    append_face(vertices, corners, 0, 1, 2, 3, colours[0]);
    append_face(vertices, corners, 5, 4, 7, 6, colours[1]);
    append_face(vertices, corners, 4, 0, 3, 7, colours[2]);
    append_face(vertices, corners, 1, 5, 6, 2, colours[3]);
    append_face(vertices, corners, 3, 2, 6, 7, colours[4]);
    append_face(vertices, corners, 4, 5, 1, 0, colours[5]);
    std::vector<std::uint32_t> indices(vertices.size());
    for (std::size_t index = 0; index < indices.size(); ++index)
        indices[index] = static_cast<std::uint32_t>(index);
    sl::Mesh *cube = sl::create_mesh(vertices, indices);
    if (!surface || !cube)
    {
        std::fprintf(stderr, "Direct3D 12 3D setup failed: %s\n", sl::last_error().c_str());
        sl::destroy_mesh(cube);
        sl::destroy_surface_3d(surface);
        sl::shutdown();
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

        const float time = static_cast<float>(sl::time_ms()) * 0.001f;
        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({9, 13, 20});
            sl::Material material;
            material.lit = false;
            material.cull = sl::CullMode::none;
            sl::draw_mesh(cube, sl::model_matrix({0.0f, 0.0f, 0.0f}, {time * 0.7f, time, time * 0.35f}), material);
            sl::end_surface_3d();
        }
        sl::clear_to_colour(sl::screen, {9, 13, 20});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0, 0);
        sl::gprintf_center(32, {232, 236, 244}, "Direct3D 12 3D example - Escape to exit");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::destroy_mesh(cube);
    sl::destroy_surface_3d(surface);
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

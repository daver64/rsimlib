#include "sl.h"

#include <cstdio>

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv))
        return -1;
    if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
        return -1;

    sl::Surface3D *surface = sl::create_surface_3d(480, 360);
    sl::Mesh *cube = sl::create_cube_mesh(1.0f, {240, 120, 80});
    sl::Mesh *sphere = sl::create_sphere_mesh(0.5f, 32, 16, {90, 170, 255});
    sl::Mesh *plane = sl::create_plane_mesh(6.0f, 6.0f, {120, 130, 120});
    if (!surface || !cube || !sphere || !plane)
    {
        std::fprintf(stderr, "3D surface unavailable on this backend.\n");
        sl::destroy_mesh(cube);
        sl::destroy_mesh(sphere);
        sl::destroy_mesh(plane);
        sl::destroy_surface_3d(surface);
        sl::shutdown();
        return -1;
    }

    sl::Camera camera;
    camera.position = {2.5f, 2.2f, 4.0f};
    camera.target = {0.0f, 0.3f, 0.0f};
    sl::set_light_3d({{-0.4f, -1.0f, -0.6f}, 0.25f});

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
            sl::clear_surface_3d({20, 30, 60});
            sl::draw_mesh(plane, sl::model_matrix({0, -0.5f, 0}, {0, 0, 0}), {{255, 255, 255}, true});
            sl::draw_mesh(cube, sl::model_matrix({-0.8f, 0, 0}, {time * 0.7f, time, 0}), {{255, 255, 255}, true});
            sl::draw_mesh(sphere, sl::model_matrix({0.9f, 0, 0}, {0, 0, 0}), {{255, 255, 255}, true});
            sl::end_surface_3d();
        }

        sl::clear_to_colour(sl::screen, {40, 40, 40});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 160, 120);
        sl::gprintf_center(32, {232, 236, 244}, "Surface3D example - Escape to exit");
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::destroy_mesh(cube);
    sl::destroy_mesh(sphere);
    sl::destroy_mesh(plane);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}

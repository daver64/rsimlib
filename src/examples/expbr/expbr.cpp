#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// A grid of spheres: metallic increases left to right, roughness increases bottom to top (top row is rough).
// The bottom row adds an emissive sphere. P toggles PBR against plain Lambert shading.
int main(int argc, char **argv)
{
    std::vector<char *> backend_arguments{argv[0]};
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument(argv[i]);
        if (argument == "--gl" || argument == "--vulkan" || argument == "--d3d12")
            backend_arguments.push_back(argv[i]);
        else
        {
            std::fprintf(stderr, "Usage: expbr [--gl|--vulkan|--d3d12]\n");
            return 2;
        }
    }
    if (!sl::configure_graphics_backend_from_args(static_cast<int>(backend_arguments.size()), backend_arguments.data()) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 960, 640))
    {
        std::fprintf(stderr, "Graphics initialization failed: %s\n", sl::last_error().c_str());
        return 1;
    }
    sl::set_window_title("simlib - PBR materials");
    sl::Surface3D *surface = sl::create_surface_3d(960, 640, 4);
    sl::Mesh *sphere = sl::create_sphere_mesh(0.45f, 48, 32);
    if (!surface || !sphere)
    {
        std::fprintf(stderr, "3D unavailable: %s\n", sl::last_error().c_str());
        sl::shutdown();
        return 1;
    }

    constexpr int columns = 6;
    constexpr int rows = 4;
    sl::Camera camera;
    camera.target = {0.0f, 0.0f, 0.0f};
    float yaw = 0.0f, pitch = 0.0f, distance = 7.5f, light_angle = 0.6f;
    bool pbr = true;
    bool running = true;
    std::uint32_t frame = 0;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            else if (event.type() == sl::Event::Type::key_down)
            {
                if (event.key() == sl::Event::Key::arrow_left)
                    yaw -= 0.08f;
                else if (event.key() == sl::Event::Key::arrow_right)
                    yaw += 0.08f;
                else if (event.key() == sl::Event::Key::arrow_up)
                    pitch = std::min(1.2f, pitch + 0.06f);
                else if (event.key() == sl::Event::Key::arrow_down)
                    pitch = std::max(-1.2f, pitch - 0.06f);
                else if (event.key() == sl::Event::Key::plus || event.key() == sl::Event::Key::equals ||
                         event.key() == sl::Event::Key::keypad_plus)
                    distance = std::max(2.0f, distance / 1.1f);
                else if (event.key() == sl::Event::Key::minus || event.key() == sl::Event::Key::keypad_minus)
                    distance = std::min(30.0f, distance * 1.1f);
                else if (event.key() == sl::Event::Key::letter_l)
                    light_angle += 0.25f;
                else if (event.key() == sl::Event::Key::letter_p)
                    pbr = !pbr;
            }
            sl::display_handle_event(event);
        }

        camera.position = camera.target + glm::vec3(std::sin(yaw) * std::cos(pitch), std::sin(pitch),
                                                    std::cos(yaw) * std::cos(pitch)) * distance;
        sl::set_light_3d({{std::cos(light_angle) * 0.6f, -0.7f, -std::sin(light_angle) * 0.6f - 0.4f}, 0.25f});
        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({30, 40, 58});
            for (int row = 0; row < rows; ++row)
                for (int column = 0; column < columns; ++column)
                {
                    sl::Material material;
                    material.tint = {215, 170, 60, 255};
                    material.pbr = pbr;
                    material.metallic = static_cast<float>(column) / (columns - 1);
                    material.roughness = std::max(0.05f, static_cast<float>(row) / (rows - 1));
                    const glm::vec3 position((column - (columns - 1) * 0.5f) * 1.1f, (row - (rows - 1) * 0.5f) * 1.1f, 0.0f);
                    sl::draw_mesh(sphere, glm::translate(glm::mat4(1.0f), position), material);
                }
            sl::Material glow;
            glow.tint = {40, 40, 40, 255};
            glow.pbr = pbr;
            glow.metallic = 0.0f;
            glow.roughness = 0.6f;
            glow.emissive[0] = 0.1f;
            glow.emissive[1] = 0.9f;
            glow.emissive[2] = 0.3f;
            sl::draw_mesh(sphere, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -2.5f, 0.0f)), glow);
            sl::end_surface_3d();
        }
        sl::clear_to_colour(sl::screen, {18, 22, 32});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0, 0);
        sl::gprintf(16, 16, {255, 255, 255}, "Arrows: orbit  =/-: zoom  L: rotate light  P: PBR %s  Esc: exit",
                    pbr ? "on" : "off (Lambert)");
        sl::gprintf(16, 36, {200, 210, 225}, "Metallic 0..1 left to right, roughness 0..1 bottom to top; green = emissive");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();

        ++frame;
        const char *frames = std::getenv("RSIMLIB_FRAMES");
        if (frames && frame >= static_cast<std::uint32_t>(std::max(1, std::atoi(frames))))
            running = false;
        const char *screenshot = std::getenv("RSIMLIB_SCREENSHOT");
        if (screenshot && frame == 2u && !sl::save_bitmap(sl::screen, screenshot))
            std::fprintf(stderr, "Screenshot save failed: %s\n", sl::last_error().c_str());
    }

    sl::wait_for_graphics();
    sl::destroy_mesh(sphere);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}

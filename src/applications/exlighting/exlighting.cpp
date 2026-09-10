#include "sl.h"

#include <cmath>
#include <string_view>
#include <vector>

int main(int argc, char *argv[])
{
    bool disable_shadows = false;
    bool hard_shadows = false;
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        disable_shadows = disable_shadows || argument == "--no-shadows";
        hard_shadows = hard_shadows || argument == "--hard-shadows";
    }
    if (!sl::configure_graphics_backend_from_args(argc, argv))
    {
        return -1;
    }
    if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }

    sl::Bitmap *balloon = sl::load_bitmap("assets/textures/balloon_red.png");
    if (!balloon)
    {
        sl::shutdown();
        return -1;
    }

    sl::Bitmap *scene = sl::create_render_target(800, 600);
    sl::LightingPass lighting;
    if (!scene || !lighting.initialise())
    {
        sl::destroy_bitmap(scene);
        sl::destroy_bitmap(balloon);
        sl::shutdown();
        return -1;
    }
    lighting.set_ambient(0.18f);
    const std::vector<sl::ShadowCaster> casters = {
        sl::make_rectangle_shadow_caster(300.0f, 370.0f, 500.0f, 400.0f),
        {{{170.0f, 390.0f}, {230.0f, 320.0f}, {290.0f, 390.0f}}},
    };

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
        }

        const float angle_degrees = static_cast<float>(sl::time_ms() % 3600) * 0.1f;
        sl::begin_render_target(scene);
        sl::clear_render_target({45, 48, 56});
        sl::gprintf_center(32, {0, 255, 0}, "2D lighting - press Escape to exit");
        sl::rectfill(sl::screen, 300.0f, 370.0f, 500.0f, 400.0f, {90, 94, 104});
        sl::draw_sprite_rotated(balloon, 400.0f, 300.0f, angle_degrees);
        sl::end_render_target();

        sl::Light warm_light;
        warm_light.x = 400.0f + std::cos(static_cast<float>(sl::time_ms()) * 0.001f) * 220.0f;
        warm_light.y = 300.0f - std::sin(static_cast<float>(sl::time_ms()) * 0.001f) * 160.0f;
        warm_light.radius = 320.0f;
        warm_light.intensity = 1.4f;
        warm_light.shadow_softness = hard_shadows ? 0.0f : 3.0f;
        warm_light.colour = {255, 190, 110};

        sl::Light cool_light;
        cool_light.x = 620.0f;
        cool_light.y = 190.0f;
        cool_light.radius = 220.0f;
        cool_light.intensity = 0.65f;
        cool_light.shadow_softness = hard_shadows ? 0.0f : 2.0f;
        cool_light.colour = {100, 170, 255};

        const std::vector<sl::Light> lights = {warm_light, cool_light};
        lighting.apply(scene, lights, 0, 0, 800, 600, true,
            disable_shadows ? std::vector<sl::ShadowCaster>{} : casters);
        sl::show_video_bitmap();
        sl::end_frame();
    }

    lighting.shutdown();
    sl::destroy_bitmap(scene);
    sl::destroy_bitmap(balloon);
    sl::shutdown();
    return 0;
}

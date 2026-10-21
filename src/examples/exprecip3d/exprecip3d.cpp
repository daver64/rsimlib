#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    constexpr int window_w = 1000;
    constexpr int window_h = 640;
    constexpr int view_w = 960;
    constexpr int view_h = 540;

    struct Building
    {
        float x, z, width, depth, height;
    };

    const Building buildings[] = {{-9, -14, 6, 6, 5}, {5, -18, 8, 7, 8}, {-16, -4, 5, 9, 3.5f}, {14, -6, 6, 6, 4.5f}, {0, -7, 3, 3, 2}};

    // Rain lands on rooftops as well as the ground, so the drops need to know the height of what is below them.
    float surface_height(float x, float z)
    {
        for (const Building &b : buildings)
            if (std::fabs(x - b.x) < b.width * 0.5f && std::fabs(z - b.z) < b.depth * 0.5f)
                return b.height;
        return 0.0f;
    }
}

int main(int argc, char *argv[])
{
    sl::set_text_panel_style({true});
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, window_w, window_h))
        return -1;

    sl::Surface3D *surface = sl::create_surface_3d(view_w, view_h, 4);
    sl::Mesh *ground = sl::create_plane_mesh(160.0f, 160.0f);
    sl::Mesh *cube = sl::create_cube_mesh(1.0f);
    if (!surface || !ground || !cube)
    {
        std::fprintf(stderr, "3D surface unavailable on this backend.\n");
        sl::shutdown();
        return -1;
    }
    sl::set_fps(60);
    const bool audio = sl::audio_fx_init();

    sl::Precipitation3D precipitation;
    precipitation.set_ground(surface_height);
    precipitation.set_accumulation(0.04f, 0.03f);
    sl::PrecipitationLens lens;
    sl::PrecipitationAudio sound;

    // The clear-weather environment is kept; the weather is applied to a copy each frame.
    sl::Environment3D clear_sky;
    clear_sky.sun_direction = {-0.5f, -0.8f, -0.4f};
    clear_sky.fog_density = 0.0006f;
    clear_sky.clouds = sl::cloud_layer(sl::CloudType::cumulus);
    clear_sky.clouds.coverage = 0.3f;

    sl::PrecipitationType type = sl::PrecipitationType::rain;
    sl::PrecipitationLevel level = sl::PrecipitationLevel::moderate;
    bool lens_on = true;
    bool sound_on = audio;
    bool orbit = true;
    auto apply = [&]
    {
        sl::PrecipitationConfig target = sl::precipitation_preset(type, level);
        // Follow the clouds, stronger in heavier weather; precipitation_preset()'s pixel wind is replaced.
        target.wind = sl::Precipitation3D::wind_from_clouds(clear_sky.clouds, 0.3f + 0.9f * target.intensity);
        target.size = 0.8f + 0.5f * target.intensity;
        precipitation.weather.transition_to(target, 3.0f);
    };
    apply();

    const char *level_names[] = {"light", "moderate", "heavy", "storm"};
    const char *type_names[] = {"clear", "rain", "hail", "snow"};
    float heading = 0.15f;
    glm::vec3 position{0.0f, 2.2f, 8.0f};
    bool running = true;
    float time = 0.0f;
    float last = static_cast<float>(sl::time_ms()) * 0.001f;
    float frame_ms = 16.0f;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit)
                running = false;
            else if (event.type() == sl::Event::Type::key_down)
            {
                using Key = sl::Event::Key;
                switch (event.key())
                {
                case Key::escape:
                    running = false;
                    break;
                case Key::digit_0:
                    type = sl::PrecipitationType::none;
                    apply();
                    break;
                case Key::digit_1:
                    type = sl::PrecipitationType::rain;
                    apply();
                    break;
                case Key::digit_2:
                    type = sl::PrecipitationType::hail;
                    apply();
                    break;
                case Key::digit_3:
                    type = sl::PrecipitationType::snow;
                    apply();
                    break;
                case Key::page_up:
                    level = static_cast<sl::PrecipitationLevel>(std::min(3, static_cast<int>(level) + 1));
                    apply();
                    break;
                case Key::page_down:
                    level = static_cast<sl::PrecipitationLevel>(std::max(0, static_cast<int>(level) - 1));
                    apply();
                    break;
                case Key::letter_l:
                    lens_on = !lens_on;
                    break;
                case Key::letter_m:
                    sound_on = audio && !sound_on;
                    break;
                case Key::space:
                    orbit = !orbit;
                    break;
                default:
                    break;
                }
            }
            sl::display_handle_event(event);
        }
        const float now = static_cast<float>(sl::time_ms()) * 0.001f;
        const float dt = std::min(now - last, 0.1f);
        last = now;
        time += dt;
        frame_ms += (dt * 1000.0f - frame_ms) * 0.05f;

        if (orbit)
            heading += dt * 0.12f;
        if (sl::key_down(sl::Event::Key::arrow_left))
            heading -= dt * 1.2f;
        if (sl::key_down(sl::Event::Key::arrow_right))
            heading += dt * 1.2f;
        const glm::vec3 forward{std::sin(heading), 0.0f, -std::cos(heading)};
        if (sl::key_down(sl::Event::Key::arrow_up))
            position += forward * dt * 6.0f;
        if (sl::key_down(sl::Event::Key::arrow_down))
            position -= forward * dt * 6.0f;
        position.x = std::clamp(position.x, -50.0f, 50.0f);
        position.z = std::clamp(position.z, -50.0f, 50.0f);

        sl::Camera camera;
        camera.position = position;
        camera.target = position + forward * 10.0f + glm::vec3(0.0f, -0.6f, 0.0f);

        precipitation.update(dt, camera);
        lens.update(dt, precipitation.weather.current());
        sound.update(sound_on ? precipitation.weather.current() : sl::PrecipitationConfig{sl::PrecipitationType::none, 0.0f}, 200);

        const sl::PrecipitationConfig &weather = precipitation.weather.current();
        const float cover = precipitation.snow_cover();
        if (sl::begin_surface_3d(surface, camera))
        {
            sl::Environment3D environment = clear_sky;
            environment.camera_altitude = position.y;
            environment.camera_position = {position.x, position.y, position.z};
            environment.time = time;
            sl::precipitation_apply_environment(environment, weather);
            sl::clear_surface_3d({60, 70, 90});
            sl::set_environment_3d(environment);
            sl::draw_sky_3d();
            const float dim = 1.0f - 0.5f * weather.intensity * (weather.type == sl::PrecipitationType::none ? 0.0f : 1.0f);
            sl::set_light_3d({environment.sun_direction, 0.45f, glm::vec3(dim)});

            sl::Material material;
            material.tint = sl::snow_tinted({70, 96, 64, 255}, cover);
            sl::draw_mesh(ground, glm::mat4(1.0f), material);
            for (const Building &b : buildings)
            {
                material.tint = sl::snow_tinted({150, 120, 105, 255}, cover * 0.55f);
                sl::draw_mesh(cube, sl::model_matrix({b.x, b.height * 0.5f, b.z}, {0, 0, 0}, {b.width, b.height, b.depth}), material);
                // A white slab on each roof, as deep as the snow lying there.
                if (cover > 0.02f)
                {
                    material.tint = {246, 249, 255, 255};
                    sl::draw_mesh(cube, sl::model_matrix({b.x, b.height + cover * 0.12f, b.z}, {0, 0, 0},
                                                         {b.width + 0.2f, cover * 0.24f, b.depth + 0.2f}),
                                  material);
                }
            }
            precipitation.draw(camera);
            sl::end_surface_3d();
        }

        sl::clear_to_colour(sl::screen, {24, 26, 32});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 20, 70);
        if (lens_on)
            lens.draw(20, 70, view_w, view_h, weather);

        sl::gprintf(20, 12, {236, 240, 248}, "%s, %s  intensity %.2f  wind %.1f, %.1f m/s  snow cover %.0f%%",
                    type_names[static_cast<int>(precipitation.weather.target().type)], level_names[static_cast<int>(level)],
                    weather.intensity, weather.wind.x, weather.wind.y, cover * 100.0f);
        sl::gprintf(20, 34, {170, 180, 200}, "1 rain  2 hail  3 snow  0 clear   PgUp/PgDn strength   arrows walk/turn   Space orbit %s   L lens %s   M sound %s",
                    orbit ? "on" : "off", lens_on ? "on" : "off", sound_on ? "on" : "off");
        sl::gprintf(20, 616, {170, 180, 200}, "%d particles, %d splashes   %.1f ms/frame", precipitation.particle_count(),
                    precipitation.splash_count(), frame_ms);
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sound.release();
    precipitation.release();
    if (audio)
        sl::audio_fx_shutdown();
    sl::destroy_mesh(cube);
    sl::destroy_mesh(ground);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}

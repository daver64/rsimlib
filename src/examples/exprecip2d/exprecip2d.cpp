#include "sl.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr int window_w = 960;
    constexpr int window_h = 600;
    constexpr float ground_line = 520.0f;

    struct Block
    {
        float x, top, width;
    };

    // Rooftops and ledges the weather lands on, in world coordinates; the world repeats every `period` pixels.
    const Block blocks[] = {{80, 430, 150}, {330, 470, 110}, {560, 400, 170}, {820, 450, 130}, {1040, 480, 90}};
    constexpr float period = 1280.0f;

    float surface_at(float world_x)
    {
        const float x = std::fmod(std::fmod(world_x, period) + period, period);
        for (const Block &b : blocks)
            if (x >= b.x && x <= b.x + b.width)
                return b.top;
        return ground_line;
    }

    void draw_scene(float scroll, sl::PrecipitationType type)
    {
        const bool snow = type == sl::PrecipitationType::snow;
        for (int i = 0; i < 40; ++i)
        {
            const float t = static_cast<float>(i) / 39.0f;
            sl::rectfill(sl::screen, 0, i * 15.0f, window_w, i * 15.0f + 15.0f,
                         {static_cast<Uint8>(22 + 40 * t), static_cast<Uint8>(30 + 45 * t), static_cast<Uint8>(52 + 55 * t), 255});
        }
        // Far hills drift slower than the near ground.
        for (int i = -1; i < 12; ++i)
        {
            const float x = i * 140.0f - std::fmod(scroll * 0.3f, 140.0f);
            const float h = 70.0f + 40.0f * std::sin(i * 1.7f + std::floor(scroll * 0.3f / 140.0f));
            sl::trianglefill(sl::screen, x - 90.0f, ground_line, x + 70.0f, ground_line - h, x + 230.0f, ground_line, {34, 44, 62, 255});
        }
        sl::rectfill(sl::screen, 0, ground_line, window_w, window_h, {26, 36, 30, 255});
        for (int tile = -1; tile < 3; ++tile)
            for (const Block &b : blocks)
            {
                const float x = b.x + tile * period - scroll;
                if (x > window_w || x + b.width < 0)
                    continue;
                sl::rectfill(sl::screen, x, b.top, x + b.width, ground_line, {58, 52, 66, 255});
                sl::rectfill(sl::screen, x - 4, b.top, x + b.width + 4, b.top + 7, {88, 80, 96, 255});
                for (float wx = x + 16; wx < x + b.width - 18; wx += 30)
                    for (float wy = b.top + 22; wy < ground_line - 20; wy += 34)
                        sl::rectfill(sl::screen, wx, wy, wx + 14, wy + 18, {236, 196, 98, 255});
            }
        if (snow)
            sl::rectfill(sl::screen, 0, ground_line, window_w, ground_line + 3, {230, 236, 246, 255});
    }
}

int main(int argc, char *argv[])
{
    sl::set_text_panel_style({true});
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, window_w, window_h))
        return -1;
    sl::set_fps(60);
    const bool audio = sl::audio_fx_init();

    // The weather object keeps the particles; the lens and audio helpers are optional extras.
    sl::Precipitation2D precipitation(window_w, window_h);
    float scroll = 0.0f;
    precipitation.set_ground([&scroll](float screen_x) { return surface_at(screen_x + scroll); });
    precipitation.set_max_particles(1800);
    sl::PrecipitationLens lens;
    sl::PrecipitationAudio sound;

    sl::PrecipitationType type = sl::PrecipitationType::rain;
    sl::PrecipitationLevel level = sl::PrecipitationLevel::moderate;
    float wind_bias = 0.0f;
    bool lens_on = true;
    bool sound_on = audio;
    auto apply = [&]
    {
        sl::PrecipitationConfig target = sl::precipitation_preset(type, level);
        target.wind.x += wind_bias;
        precipitation.weather.transition_to(target, 3.0f);
    };
    apply();

    const char *level_names[] = {"light", "moderate", "heavy", "storm"};
    const char *type_names[] = {"clear", "rain", "hail", "snow"};
    bool running = true;
    float last = static_cast<float>(sl::time_ms()) * 0.001f;
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
                case Key::arrow_up:
                    level = static_cast<sl::PrecipitationLevel>(std::min(3, static_cast<int>(level) + 1));
                    apply();
                    break;
                case Key::arrow_down:
                    level = static_cast<sl::PrecipitationLevel>(std::max(0, static_cast<int>(level) - 1));
                    apply();
                    break;
                case Key::letter_q:
                    wind_bias -= 40.0f;
                    apply();
                    break;
                case Key::letter_e:
                    wind_bias += 40.0f;
                    apply();
                    break;
                case Key::letter_l:
                    lens_on = !lens_on;
                    break;
                case Key::letter_b:
                    lens.droplets = !lens.droplets;
                    break;
                case Key::letter_m:
                    sound_on = audio && !sound_on;
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

        float move = 0.0f;
        if (sl::key_down(sl::Event::Key::arrow_left) || sl::key_down(sl::Event::Key::letter_a))
            move -= 260.0f * dt;
        if (sl::key_down(sl::Event::Key::arrow_right) || sl::key_down(sl::Event::Key::letter_d))
            move += 260.0f * dt;
        scroll += move;
        precipitation.scroll(move, 0.0f);

        precipitation.update(dt);
        lens.update(dt, precipitation.weather.current());
        if (sound_on)
            sound.update(precipitation.weather.current());
        else
            sound.update({sl::PrecipitationType::none, 0.0f});

        const sl::PrecipitationConfig &now_weather = precipitation.weather.current();
        draw_scene(scroll, now_weather.type);
        precipitation.draw();
        if (lens_on)
            lens.draw(0, 0, window_w, window_h, now_weather);

        sl::gprintf(14, 12, {236, 240, 248}, "%s, %s  intensity %.2f  wind %+.0f px/s   %d particles, %d splashes",
                    type_names[static_cast<int>(precipitation.weather.target().type)], level_names[static_cast<int>(level)],
                    now_weather.intensity, now_weather.wind.x, precipitation.particle_count(), precipitation.splash_count());
        sl::gprintf(14, 32, {170, 180, 200}, "1 rain  2 hail  3 snow  0 clear   Up/Down strength   Q/E wind   Left/Right scroll   L haze %s   B droplets %s   M sound %s",
                    lens_on ? "on" : "off", lens.droplets ? "on" : "off", sound_on ? "on" : "off");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sound.release();
    if (audio)
        sl::audio_fx_shutdown();
    sl::shutdown();
    return 0;
}

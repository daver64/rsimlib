#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>

namespace
{
    struct Palette
    {
        sl::Colour planet;
        sl::Colour accent;
        sl::Colour moon;
    };

    constexpr Palette palettes[] = {
        {{65, 126, 238}, {255, 190, 108}, {105, 230, 222}},
        {{137, 92, 224}, {98, 234, 188}, {255, 177, 115}},
        {{220, 82, 125}, {255, 203, 104}, {129, 184, 255}},
    };

    void draw_stars(float width, float height, float scale, float time)
    {
        for (int i = 0; i < 56; ++i)
        {
            const float x = std::fmod(31.0f + i * 157.31f + time * (i % 3 + 1) * 2.0f, width);
            const float y = std::fmod(47.0f + i * 283.67f, height);
            const int shade = 100 + static_cast<int>(45.0f * std::sin(time * 1.7f + i * 0.83f));
            const sl::Colour colour{
                static_cast<std::uint8_t>(shade),
                static_cast<std::uint8_t>(shade + 8),
                static_cast<std::uint8_t>(std::min(190, shade + 28))};
            const float radius = (i % 9 == 0 ? 1.8f : 1.0f) * scale;
            sl::circlefill(sl::screen, x, y, radius, colour);
        }
    }
}

int main(int argc, char **argv)
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 960, 540))
    {
        std::fprintf(stderr, "Unable to initialize the Android showcase: %s\n", sl::last_error().c_str());
        return 1;
    }

    sl::set_window_title("simlib Android Showcase");
    sl::set_fps(60);

    sl::EmitterConfig sparks;
    sparks.emit_rate = 75.0f;
    sparks.particle_lifetime = 0.9f;
    sparks.min_speed = 8.0f;
    sparks.max_speed = 42.0f;
    sparks.spread_degrees = 180.0f;
    sparks.start_size = 5.0f;
    sparks.end_size = 0.5f;
    sparks.start_colour = {115, 205, 255, 220};
    sparks.end_colour = {80, 130, 255, 0};
    sparks.max_particles = 150;
    sl::ParticleEmitter trail(sparks);

    bool running = true;
    int palette_index = 0;
    int touch_count = 0;
    float time = 0.0f;

    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            else if (event.type() == sl::Event::Type::finger_down ||
                     (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::space))
            {
                palette_index = (palette_index + 1) % (sizeof(palettes) / sizeof(palettes[0]));
                ++touch_count;
            }
            sl::display_handle_event(event);
        }

        const float dt = std::min(0.05f, static_cast<float>(sl::get_frame_time()) / 1000.0f);
        time += dt;

        const float width = static_cast<float>(sl::screen->width);
        const float height = static_cast<float>(sl::screen->height);
        const float scale = std::min(width / 540.0f, height / 960.0f);
        const float center_x = width * 0.5f;
        const float center_y = height * 0.51f;
        const Palette &palette = palettes[palette_index];

        sl::clear_to_colour(sl::screen, {7, 12, 24});
        draw_stars(width, height, scale, time);

        const float inner_orbit_x = 150.0f * scale;
        const float inner_orbit_y = 106.0f * scale;
        const float outer_orbit_x = 218.0f * scale;
        const float outer_orbit_y = 154.0f * scale;
        sl::ellipse(sl::screen, center_x, center_y, inner_orbit_x, inner_orbit_y, {46, 69, 102}, 2.0f * scale);
        sl::ellipse(sl::screen, center_x, center_y, outer_orbit_x, outer_orbit_y, {46, 69, 102}, 2.0f * scale);

        const float planet_angle = time * 0.7f;
        const float planet_x = center_x + std::cos(planet_angle) * outer_orbit_x;
        const float planet_y = center_y + std::sin(planet_angle) * outer_orbit_y;
        trail.set_position(planet_x, planet_y);
        trail.update(dt);
        trail.render();

        const float moon_angle = -time * 1.05f + 1.1f;
        const float moon_x = center_x + std::cos(moon_angle) * inner_orbit_x;
        const float moon_y = center_y + std::sin(moon_angle) * inner_orbit_y;
        sl::circlefill(sl::screen, center_x, center_y, 76.0f * scale, {22, 38, 70});
        sl::circlefill(sl::screen, center_x, center_y, 57.0f * scale, palette.planet);
        sl::circlefill(sl::screen, center_x - 17.0f * scale, center_y - 19.0f * scale,
                       20.0f * scale, {183, 218, 255});
        sl::circle(sl::screen, center_x, center_y, 65.0f * scale, palette.moon, 2.0f * scale);
        sl::circlefill(sl::screen, planet_x, planet_y, 18.0f * scale, palette.accent);
        sl::circle(sl::screen, planet_x, planet_y, 23.0f * scale, {255, 231, 183}, 2.0f * scale);
        sl::circlefill(sl::screen, moon_x, moon_y, 12.0f * scale, palette.moon);
        sl::circle(sl::screen, moon_x, moon_y, 16.0f * scale, {220, 240, 255}, 1.5f * scale);

        const int margin = static_cast<int>(28.0f * scale);
        sl::gprintf(margin, margin, {236, 243, 255}, "SIMLIB");
        sl::gprintf(margin, margin + static_cast<int>(27.0f * scale), palette.accent, "ANDROID SHOWCASE");
        sl::gprintf(margin, margin + static_cast<int>(51.0f * scale), {155, 176, 210}, "GLES 3.1 / SDL2 / TOUCH");
        sl::gprintf_center(static_cast<int>(height * 0.25f), {205, 220, 245}, "ANIMATED ORBITS");

        const int panel_top = static_cast<int>(height - 150.0f * scale);
        sl::rectfill(sl::screen, margin, static_cast<float>(panel_top), width - margin, height - margin, {14, 23, 40});
        sl::rect(sl::screen, margin, static_cast<float>(panel_top), width - margin, height - margin, {48, 73, 110}, 2.0f * scale);
        sl::gprintf(margin + static_cast<int>(16.0f * scale), panel_top + static_cast<int>(22.0f * scale),
                    {226, 236, 255}, "2D SHAPES / TEXT / PARTICLES");
        sl::gprintf(margin + static_cast<int>(16.0f * scale), panel_top + static_cast<int>(52.0f * scale),
                    {164, 190, 225}, "Tap anywhere to change the palette");
        sl::gprintf(margin + static_cast<int>(16.0f * scale), panel_top + static_cast<int>(82.0f * scale),
                    palette.accent, "PALETTE %d  |  TOUCHES %d", palette_index + 1, touch_count);

        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::shutdown();
    return 0;
}

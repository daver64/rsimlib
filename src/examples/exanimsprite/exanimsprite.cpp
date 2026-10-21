// exanimsprite: animated sprites from an atlas with sl::AnimationManager.
//
// The atlas is generated at start-up, so no assets are needed. Row 0 is a pulsing orb, row 1 a glowing bar and
// row 2 an expanding explosion ring. Looping sprites run at different frame durations, one is drawn rotated, and a
// click spawns a one-shot explosion that removes itself when it finishes.
//
//   Left click  spawn an explosion      Space  pause / resume the orbs
//   R           toggle rotation of the bar    Esc  quit

#include "sl.h"

#include "animated_sprite.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
    constexpr int tile = 32;
    constexpr int frames = 16;

    sl::Bitmap *make_atlas_bitmap()
    {
        sl::Bitmap *bitmap = sl::create_bitmap(tile * frames, tile * 3);
        if (!bitmap)
            return nullptr;
        for (int f = 0; f < frames; ++f)
        {
            for (int y = 0; y < tile; ++y)
            {
                for (int x = 0; x < tile; ++x)
                {
                    const float dx = x - 15.5f, dy = y - 15.5f;
                    const float dist = std::sqrt(dx * dx + dy * dy);
                    sl::Colour out{0, 0, 0, 0};

                    // Row 0: an orb that breathes in and out.
                    const float phase = static_cast<float>(f) / frames;
                    const float radius = 9.0f + 5.0f * std::sin(phase * 6.2831853f);
                    if (dist < radius)
                    {
                        const float shade = 1.0f - dist / radius * 0.6f;
                        out = {static_cast<std::uint8_t>(80 * shade), static_cast<std::uint8_t>(200 * shade),
                               static_cast<std::uint8_t>(255 * shade), 255};
                    }
                    sl::putpixel(bitmap, f * tile + x, y, out);

                    // Row 1: a bar whose colour pulses; any turning comes from draw_rotated, so R is easy to see.
                    out = {0, 0, 0, 0};
                    if (std::fabs(dx) < 13.0f && std::fabs(dy) < 3.0f)
                    {
                        const float glow = 0.5f + 0.5f * std::sin(phase * 6.2831853f);
                        out = {255, static_cast<std::uint8_t>(90 + 140 * glow), 60, 255};
                    }
                    sl::putpixel(bitmap, f * tile + x, tile + y, out);

                    // Row 2: a ring that grows and fades.
                    const float ring = 2.0f + 14.0f * phase;
                    out = {0, 0, 0, 0};
                    if (std::fabs(dist - ring) < 2.0f)
                        out = {255, static_cast<std::uint8_t>(230 - 150 * phase), 80,
                               static_cast<std::uint8_t>(255 * (1.0f - phase))};
                    sl::putpixel(bitmap, f * tile + x, 2 * tile + y, out);
                }
            }
        }
        return bitmap;
    }
}

int main(int argc, char *argv[])
{
    sl::set_text_panel_style({true});
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 900, 600))
    {
        return -1;
    }

    sl::Bitmap *bitmap = make_atlas_bitmap();
    const sl::Atlas atlas = sl::create_atlas(bitmap, tile, tile);
    if (!bitmap || atlas.tile_count != frames * 3)
    {
        sl::destroy_bitmap(bitmap);
        return -1;
    }

    sl::AnimationManager manager;
    std::vector<int> orbs;
    // Orbs share one animation at different speeds; the first index of each row is its start tile.
    const float durations[] = {0.12f, 0.06f, 0.03f, 0.016f};
    for (int i = 0; i < 4; ++i)
    {
        sl::AnimatedSprite orb = sl::create_animated_sprite(atlas, 0, frames, 120 + i * 180, 120, 96, 96, durations[i]);
        orbs.push_back(manager.add_animated_sprite(orb));
    }

    // A rotated sprite: the bar uses its tiles from row 1 and is also turned every frame.
    sl::AnimatedSprite bar = sl::create_animated_sprite(atlas, atlas.columns, frames, 450, 360, 128, 128, 0.04f);
    bar.draw_rotated = true;
    bar.pivotx = 64.0f;
    bar.pivoty = 64.0f;
    const int bar_index = manager.add_animated_sprite(bar);

    bool spin = true;
    float angle = 0.0f; // degrees
    int explosions = 0;
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
            else if (event.type() == sl::Event::Type::key_down)
            {
                if (event.key() == sl::Event::Key::space)
                {
                    for (int index : orbs)
                        manager.toggle_pause(index);
                }
                else if (event.key() == sl::Event::Key::letter_r)
                {
                    spin = !spin;
                }
            }
            else if (event.type() == sl::Event::Type::mouse_button_down)
            {
                // One-shot: plays the ring once (counts = frames - 1 advances) and then removes itself.
                sl::AnimatedSprite boom = sl::create_animated_sprite(
                    atlas, 2 * atlas.columns, frames, sl::mouse_x() - 48, sl::mouse_y() - 48, 96, 96, 0.03f, true,
                    frames - 1);
                manager.add_animated_sprite(boom);
                ++explosions;
            }
            sl::display_handle_event(event);
        }

        if (spin)
            angle += 120.0f * sl::get_frame_time() / 1000.0f;
        manager.set_sprite_rotation(bar_index, angle);
        manager.update();

        sl::clear_to_colour(sl::screen, {20, 24, 34});
        manager.draw_sprites(sl::screen);
        sl::Font *font = sl::get_default_monospace_font();
        for (int i = 0; i < 4; ++i)
        {
            char label[48];
            std::snprintf(label, sizeof label, "%.3fs/frame", durations[i]);
            sl::textout(font, 110 + i * 180, 230, {200, 210, 230}, label);
        }
        sl::textout(font, 20, 20, {255, 255, 255},
                    "Click: explosion   Space: pause orbs   R: toggle rotation   Esc: quit");
        sl::textout(font, 20, 560, {160, 170, 190}, "Explosions spawned: " + std::to_string(explosions));
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::destroy_bitmap(bitmap);
    sl::wait_for_graphics();
    sl::shutdown();
    return 0;
}

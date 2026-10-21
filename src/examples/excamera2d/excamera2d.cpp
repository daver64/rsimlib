#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{
    constexpr float world_size = 1600.0f;

    sl::Bitmap *make_world()
    {
        sl::Bitmap *world = sl::create_bitmap(256, 256);
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
            {
                const bool dark = (x + y) % 2 == 0;
                sl::rectfill(world, x * 16.0f, y * 16.0f, x * 16.0f + 15.0f, y * 16.0f + 15.0f,
                             dark ? sl::Colour{54, 82, 70} : sl::Colour{68, 104, 86});
            }
        return world;
    }

    // An arrow-shaped ship, 32 x 32, pointing right.
    sl::Bitmap *make_ship()
    {
        sl::Bitmap *ship = sl::create_bitmap(32, 32);
        sl::clear_to_colour(ship, {0, 0, 0, 0});
        sl::triangle(ship, 2, 2, 30, 16, 2, 30, sl::Colour{240, 210, 120});
        sl::trianglefill(ship, 2, 2, 30, 16, 2, 30, sl::Colour{240, 210, 120});
        sl::circlefill(ship, 10, 16, 4, sl::Colour{60, 60, 90});
        return ship;
    }
} // namespace

int main(int argc, char *argv[])
{
    sl::set_text_panel_style({true});
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 960, 640))
    {
        return -1;
    }

    sl::Bitmap *world = make_world();
    sl::Bitmap *ship = make_ship();
    if (!world || !ship)
    {
        sl::shutdown();
        return -1;
    }

    sl::Camera2D camera;
    camera.set_bounds(0.0f, 0.0f, world_size, world_size);
    float ship_x = 800.0f, ship_y = 800.0f, heading = 0.0f;
    float angle = 0.0f;
    sl::Tween pulse;
    pulse.from = 0.8f;
    pulse.to = 1.4f;
    pulse.duration = 0.8f;
    pulse.easing = sl::Ease::sine_in_out;
    pulse.ping_pong = true;

    bool running = true;
  
    sl::set_fps(60);
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            if (event.type() == sl::Event::Type::key_down && !event.key_repeat() && event.key() == sl::Event::Key::space)
                camera.shake(14.0f, 0.5f);
            sl::display_handle_event(event);
        }

        const float dt = std::min(0.05f, static_cast<float>(sl::get_frame_time()) / 1000.0f);
        const float turn = 180.0f * dt;
        if (sl::key_down(sl::Key::arrow_left))
            heading -= turn;
        if (sl::key_down(sl::Key::arrow_right))
            heading += turn;
        const float speed = sl::key_down(sl::Key::arrow_up) ? 260.0f : 90.0f;
        ship_x += std::cos(heading * 3.14159265f / 180.0f) * speed * dt;
        ship_y += std::sin(heading * 3.14159265f / 180.0f) * speed * dt;
        ship_x = std::clamp(ship_x, 16.0f, world_size - 16.0f);
        ship_y = std::clamp(ship_y, 16.0f, world_size - 16.0f);
        if (sl::key_down(sl::Key::letter_z))
            camera.zoom = std::min(4.0f, camera.zoom * (1.0f + dt));
        if (sl::key_down(sl::Key::letter_x))
            camera.zoom = std::max(0.4f, camera.zoom / (1.0f + dt));
        if (sl::key_down(sl::Key::letter_q))
            camera.rotation -= 60.0f * dt;
        if (sl::key_down(sl::Key::letter_e))
            camera.rotation += 60.0f * dt;
        angle += 90.0f * dt;
        pulse.update(dt);
        camera.follow(ship_x, ship_y, dt, 4.0f);
        camera.update(dt);

        sl::clear_to_colour(sl::screen, {14, 18, 26});

        // Tile the world texture across the world through the camera.
        for (int ty = 0; ty < 6; ++ty)
            for (int tx = 0; tx < 6; ++tx)
            {
                sl::SpriteParams tile;
                tile.x = tx * 256.0f * 1.0f;
                tile.y = ty * 256.0f;
                tile.origin_x = 0.0f;
                tile.origin_y = 0.0f;
                camera.draw_sprite(world, tile);
            }

        // Some landmarks, plus the ship (origin at its centre).
        for (int i = 0; i < 12; ++i)
        {
            sl::SpriteParams p;
            p.x = 200.0f + i * 110.0f;
            p.y = 300.0f + std::fmod(i * 370.0f, 1000.0f);
            p.rotation = angle * (i % 2 ? 1.0f : -1.0f);
            p.scale_x = p.scale_y = pulse.value();
            p.tint = {static_cast<Uint8>(120 + i * 10), 160, 255, 255};
            camera.draw_sprite(ship, p);
        }
        sl::SpriteParams player;
        player.x = ship_x;
        player.y = ship_y;
        player.rotation = heading;
        player.scale_x = player.scale_y = 1.5f;
        camera.draw_sprite(ship, player);

        // A clipped inset: the big spinning sprite is cut off at the panel edges.
        const int px = 700, py = 20, pw = 240, ph = 160;
        sl::rectfill(sl::screen, px - 3.0f, py - 3.0f, px + pw + 2.0f, py + ph + 2.0f, sl::Colour{240, 210, 120});
        sl::rectfill(sl::screen, px, py, px + pw - 1.0f, py + ph - 1.0f, sl::Colour{20, 24, 40});
        sl::set_clip_rect(px, py, pw, ph);
        sl::SpriteParams big;
        big.x = px + pw * 0.5f;
        big.y = py + ph * 0.5f;
        big.scale_x = big.scale_y = 7.0f;
        big.rotation = angle;
        big.origin_x = 0.0f; // pivots about the top-left corner
        big.origin_y = 0.0f;
        sl::draw_sprite_ex(ship, big);
        sl::circlefill(sl::screen, px + pw - 10.0f, py + ph - 10.0f, 40.0f, sl::Colour{220, 90, 90, 200});
        sl::clear_clip_rect();

        sl::gprintf(10, 10, {255, 240, 170}, "Camera2D / clip rect / draw_sprite_ex");
        sl::gprintf(10, 34, {255, 240, 170}, "Arrows: steer  Z/X: zoom  Q/E: rotate  Space: shake");
        sl::gprintf(10, 58, {255, 240, 170}, "zoom %.2f  rotation %.0f", camera.zoom, camera.rotation);
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();


    }

    sl::destroy_bitmap(world);
    sl::destroy_bitmap(ship);
    sl::shutdown();
    return 0;
}

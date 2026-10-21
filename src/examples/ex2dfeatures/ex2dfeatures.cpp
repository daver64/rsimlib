#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{
    // A 32x32 pointy-top hexagon tile; colour is the fill.
    sl::Bitmap *make_hex_tile(sl::Colour fill)
    {
        sl::Bitmap *tile = sl::create_bitmap(32, 32);
        sl::clear_to_colour(tile, {0, 0, 0, 0});
        const float cx = 16.0f, cy = 16.0f, r = 15.5f;
        float px[6], py[6];
        for (int i = 0; i < 6; ++i)
        {
            const float a = (60.0f * i - 90.0f) * 3.14159265f / 180.0f;
            px[i] = cx + r * std::cos(a);
            py[i] = cy + r * std::sin(a);
        }
        for (int i = 0; i < 6; ++i)
            sl::trianglefill(tile, cx, cy, px[i], py[i], px[(i + 1) % 6], py[(i + 1) % 6], fill);
        return tile;
    }

    // Four 24x24 frames of a bouncing ball in one 96x24 sheet.
    sl::Bitmap *make_ball_sheet()
    {
        sl::Bitmap *sheet = sl::create_bitmap(96, 24);
        sl::clear_to_colour(sheet, {0, 0, 0, 0});
        const float heights[4] = {18.0f, 10.0f, 4.0f, 10.0f};
        for (int i = 0; i < 4; ++i)
            sl::circlefill(sheet, i * 24 + 12.0f, heights[i], 5.0f, sl::Colour{240, 120, 90});
        return sheet;
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

    sl::Font *font = sl::get_default_monospace_font();
    sl::Bitmap *ball_sheet_bitmap = make_ball_sheet();
    sl::Bitmap *hex_green = make_hex_tile({70, 140, 90});
    sl::Bitmap *hex_sand = make_hex_tile({200, 180, 110});
    sl::Bitmap *lowres = sl::create_render_target(80, 45);
    if (!font || !ball_sheet_bitmap || !hex_green || !hex_sand || !lowres)
    {
        sl::shutdown();
        return -1;
    }

    sl::SpriteSheet sheet = sl::sprite_sheet_from_atlas(sl::create_atlas(ball_sheet_bitmap, 24, 24), 0.12f);
    sheet.add_clip("bounce", 0, 3);
    sheet.add_clip("pingpong", 0, 3, true, sl::PlayDirection::ping_pong);
    sheet.add_clip("drop", 0, 2, false);
    sl::Animator animator(&sheet);
    int event_count = 0;
    animator.on_event = [&](const std::string &) { ++event_count; };
    sheet.add_event("bounce", 2, "land");
    animator.play("bounce");

    // Hex map: two tilesets stacked into one atlas-like pair would need a shared bitmap, so use one map per tile type.
    sl::TileMap *hex_map = sl::create_hex_tilemap(7, 5, 32, 32, 16, true, true);
    sl::tilemap_add_tileset(hex_map, sl::create_atlas(hex_green, 32, 32));
    const int hex_layer = sl::tilemap_add_layer(hex_map, "ground");
    for (int r = 0; r < 5; ++r)
        for (int c = 0; c < 7; ++c)
            if ((c + r) % 3 != 0)
                sl::tilemap_set(hex_map, hex_layer, c, r, 1);
    sl::TileMap *sand_map = sl::create_hex_tilemap(7, 5, 32, 32, 16, true, true);
    sl::tilemap_add_tileset(sand_map, sl::create_atlas(hex_sand, 32, 32));
    const int sand_layer = sl::tilemap_add_layer(sand_map, "ground");
    for (int r = 0; r < 5; ++r)
        for (int c = 0; c < 7; ++c)
            if ((c + r) % 3 == 0)
                sl::tilemap_set(sand_map, sand_layer, c, r, 1);

    // Infinite map: scrolls forever and only stores what was set.
    sl::TileMap *world = sl::create_infinite_tilemap(32, 32);
    sl::tilemap_add_tileset(world, sl::create_atlas(hex_green, 32, 32));
    const int world_layer = sl::tilemap_add_layer(world, "ground");
    for (int i = -40; i < 40; ++i)
        sl::tilemap_set(world, world_layer, i, (i * 7) % 5, 1);

    bool running = true;
    unsigned frame = 0;
    float t = 0.0f;
    sl::set_fps(60);
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
        const float dt = std::min(0.05f, static_cast<float>(sl::get_frame_time()) / 1000.0f);
        t += dt;
        animator.update(dt);

        sl::clear_to_colour(sl::screen, {30, 34, 48});

        // --- Text layout (top-left) ---
        sl::TextStyle wrapped;
        wrapped.wrap_width = 280;
        wrapped.align = sl::TextAlign::center;
        wrapped.outline = true;
        wrapped.shadow = true;
        wrapped.shadow_offset_x = 2;
        wrapped.shadow_offset_y = 2;
        sl::draw_text_block(font, 10, 10, {255, 240, 170},
                            "Word wrapping, centre alignment, an outline and a drop shadow, all from one call.", wrapped);
        sl::TextStyle right;
        right.wrap_width = 280;
        right.align = sl::TextAlign::right;
        right.line_spacing = 4;
        sl::draw_text_block(font, 10, 130, {255, 240, 170}, "Right aligned\nwith extra line spacing\nand explicit newlines", right);

        // --- Blend modes (top-right) ---
        const int bx = 330, by = 10;
        for (int i = 0; i < 4; ++i)
            sl::rectfill(sl::screen, bx + i * 150.0f, by + 20.0f, bx + i * 150.0f + 139.0f, by + 120.0f,
                         sl::Colour{static_cast<Uint8>(60 + i * 40), 150, static_cast<Uint8>(220 - i * 40)});
        const sl::BlendMode modes[4] = {sl::BlendMode::normal, sl::BlendMode::additive, sl::BlendMode::multiply, sl::BlendMode::screen};
        const char *names[4] = {"normal", "additive", "multiply", "screen"};
        for (int i = 0; i < 4; ++i)
        {
            sl::set_blend_mode(modes[i]);
            sl::circlefill(sl::screen, bx + i * 150.0f + 50.0f, by + 70.0f, 35.0f, sl::Colour{230, 120, 60});
            sl::circlefill(sl::screen, bx + i * 150.0f + 90.0f, by + 70.0f, 35.0f, sl::Colour{80, 200, 230});
            sl::set_blend_mode(sl::BlendMode::normal);
            sl::gprintf(bx + i * 150, by, {255, 240, 170}, "%s", names[i]);
        }

        // --- Animation clips (middle-left) ---
        sl::gprintf(10, 230, {255, 240, 170}, "Animator: clip '%s' frame %d (%d events)", animator.clip_name().c_str(),
                    animator.frame_index(), event_count);
        const char *clips[3] = {"bounce", "pingpong", "drop"};
        for (int i = 0; i < 3; ++i)
        {
            sl::Animator local(&sheet);
            local.play(clips[i]);
            local.update(std::fmod(t, 0.48f + i * 0.12f));
            sl::SpriteParams params;
            params.x = 40.0f + i * 85.0f;
            params.y = 290.0f;
            params.scale_x = params.scale_y = 2.0f;
            local.draw(ball_sheet_bitmap, params);
            sl::gprintf(10 + i * 85, 320, {255, 240, 170}, "%s", clips[i]);
        }

        // --- Hex map and infinite map (middle / bottom-left) ---
        sl::draw_tilemap(hex_map, -300.0f, -230.0f);
        sl::draw_tilemap(sand_map, -300.0f, -230.0f);
        sl::gprintf(300, 215, {255, 240, 170}, "Hexagonal tile map");

        sl::set_clip_rect(10, 380, 280, 130);
        sl::rectfill(sl::screen, 10, 380, 289, 509, sl::Colour{20, 24, 36});
        // Tiles draw in screen coordinates (map pixel = camera + screen pixel); the clip rect trims them to the panel.
        sl::draw_tilemap(world, -150.0f + std::sin(t * 0.5f) * 600.0f, -400.0f);
        sl::clear_clip_rect();
        sl::gprintf(10, 515, {255, 240, 170}, "Infinite map (negative coordinates)");
        sl::gprintf(10, 560, {255, 240, 170}, "Unicode: caf\xC3\xA9 \xC3\xB1 \xCE\x95\xCE\xBB\xCE\xBB\xCE\xB7\xCE\xBD\xCE\xB9\xCE\xBA\xCE\xAC \xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82 \xE2\x82\xAC \xE2\x86\x92");

        // --- Integer scaling of a low-resolution target (bottom-right) ---
        sl::begin_render_target(lowres);
        sl::clear_render_target({40, 60, 90});
        sl::circlefill(lowres, 40.0f + std::cos(t * 2.0f) * 25.0f, 22.0f + std::sin(t * 3.0f) * 12.0f, 8.0f, sl::Colour{250, 220, 100});
        sl::rect(lowres, 0.0f, 0.0f, 79.0f, 44.0f, sl::Colour{255, 255, 255});
        sl::end_render_target();
        sl::ScaleFit fit = sl::compute_scale_fit(80, 45, 300, 200, sl::ScaleMode::integer);
        fit.x += 620;
        fit.y += 330;
        sl::rectfill(sl::screen, 620, 330, 919, 529, sl::Colour{10, 12, 18});
        sl::draw_scaled_target(lowres, fit);
        sl::gprintf(620, 535, {255, 240, 170}, "80x45 target at %.0fx integer scale", fit.scale_x);

        sl::gprintf(10, 600, {255, 240, 170}, "2D features: text layout, blend modes, animation, hex/infinite maps, integer scale");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();

        ++frame;
        const char *frames = std::getenv("RSIMLIB_FRAMES");
        if (frames && frame >= static_cast<unsigned>(std::max(1, std::atoi(frames))))
            running = false;
        const char *screenshot = std::getenv("RSIMLIB_SCREENSHOT");
        if (screenshot && frame == 45u && !sl::save_bitmap(sl::screen, screenshot))
            std::fprintf(stderr, "Screenshot save failed: %s\n", sl::last_error().c_str());
    }

    sl::destroy_tilemap(hex_map);
    sl::destroy_tilemap(sand_map);
    sl::destroy_tilemap(world);
    sl::destroy_bitmap(ball_sheet_bitmap);
    sl::destroy_bitmap(hex_green);
    sl::destroy_bitmap(hex_sand);
    sl::destroy_bitmap(lowres);
    sl::shutdown();
    return 0;
}

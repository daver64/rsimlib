// expathfinding: tile-grid flow fields with pathfinding.h.
//
// Enemies never search. Each frame the Pathfinder rebuilds a chase distance field from the player, plus decaying
// scent (where the player has been) and sound (where the player was noisy) layers. An enemy just reads the
// neighbouring tiles: chase first, then scent, then sound.
//
//   WASD / arrows  move          Shift  move noisily (feeds the sound layer)
//   Left click     toggle a wall Space  cycle overlay: none / chase / scent / sound
//   Esc            quit

#define PATHFINDING_IMPLEMENTATION
#include "pathfinding.h"

#include "sl.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
    constexpr int columns = 32;
    constexpr int rows = 20;
    constexpr float cell = 28.0f;
    constexpr float origin_x = 8.0f;
    constexpr float origin_y = 36.0f;

    // BlockedFn is a plain function pointer, so the wall map lives at file scope.
    std::vector<unsigned char> walls(columns *rows, 0);

    bool blocked(float x, float y, float, float)
    {
        const int col = static_cast<int>(x / cell);
        const int row = static_cast<int>(y / cell);
        return col < 0 || row < 0 || col >= columns || row >= rows || walls[row * columns + col];
    }

    struct Enemy
    {
        int col, row;
        float timer = 0.0f;
        const char *mode = "idle";
        int dx = 0, dy = 0; // last step taken, drawn as a pointer
    };

    sl::Colour heat(float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return {static_cast<std::uint8_t>(255 * t), static_cast<std::uint8_t>(80 * t), static_cast<std::uint8_t>(255 * (1 - t)), 255};
    }
}

int main(int argc, char *argv[])
{
    sl::set_text_panel_style({true});
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 920, 620))
    {
        return -1;
    }

    // A few walls to make the maze interesting.
    for (int r = 4; r < 16; ++r)
        walls[r * columns + 10] = walls[r * columns + 21] = 1;
    for (int c = 10; c < 22; ++c)
        walls[4 * columns + c] = walls[15 * columns + c] = 1;
    walls[4 * columns + 15] = walls[15 * columns + 16] = 0;

    Pathfinder pathfinder;
    pathfinder.resize(columns, rows, cell, cell, blocked);

    int player_col = 3, player_row = 3;
    float move_timer = 0.0f;
    std::vector<Enemy> enemies = {{28, 17}, {28, 2}, {2, 17}, {16, 10}};
    int overlay = 0;
    const char *overlay_names[] = {"none", "chase distance", "scent", "sound"};
    bool noisy = false, moved = false;
    std::uint64_t last = sl::time_ms();
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
            else if (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::space)
            {
                overlay = (overlay + 1) % 4;
            }
            else if (event.type() == sl::Event::Type::mouse_button_down)
            {
                const int col = static_cast<int>((sl::mouse_x() - origin_x) / cell);
                const int row = static_cast<int>((sl::mouse_y() - origin_y) / cell);
                if (col >= 0 && row >= 0 && col < columns && row < rows && !(col == player_col && row == player_row))
                    walls[row * columns + col] ^= 1;
            }
            sl::display_handle_event(event);
        }

        const std::uint64_t now = sl::time_ms();
        const float dt = std::min(0.1f, static_cast<float>(now - last) / 1000.0f);
        last = now;

        // The player steps at a fixed rate so the chase is fair.
        noisy = sl::key_down(sl::Event::Key::left_shift) || sl::key_down(sl::Event::Key::right_shift);
        move_timer += dt;
        const float step_time = noisy ? 0.10f : 0.16f;
        moved = false;
        if (move_timer >= step_time)
        {
            int dx = 0, dy = 0;
            if (sl::key_down(sl::Event::Key::arrow_left) || sl::key_down(sl::Event::Key::letter_a))
                dx = -1;
            else if (sl::key_down(sl::Event::Key::arrow_right) || sl::key_down(sl::Event::Key::letter_d))
                dx = 1;
            else if (sl::key_down(sl::Event::Key::arrow_up) || sl::key_down(sl::Event::Key::letter_w))
                dy = -1;
            else if (sl::key_down(sl::Event::Key::arrow_down) || sl::key_down(sl::Event::Key::letter_s))
                dy = 1;
            if ((dx || dy) && pathfinder.open(player_col + dx, player_row + dy))
            {
                player_col += dx;
                player_row += dy;
                moved = true;
            }
            if (dx || dy)
                move_timer = 0.0f;
        }
        pathfinder.update(player_col, player_row, moved && noisy);

        for (Enemy &enemy : enemies)
        {
            enemy.timer += dt;
            if (enemy.timer < 0.22f)
                continue;
            enemy.timer = 0.0f;
            TileStep step;
            const PathLayer layers[] = {PathLayer::chase, PathLayer::scent, PathLayer::sound};
            const char *names[] = {"chase", "scent", "sound"};
            enemy.mode = "idle";
            enemy.dx = enemy.dy = 0;
            for (int i = 0; i < 3; ++i)
            {
                if (pathfinder.next_step(layers[i], enemy.col, enemy.row, step))
                {
                    enemy.col += step.dx;
                    enemy.row += step.dy;
                    enemy.dx = step.dx;
                    enemy.dy = step.dy;
                    enemy.mode = names[i];
                    break;
                }
            }
        }

        sl::clear_to_colour(sl::screen, {18, 22, 30});
        float max_chase = 1.0f;
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < columns; ++c)
                max_chase = std::max(max_chase, pathfinder.value(PathLayer::chase, c, r));

        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < columns; ++c)
            {
                const float x = origin_x + c * cell, y = origin_y + r * cell;
                sl::Colour colour{32, 38, 50};
                if (walls[r * columns + c])
                {
                    colour = {110, 116, 130};
                }
                else if (overlay == 1)
                {
                    const float d = pathfinder.value(PathLayer::chase, c, r);
                    if (d >= 0)
                        colour = heat(1.0f - d / max_chase);
                }
                else if (overlay == 2 || overlay == 3)
                {
                    const float v = pathfinder.value(overlay == 2 ? PathLayer::scent : PathLayer::sound, c, r);
                    if (v > 0.02f)
                        colour = heat(v);
                }
                sl::rectfill(sl::screen, x, y, x + cell - 1, y + cell - 1, colour);
                if (overlay == 1 && !walls[r * columns + c])
                {
                    const int d = static_cast<int>(pathfinder.value(PathLayer::chase, c, r));
                    if (d >= 0)
                        sl::textout(sl::get_default_monospace_font(), static_cast<int>(x) + 3, static_cast<int>(y) + 8,
                                    {255, 255, 255}, std::to_string(d));
                }
            }
        }
        for (const Enemy &enemy : enemies)
        {
            const float x = origin_x + (enemy.col + 0.5f) * cell, y = origin_y + (enemy.row + 0.5f) * cell;
            sl::circlefill(sl::screen, x, y, cell * 0.38f, {230, 70, 70});
            // The direction the enemy just moved.
            if (enemy.dx || enemy.dy)
                sl::line(sl::screen, x, y, x + enemy.dx * cell * 0.9f, y + enemy.dy * cell * 0.9f, {255, 255, 255}, 2.0f);
        }
        sl::circlefill(sl::screen, origin_x + (player_col + 0.5f) * cell, origin_y + (player_row + 0.5f) * cell,
                       cell * 0.38f, noisy ? sl::Colour{255, 240, 120} : sl::Colour{120, 255, 150});

        sl::Font *font = sl::get_default_monospace_font();
        sl::textout(font, 8, 8, {255, 255, 255},
                    "WASD move  Shift noisy  Click wall  Space overlay: " + std::string(overlay_names[overlay]));
        // Explain the current overlay and give a colour key.
        const char *explanation[] = {
            "Enemies (red) pick a layer each step; the white line shows where each just moved.",
            "CHASE: number = steps from the player. Enemies walk downhill. Dark = unreachable.",
            "SCENT: the player's fading trail. Used when an enemy has no chase route.",
            "SOUND: pulse while moving noisily (Shift). Spreads through open tiles only."};
        sl::Font *key_font = sl::get_default_monospace_font();
        sl::textout(key_font, 8, 24, {255, 220, 120}, explanation[overlay]);
        if (overlay != 0)
        {
            sl::textout(key_font, 700, 8, {200, 200, 200}, overlay == 1 ? "near" : "weak");
            for (int i = 0; i < 10; ++i)
                sl::rectfill(sl::screen, 745.0f + i * 10, 8, 754.0f + i * 10, 20,
                             heat(overlay == 1 ? 1.0f - i / 9.0f : i / 9.0f));
            sl::textout(key_font, 850, 8, {200, 200, 200}, overlay == 1 ? "far" : "strong");
        }
        std::string status = "Enemy modes (layer in use):";
        for (const Enemy &enemy : enemies)
            status += std::string(" ") + enemy.mode;
        sl::textout(font, 8, 600, {170, 180, 200}, status);
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::shutdown();
    return 0;
}

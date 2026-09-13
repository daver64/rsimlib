// ============================================================================
// exroguelike.cpp
//
// A coloured ASCII roguelike example for simlib.
//
// Optional/decoupled libraries:
//     rworld  - procedural overworld generation
//     recs    - entity component system
//
// ============================================================================

#include "game_types.h"
#include "game_logic.h"
#include "world_gen.h"
#include "render.h"

static bool handle_input(
    Game &game,
    const sl::Event &event)
{
    if (event.type() != sl::Event::Type::key_down)
    {
        return false;
    }

    const sl::Event::Key key = event.key();

    if (key == sl::Event::Key::arrow_left || key == sl::Event::Key::letter_h || key == sl::Event::Key::keypad_4)
        return try_player_move(game, -1, 0);

    if (key == sl::Event::Key::arrow_right || key == sl::Event::Key::letter_l || key == sl::Event::Key::keypad_6)
        return try_player_move(game, 1, 0);

    if (key == sl::Event::Key::arrow_up || key == sl::Event::Key::letter_k || key == sl::Event::Key::keypad_8)
        return try_player_move(game, 0, -1);

    if (key == sl::Event::Key::arrow_down || key == sl::Event::Key::letter_j || key == sl::Event::Key::keypad_2)
        return try_player_move(game, 0, 1);

    if (key == sl::Event::Key::letter_y || key == sl::Event::Key::keypad_7)
        return try_player_move(game, -1, -1);

    if (key == sl::Event::Key::letter_u || key == sl::Event::Key::keypad_9)
        return try_player_move(game, 1, -1);

    if (key == sl::Event::Key::letter_b || key == sl::Event::Key::keypad_1)
        return try_player_move(game, -1, 1);

    if (key == sl::Event::Key::letter_n || key == sl::Event::Key::keypad_3)
        return try_player_move(game, 1, 1);

    // Check stair / entrance keys first (>, <, comma, period when on stairs, Enter, Return, 'g', 'e')
    if (key == sl::Event::Key::greater || key == sl::Event::Key::less ||
        key == sl::Event::Key::period || key == sl::Event::Key::comma ||
        key == sl::Event::Key::return_key || key == sl::Event::Key::keypad_enter ||
        key == sl::Event::Key::letter_g || key == sl::Event::Key::letter_e)
    {
        if (use_stairs(game))
        {
            return true;
        }
    }

    // Pass / wait turn keys (period, space, keypad 5)
    if (key == sl::Event::Key::period || key == sl::Event::Key::space || key == sl::Event::Key::keypad_5)
    {
        return true;
    }

    return false;
}

static void initialise_game(
    Game &game)
{
    generate_overworld(
        game);

    update_camera(
        game);

    update_fov(
        game);

    build_lights(
        game);
}

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, SCREEN_WIDTH, SCREEN_HEIGHT))
    {
        return -1;
    }

    sl::set_window_title("simlib - ASCII Roguelike (exroguelike)");
    sl::set_fps(60);

    g_map_font = sl::open_monospace_font(24);
    if (!g_map_font)
    {
        g_map_font = sl::get_default_monospace_font();
    }

    g_ui_font = sl::open_monospace_font(12);
    if (!g_ui_font)
    {
        g_ui_font = sl::get_default_monospace_font();
    }

    sl::Bitmap *scene = sl::create_render_target(SCREEN_WIDTH, SCREEN_HEIGHT);
    sl::LightingPass lighting_pass;
    lighting_pass.initialise();
    lighting_pass.set_ambient(0.25f);

    Game game;

    initialise_game(
        game);

    bool running = true;

    while (running)
    {
        sl::Event event;

        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down &&
                 (event.key() == sl::Event::Key::escape || event.key() == sl::Event::Key::letter_q)))
            {
                running = false;
                break;
            }

            if (handle_input(game, event))
            {
                game.game_time += 0.1f;
                advance_turn(game);
                update_camera(game);
                update_fov(game);
                build_lights(game);
            }

            sl::display_handle_event(event);
        }

        if (!running)
        {
            break;
        }

        if (scene)
        {
            sl::begin_render_target(scene);
            sl::clear_render_target(sl::Colour{10, 10, 15, 255});

            render_map(scene, game);

            sl::end_render_target();

            sl::clear_to_colour(sl::screen, sl::Colour{10, 10, 15, 255});

            const float ambient = (game.area_type == AreaType::Overworld) ? (game.night ? 0.40f : 0.85f) : 0.25f;
            lighting_pass.set_ambient(ambient);

            auto lights = build_simlib_lights(game);
            auto casters = build_simlib_shadow_casters(game);
            lighting_pass.apply(scene, lights, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, casters);

            render_ui(sl::screen, game);
        }
        else
        {
            sl::clear_to_colour(sl::screen, sl::Colour{10, 10, 15, 255});
            render_game(sl::screen, game);
        }

        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();

    if (scene)
    {
        sl::destroy_bitmap(scene);
    }

    lighting_pass.shutdown();
    sl::shutdown();

    return 0;
}

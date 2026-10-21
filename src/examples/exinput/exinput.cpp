#include "sl.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace
{
    using Key = sl::Event::Key;

    struct InputState
    {
        float player_x = 400.0f;
        float player_y = 280.0f;
        sl::InputMap input;
        bool remapping = false;
    };

    void setup_bindings(sl::InputMap &input)
    {
        input.bind_key("left", Key::arrow_left);
        input.bind_key("left", Key::letter_a);
        input.bind_gamepad_axis("left", sl::GamepadAxis::left_x, -1);
        input.bind_key("right", Key::arrow_right);
        input.bind_key("right", Key::letter_d);
        input.bind_gamepad_axis("right", sl::GamepadAxis::left_x, 1);
        input.bind_key("up", Key::arrow_up);
        input.bind_key("up", Key::letter_w);
        input.bind_gamepad_axis("up", sl::GamepadAxis::left_y, -1);
        input.bind_key("down", Key::arrow_down);
        input.bind_key("down", Key::letter_s);
        input.bind_gamepad_axis("down", sl::GamepadAxis::left_y, 1);
        input.bind_gamepad_button("fire", sl::GamepadButton::south);
        input.bind_mouse("fire", sl::MouseButton::left);
        input.bind_key("fire", Key::space);
    }

    void process_input(bool &running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down &&
                 event.key() == Key::escape))
            {
                running = false;
            }
            sl::gamepad_handle_event(event);
            sl::display_handle_event(event);
        }
    }

    void update_player(InputState &state)
    {
        const float elapsed = std::min(0.05f, static_cast<float>(sl::get_frame_time()) / 1000.0f);
        state.input.update();

        // F2 rebinds "fire" to the next key, button or stick direction pressed.
        if (state.input.pressed("remap"))
        {
            state.remapping = true;
            state.input.clear_bindings("fire");
        }
        else if (state.remapping)
        {
            if (const auto captured = sl::InputMap::capture();
                captured && !captured->is_key(Key::f2))
            {
                state.input.bind("fire", *captured);
                state.remapping = false;
            }
        }

        float horizontal = 0.0f, vertical = 0.0f;
        state.input.vector("left", "right", "up", "down", horizontal, vertical);
        state.player_x = std::clamp(state.player_x + horizontal * 260.0f * elapsed, 32.0f, 768.0f);
        state.player_y = std::clamp(state.player_y + vertical * 180.0f * elapsed, 112.0f, 548.0f);
    }

    void draw_screen(const InputState &state)
    {
        sl::clear_to_colour(sl::screen, {22, 28, 38});
        const int mouse_x = sl::mouse_x();
        const int mouse_y = sl::mouse_y();
        const std::uint32_t mouse_buttons = sl::mouse_buttons();

        sl::gprintf(24, 20, {235, 220, 155}, "Input example");
        sl::gprintf(24, 48, {170, 185, 205}, "WASD / arrows or gamepad left stick: move the square (InputMap actions)");
        sl::gprintf(24, 72, {170, 185, 205}, "Mouse: %d, %d    Buttons: 0x%02X", mouse_x, mouse_y, mouse_buttons);
        sl::gprintf(24, 96, {170, 185, 205}, "Gamepads connected: %d    Escape: exit", sl::gamepad_count());
        std::string fire_keys;
        for (const sl::InputBinding &binding : state.input.bindings("fire"))
            fire_keys += (fire_keys.empty() ? "" : ", ") + sl::InputMap::describe(binding);
        sl::gprintf(24, 144, {170, 185, 205}, state.remapping ? "Press the new 'fire' input..." : "'fire' = %s    F2: remap", fire_keys.c_str());
        if (sl::gamepad_count() > 0)
            sl::gprintf(24, 120, {170, 185, 205}, "Controller: %s", sl::gamepad_name(0).c_str());

        sl::rectfill(sl::screen, state.player_x - 24.0f, state.player_y - 24.0f,
                     state.player_x + 24.0f, state.player_y + 24.0f,
                     state.input.down("fire")
                         ? sl::Colour{235, 126, 96}
                         : sl::Colour{92, 180, 226});
        sl::line(sl::screen, static_cast<float>(mouse_x) - 12.0f, static_cast<float>(mouse_y),
                 static_cast<float>(mouse_x) + 12.0f, static_cast<float>(mouse_y), {235, 220, 155});
        sl::line(sl::screen, static_cast<float>(mouse_x), static_cast<float>(mouse_y) - 12.0f,
                 static_cast<float>(mouse_x), static_cast<float>(mouse_y) + 12.0f, {235, 220, 155});

        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }
}

int main(int argc, char *argv[])
{
    sl::set_text_panel_style({true});
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }
    sl::gamepad_set_deadzone(0.15f);
    if (!sl::gamepad_init())
        sl::gprintf_center(580, {240, 130, 120}, "Gamepad subsystem unavailable");

    InputState state;
    setup_bindings(state.input);
    state.input.bind_key("remap", Key::f2);
    bool running = true;
    while (running)
    {
        process_input(running);
        update_player(state);
        draw_screen(state);
    }

    sl::gamepad_shutdown();
    sl::wait_for_graphics();
    sl::shutdown();
    return 0;
}
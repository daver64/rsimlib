#include "sl.h"

#include <algorithm>
#include <cmath>

namespace
{
    struct InputState
    {
        float player_x = 400.0f;
        float player_y = 280.0f;
    };

    void process_input(bool &running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down &&
                 event.key() == sl::Event::Key::escape))
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
        float horizontal = 0.0f;
        float vertical = 0.0f;
        if (sl::key_down(SDL_SCANCODE_LEFT) || sl::key_down(SDL_SCANCODE_A)) horizontal -= 1.0f;
        if (sl::key_down(SDL_SCANCODE_RIGHT) || sl::key_down(SDL_SCANCODE_D)) horizontal += 1.0f;
        if (sl::key_down(SDL_SCANCODE_UP) || sl::key_down(SDL_SCANCODE_W)) vertical -= 1.0f;
        if (sl::key_down(SDL_SCANCODE_DOWN) || sl::key_down(SDL_SCANCODE_S)) vertical += 1.0f;

        if (sl::gamepad_count() > 0)
        {
            horizontal += sl::gamepad_axis(0, sl::GamepadAxis::left_x);
            vertical += sl::gamepad_axis(0, sl::GamepadAxis::left_y);
        }
        const float length = std::sqrt(horizontal * horizontal + vertical * vertical);
        if (length > 1.0f)
        {
            horizontal /= length;
            vertical /= length;
        }
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
        sl::gprintf(24, 48, {170, 185, 205}, "WASD / arrows or gamepad left stick: move the square");
        sl::gprintf(24, 72, {170, 185, 205}, "Mouse: %d, %d    Buttons: 0x%02X", mouse_x, mouse_y, mouse_buttons);
        sl::gprintf(24, 96, {170, 185, 205}, "Gamepads connected: %d    Escape: exit", sl::gamepad_count());
        if (sl::gamepad_count() > 0)
            sl::gprintf(24, 120, {170, 185, 205}, "Controller: %s", sl::gamepad_name(0).c_str());

        sl::rectfill(sl::screen, state.player_x - 24.0f, state.player_y - 24.0f,
            state.player_x + 24.0f, state.player_y + 24.0f,
            sl::gamepad_count() > 0 && sl::gamepad_button(0, sl::GamepadButton::south)
                ? sl::Colour{235, 126, 96} : sl::Colour{92, 180, 226});
        sl::line(sl::screen, static_cast<float>(mouse_x) - 12.0f, static_cast<float>(mouse_y),
            static_cast<float>(mouse_x) + 12.0f, static_cast<float>(mouse_y), {235, 220, 155});
        sl::line(sl::screen, static_cast<float>(mouse_x), static_cast<float>(mouse_y) - 12.0f,
            static_cast<float>(mouse_x), static_cast<float>(mouse_y) + 12.0f, {235, 220, 155});

        sl::show_video_bitmap();
        sl::end_frame();
    }
}

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }
    sl::gamepad_set_deadzone(0.15f);
    if (!sl::gamepad_init())
        sl::gprintf_center(580, {240, 130, 120}, "Gamepad subsystem unavailable");

    InputState state;
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
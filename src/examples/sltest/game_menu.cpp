#include "game.h"

namespace game
{
    /** @brief Map numeric shortcuts and Escape to main-menu actions. */
    void handle_menu_input(const sl::Event &event)
    {

        switch (event.type())
        {
            case sl::Event::Type::key_down:
                switch (event.key())
                {
                case sl::Event::Key::digit_1:
                    request_mode(Mode::playing);
                    break;
                case sl::Event::Key::digit_2:
                    request_mode(Mode::settings);
                    break;
                case sl::Event::Key::digit_3:
                    request_mode(Mode::help);
                    break;
                case sl::Event::Key::digit_4:
                    request_mode(Mode::lua_console);
                    break;
                case sl::Event::Key::escape:
                    running = false;
                    break;
                }
                break;
        }

    }

    /** @brief Build and present the centred ImGui menu over a simlib-cleared framebuffer. */
    void update_and_render_menu()
    {
        sl::clear_to_colour(sl::screen, sl::Colour{45, 48, 56});

        sl::new_frame();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 window_size{260.0f, 250.0f};
        ImGui::SetNextWindowPos(
            {viewport->WorkPos.x + (viewport->WorkSize.x - window_size.x) * 0.5f,
             viewport->WorkPos.y + (viewport->WorkSize.y - window_size.y) * 0.5f});
        ImGui::SetNextWindowSize(window_size);

        constexpr ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        ImGui::Begin("Menu", nullptr, window_flags);
        ImGui::TextUnformatted("Menu");
        ImGui::Separator();
        if (ImGui::Button("Play", {-1.0f, 0.0f})) {
            request_mode(Mode::playing);
        }
        if (ImGui::Button("Settings", {-1.0f, 0.0f})) {
            request_mode(Mode::settings);
        }
        if (ImGui::Button("Help", {-1.0f, 0.0f})) {
            request_mode(Mode::help);
        }
        if (ImGui::Button("Lua Console", {-1.0f, 0.0f})) {
            request_mode(Mode::lua_console);
        }
        ImGui::Spacing();
        if (ImGui::Button("Quit", {-1.0f, 0.0f})) {
            running = false;
        }
        ImGui::End();
        sl::render();

        sl::show_video_bitmap();
        sl::end_frame();
    apply_mode_fade();
    }
}

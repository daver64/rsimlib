#include "game.h"

namespace game
{
    /** @brief Return to the main menu when Escape is pressed. */
    void handle_help_input(const sl::Event &event)
    {
        switch (event.type())
        {
            case sl::Event::Type::key_down:
                switch (event.key())
                {
                    case sl::Event::Key::escape:
                        request_mode(Mode::menu);
                        break;
                }
                break;
        }
    }

    /** @brief Draw the centred help window and its mouse-accessible Back action. */
    void update_and_render_help()
    {
        sl::clear_to_colour(sl::screen, sl::Colour{45, 48, 56});
        sl::new_frame();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 window_size{300.0f, 150.0f};
    apply_mode_fade();
        ImGui::SetNextWindowPos(
            {viewport->WorkPos.x + (viewport->WorkSize.x - window_size.x) * 0.5f,
             viewport->WorkPos.y + (viewport->WorkSize.y - window_size.y) * 0.5f});
        ImGui::SetNextWindowSize(window_size);
        constexpr ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        ImGui::Begin("Help", nullptr, window_flags);
        ImGui::TextUnformatted("Help");
        ImGui::Separator();
        ImGui::TextUnformatted("Press Escape to return to the menu.");
        ImGui::Spacing();
        if (ImGui::Button("Back", {-1.0f, 0.0f})) {
            request_mode(Mode::menu);
        }
        ImGui::End();
        sl::render();

        sl::show_video_bitmap();
        sl::end_frame();
    }
}

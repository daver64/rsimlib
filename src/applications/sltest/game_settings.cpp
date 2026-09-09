#include "game.h"

namespace game
{
    /** @brief Return to the main menu when Escape is pressed. */
    void handle_settings_input(const sl::Event &event)
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
    /** @brief Present the settings UI and demonstrate disabled and actionable ImGui controls. */
    void update_and_render_settings()
    {
        sl::clear_to_colour(sl::screen, sl::Colour{45, 48, 56});
        sl::new_frame();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 window_size{260.0f, 220.0f};
        ImGui::SetNextWindowPos(
            {viewport->WorkPos.x + (viewport->WorkSize.x - window_size.x) * 0.5f,
             viewport->WorkPos.y + (viewport->WorkSize.y - window_size.y) * 0.5f});
        ImGui::SetNextWindowSize(window_size);
        constexpr ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        ImGui::Begin("Settings", nullptr, window_flags);
        ImGui::TextUnformatted("Settings");
        ImGui::Separator();
        ImGui::BeginDisabled();
        ImGui::Button("Option 1", {-1.0f, 0.0f});
        ImGui::Button("Option 2", {-1.0f, 0.0f});
        ImGui::EndDisabled();
        if (ImGui::Button("Option 3", {-1.0f, 0.0f})) {
            printf("Option 3 selected\n");
        }
        
        ImGui::Spacing();
        if (ImGui::Button("Back", {-1.0f, 0.0f})) {
            request_mode(Mode::menu);
        }
        ImGui::End();
        sl::render();

        apply_mode_fade();
        sl::show_video_bitmap();
        sl::end_frame();
    }
}

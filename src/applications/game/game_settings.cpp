#include "game.h"

namespace game
{
    void handle_settings_input(SDL_Event event)
    {
        switch (event.type)
        {
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                    case SDLK_ESCAPE:
                        current_mode = Mode::menu;
                        break;
                }
                break;
        }
    }
    void update_and_render_settings()
    {
        simlib::clear_to_colour(simlib::screen, simlib::Colour{45, 48, 56});
        simlib::new_frame();

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
        ImGui::Button("Option 3", {-1.0f, 0.0f});
        ImGui::EndDisabled();
        ImGui::Spacing();
        if (ImGui::Button("Back", {-1.0f, 0.0f})) {
            current_mode = Mode::menu;
        }
        ImGui::End();
        simlib::render();

        simlib::show_video_bitmap();
        simlib::end_frame();
    }
}

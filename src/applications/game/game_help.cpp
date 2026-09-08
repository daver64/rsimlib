#include "game.h"

namespace game
{
    /** @brief Return to the main menu when Escape is pressed. */
    void handle_help_input(SDL_Event event)
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

    /** @brief Draw the centred help window and its mouse-accessible Back action. */
    void update_and_render_help()
    {
        simlib::clear_to_colour(simlib::screen, simlib::Colour{45, 48, 56});
        simlib::new_frame();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 window_size{300.0f, 150.0f};
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
            current_mode = Mode::menu;
        }
        ImGui::End();
        simlib::render();

        simlib::show_video_bitmap();
        simlib::end_frame();
    }
}

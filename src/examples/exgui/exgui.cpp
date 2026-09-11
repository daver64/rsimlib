#include "sl.h"

#include <imgui.h>

#include <algorithm>

namespace
{
    const char *backend_name(sl::GraphicsBackend backend)
    {
        switch (backend)
        {
        case sl::GraphicsBackend::opengl: return "OpenGL";
        case sl::GraphicsBackend::vulkan: return "Vulkan";
        case sl::GraphicsBackend::d3d11: return "D3D11";
        case sl::GraphicsBackend::d3d12: return "D3D12";
        }
        return "Unknown";
    }
}

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 1000, 700))
    {
        return -1;
    }

    sl::gui_init();
    bool show_demo = true;
    bool show_metrics = false;
    float panel_alpha = 1.0f;
    float swatch[4] = {0.20f, 0.60f, 0.95f, 1.0f};
    int counter = 0;
    bool running = true;

    while (running)
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
            sl::gui_handle_event(event);
            sl::display_handle_event(event);
        }

        sl::clear_to_colour(sl::screen, {18, 23, 32});
        sl::new_frame();
        ImGui::SetNextWindowBgAlpha(panel_alpha);
        ImGui::Begin("simlib GUI example", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Dear ImGui integration");
        ImGui::Separator();
        ImGui::Text("Backend: %s", backend_name(sl::graphics_backend()));
        ImGui::Text("Window: %d x %d", sl::screen_width(), sl::screen_height());
        ImGui::SliderFloat("Panel alpha", &panel_alpha, 0.25f, 1.0f);
        ImGui::ColorEdit4("Accent", swatch);
        if (ImGui::Button("Increment counter")) ++counter;
        ImGui::SameLine();
        ImGui::Text("%d", counter);
        ImGui::Checkbox("Show demo window", &show_demo);
        ImGui::Checkbox("Show metrics", &show_metrics);
        ImGui::Text("Press Escape to exit");
        ImGui::End();
        if (show_demo) sl::show_demo_window();
        if (show_metrics) ImGui::ShowMetricsWindow(&show_metrics);
        sl::render();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::gui_shutdown();
    sl::shutdown();
    return 0;
}
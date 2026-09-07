#include "gui.h"

#include "display.h"

#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_opengl2.h>

namespace simlib {

void gui_init() {
    if (!get_window() || !get_gl_context()) {
        return;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(get_window(), get_gl_context());
    ImGui_ImplOpenGL2_Init();
}

void gui_handle_event(const SDL_Event& event) {
    ImGui_ImplSDL2_ProcessEvent(&event);
}

void new_frame() {
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void render() {
    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

void gui_shutdown() {
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void show_demo_window() {
    ImGui::ShowDemoWindow();
}
} // namespace simlib

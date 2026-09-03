#pragma once

#include <SDL2/SDL_events.h>

namespace simlib {

/** Create the ImGui context and attach it to the current SDL/GL window. */
void gui_init();
/** Forward an SDL event to ImGui's input handling. */
void gui_handle_event(const SDL_Event& event);
/** Start a new ImGui frame; call before issuing any ImGui widget calls. */
void new_frame();
/** Render the ImGui draw data built up since new_frame(). */
void render();
/** Destroy the ImGui context; call before the GL context is destroyed. */
void gui_shutdown();
/** Show the ImGui demo window. */
void show_demo_window();

} // namespace simlib

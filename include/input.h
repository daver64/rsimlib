#pragma once

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_scancode.h>

#include <cstdint>

namespace simlib {

/** Poll the next SDL event, returning false when the queue is empty. */
bool poll_event(SDL_Event* event);
/** Return whether a keyboard scancode is currently held. */
bool key_down(SDL_Scancode key);
/** Return the current mouse X coordinate in window pixels. */
int mouse_x();
/** Return the current mouse Y coordinate in window pixels. */
int mouse_y();
/** Return the current mouse button bitmask. */
std::uint32_t mouse_buttons();

} // namespace simlib
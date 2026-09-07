#include "input.h"

#include <SDL2/SDL.h>

namespace simlib {

bool poll_event(SDL_Event* event) {
	return event && SDL_PollEvent(event) != 0;
}

bool key_down(SDL_Scancode key) {
	SDL_PumpEvents();
	const Uint8* state = SDL_GetKeyboardState(nullptr);
	return state && key >= 0 && key < SDL_NUM_SCANCODES && state[key] != 0;
}

int mouse_x() {
	int x = 0;
	SDL_GetMouseState(&x, nullptr);
	return x;
}

int mouse_y() {
	int y = 0;
	SDL_GetMouseState(nullptr, &y);
	return y;
}

std::uint32_t mouse_buttons() {
	return SDL_GetMouseState(nullptr, nullptr);
}

} // namespace simlib
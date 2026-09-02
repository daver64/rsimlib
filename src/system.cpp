#include "system.h"

#include <SDL2/SDL.h>

namespace simlib::system {

std::uint64_t time_ms() {
	return SDL_GetTicks64();
}

void rest(std::uint32_t milliseconds) {
	SDL_Delay(milliseconds);
}

} // namespace simlib::system
#include "input.h"

#include "display.h"

#include <SDL2/SDL.h>

namespace simlib
{

	bool key_down(SDL_Scancode key)
	{
		SDL_PumpEvents();
		const Uint8 *state = SDL_GetKeyboardState(nullptr);
		return state && key >= 0 && key < SDL_NUM_SCANCODES && state[key] != 0;
	}

	namespace
	{
		/** Scale a raw SDL window coordinate into the logical screen coordinate space used for drawing. */
		int scale_to_logical(int value, int window_size, int logical_size)
		{
			if (window_size <= 0 || logical_size <= 0)
			{
				return value;
			}
			return value * logical_size / window_size;
		}
	} // namespace

	int mouse_x()
	{
		int x = 0;
		SDL_GetMouseState(&x, nullptr);
		int window_w = 0;
		int window_h = 0;
		if (SDL_Window *window = get_window())
		{
			SDL_GetWindowSize(window, &window_w, &window_h);
		}
		return scale_to_logical(x, window_w, virtual_screen_width());
	}

	int mouse_y()
	{
		int y = 0;
		SDL_GetMouseState(nullptr, &y);
		int window_w = 0;
		int window_h = 0;
		if (SDL_Window *window = get_window())
		{
			SDL_GetWindowSize(window, &window_w, &window_h);
		}
		return scale_to_logical(y, window_h, virtual_screen_height());
	}

	std::uint32_t mouse_buttons()
	{
		return SDL_GetMouseState(nullptr, nullptr);
	}

} // namespace simlib
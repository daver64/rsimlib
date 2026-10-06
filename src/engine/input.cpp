/** @file
 * @brief Implements SDL event polling and immediate input state queries.
 */

#include "input.h"

#include "display.h"

#include <SDL2/SDL.h>

namespace sl
{

	bool key_down(SDL_Scancode key)
	{
		SDL_PumpEvents();
		const Uint8 *state = SDL_GetKeyboardState(nullptr);
		return state && key >= 0 && key < SDL_NUM_SCANCODES && state[key] != 0;
	}
	bool key_down(Event::Key key)
	{
		SDL_Scancode sdl_key=SDL_SCANCODE_UNKNOWN;
		switch(key)
		{
			case Event::Key::letter_a:
			sdl_key=SDL_SCANCODE_A;
			case Event::Key::letter_b:
			sdl_key=SDL_SCANCODE_B;
			case Event::Key::letter_c:
			sdl_key=SDL_SCANCODE_C;
			case Event::Key::letter_d:
			sdl_key=SDL_SCANCODE_D;
			case Event::Key::letter_e:
			sdl_key=SDL_SCANCODE_E;
			case Event::Key::letter_f:
			sdl_key=SDL_SCANCODE_F;
			case Event::Key::letter_g:
			sdl_key=SDL_SCANCODE_G;
			case Event::Key::letter_h:
			sdl_key=SDL_SCANCODE_H;
			case Event::Key::letter_i:
			sdl_key=SDL_SCANCODE_I;
			case Event::Key::letter_j:
			sdl_key=SDL_SCANCODE_J;
			case Event::Key::letter_k:
			sdl_key=SDL_SCANCODE_K;
			case Event::Key::letter_l:
			sdl_key=SDL_SCANCODE_L;
			case Event::Key::letter_m:
			sdl_key=SDL_SCANCODE_M;
			case Event::Key::letter_n:
			sdl_key=SDL_SCANCODE_N;
			case Event::Key::letter_o:
			sdl_key=SDL_SCANCODE_O;
			case Event::Key::letter_p:
			sdl_key=SDL_SCANCODE_P;
			case Event::Key::letter_q:
			sdl_key=SDL_SCANCODE_Q;
			case Event::Key::letter_r:
			sdl_key=SDL_SCANCODE_R;
			case Event::Key::letter_s:
			sdl_key=SDL_SCANCODE_S;
			case Event::Key::letter_t:
			sdl_key=SDL_SCANCODE_T;
			case Event::Key::letter_u:
			sdl_key=SDL_SCANCODE_U;
			case Event::Key::letter_v:
			sdl_key=SDL_SCANCODE_V;
			case Event::Key::letter_w:
			sdl_key=SDL_SCANCODE_W;
			case Event::Key::letter_x:
			sdl_key=SDL_SCANCODE_X;
			case Event::Key::letter_y:
			sdl_key=SDL_SCANCODE_Y;
			case Event::Key::letter_z:
			sdl_key=SDL_SCANCODE_Z;
			case Event::Key::comma:
			sdl_key=SDL_SCANCODE_COMMA;
			case Event::Key::f1:
			sdl_key=SDL_SCANCODE_F1;
			case Event::Key::f2:
			sdl_key=SDL_SCANCODE_F2;
			case Event::Key::f3:
			sdl_key=SDL_SCANCODE_F3;
			case Event::Key::f4:
			sdl_key=SDL_SCANCODE_F4;
			case Event::Key::f5:
			sdl_key=SDL_SCANCODE_F5;
			case Event::Key::f6:
			sdl_key=SDL_SCANCODE_F6;
			case Event::Key::f7:
			sdl_key=SDL_SCANCODE_F7;
			case Event::Key::f8:
			sdl_key=SDL_SCANCODE_F8;
			case Event::Key::f9:
			sdl_key=SDL_SCANCODE_F9;
			case Event::Key::f10:
			sdl_key=SDL_SCANCODE_F10;
			case Event::Key::f11:
			sdl_key=SDL_SCANCODE_F11;
			case Event::Key::f12:
			sdl_key=SDL_SCANCODE_F12;
			case Event::Key::digit_0:
			sdl_key=SDL_SCANCODE_0;
			case Event::Key::digit_1:
			sdl_key=SDL_SCANCODE_1;
			case Event::Key::digit_2:
			sdl_key=SDL_SCANCODE_2;
			case Event::Key::digit_3:
			sdl_key=SDL_SCANCODE_3;
			case Event::Key::digit_4:
			sdl_key=SDL_SCANCODE_4;
			case Event::Key::digit_5:
			sdl_key=SDL_SCANCODE_5;
			case Event::Key::digit_6:
			sdl_key=SDL_SCANCODE_6;
			case Event::Key::digit_7:
			sdl_key=SDL_SCANCODE_7;
			case Event::Key::digit_8:
			sdl_key=SDL_SCANCODE_8;
			case Event::Key::digit_9:
			sdl_key=SDL_SCANCODE_9;
		}
		return key_down(sdl_key);
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

} // namespace sl
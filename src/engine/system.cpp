/** @file
 * @brief Implements frame pacing, timing, and core shutdown helpers.
 */

#include "system.h"

#include "display.h"
#include "renderer.h"

#include <SDL2/SDL.h>

namespace sl
{
	namespace
	{

		int target_fps = 0;
		Uint64 last_frame_ticks = 0;
		double last_frame_time_ms = 0.0;
		bool frame_started = false;

	} // namespace

	void shutdown()
	{
		display_shutdown();
	}
	std::uint64_t time_ms()
	{
		return SDL_GetTicks64();
	}

	void rest(std::uint32_t milliseconds)
	{
		SDL_Delay(milliseconds);
	}

	void set_fps(int fps)
	{
		target_fps = fps > 0 ? fps : 0;
	}

	int get_fps()
	{
		return target_fps;
	}

	void end_frame()
	{
		Uint64 now = SDL_GetTicks64();

		// Skip the first call so the initial frame doesn't report a bogus elapsed time.
		if (!frame_started)
		{
			last_frame_ticks = now;
			frame_started = true;
			return;
		}

		if (target_fps > 0)
		{
			// Skip the software delay when the backend already paces presentation via
			// vsync: sleeping on top of that double-paces the frame and causes stutter.
			detail::Renderer *renderer = detail::active_renderer();
			if (!renderer || !renderer->vsync_active())
			{
				const Uint64 target_duration_ms = 1000 / static_cast<Uint64>(target_fps);
				const Uint64 elapsed = now - last_frame_ticks;
				if (elapsed < target_duration_ms)
				{
					SDL_Delay(static_cast<Uint32>(target_duration_ms - elapsed));
					now = SDL_GetTicks64();
				}
			}
		}

		last_frame_time_ms = static_cast<double>(now - last_frame_ticks);
		last_frame_ticks = now;
	}

	double get_frame_time()
	{
		return last_frame_time_ms;
	}

} // namespace sl
/** @file
 * @brief Implements frame pacing, timing, and core shutdown helpers.
 */

#include "system.h"

#include "display.h"
#include "font.h"
#include "frame_stats.h"
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
		FrameStats published;
		double fps_window_ms = 0.0;
		int fps_window_frames = 0;

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

	namespace detail
	{
		FrameCounters &frame_counters()
		{
			static FrameCounters counters;
			return counters;
		}
	} // namespace detail

	void end_frame()
	{
		detail::recycle_transient_instance_buffers();
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

		detail::FrameCounters &counters = detail::frame_counters();
		published.frame_time_ms = last_frame_time_ms;
		published.mesh_draws = counters.mesh_draws;
		published.instanced_draws = counters.instanced_draws;
		published.instances = counters.instances;
		published.blended_draws = counters.blended_draws;
		++published.frame_number;
		counters = detail::FrameCounters{};
		fps_window_ms += last_frame_time_ms;
		++fps_window_frames;
		if (fps_window_ms >= 500.0)
		{
			published.average_fps = fps_window_frames * 1000.0 / fps_window_ms;
			fps_window_ms = 0.0;
			fps_window_frames = 0;
		}
	}

	FrameStats get_frame_stats()
	{
		return published;
	}

	void draw_frame_stats(Font *font, int x, int y)
	{
		if (!font)
			font = get_default_monospace_font();
		if (!font)
			return;
		const FrameStats stats = get_frame_stats();
		const int line = text_height(font) + 2;
		const Colour colour = White;
		textprintf(font, x, y, colour, "%.0f fps  %.1f ms", stats.average_fps, stats.frame_time_ms);
		textprintf(font, x, y + line, colour, "meshes %u  instanced %u (%u inst)", stats.mesh_draws,
		           stats.instanced_draws, stats.instances);
		textprintf(font, x, y + 2 * line, colour, "blended %u", stats.blended_draws);
	}

	double get_frame_time()
	{
		return last_frame_time_ms;
	}

} // namespace sl
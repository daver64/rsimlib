#pragma once

/** @file
 * @brief Declares the internal shared SDL_mixer backend helpers.
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>

namespace sl::audio_detail
{

    /** Acquire and initialize the shared mixer. */
    bool acquire_mixer();
    /** Release the shared mixer. */
    void release_mixer();
    /** Install or clear the post-mix capture callback; false when the mixer is closed or not 16-bit. */
    bool set_capture(std::function<void(const std::int16_t *, std::size_t)> callback, int *rate, int *channels);
    /** Return the mutex protecting mixer operations. */
    std::mutex &mixer_mutex();

} // namespace sl::audio_detail
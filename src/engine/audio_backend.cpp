/** @file
 * @brief Provides shared SDL_mixer initialization and synchronization.
 */

#include "audio_backend.h"

namespace sl::audio_detail
{
    namespace
    {

        std::mutex backend_mutex;
        std::mutex mixer_call_mutex;
        unsigned int client_count = 0;
        bool owns_audio_subsystem = false;
        std::mutex capture_mutex;
        std::function<void(const std::int16_t *, std::size_t)> capture_callback;
        int capture_channels = 2;

        void post_mix(void *, Uint8 *stream, int length)
        {
            std::lock_guard<std::mutex> lock(capture_mutex);
            if (capture_callback && capture_channels > 0)
                capture_callback(reinterpret_cast<const std::int16_t *>(stream), static_cast<std::size_t>(length) / (sizeof(std::int16_t) * static_cast<std::size_t>(capture_channels)));
        }

    } // namespace

    bool set_capture(std::function<void(const std::int16_t *, std::size_t)> callback, int *rate, int *channels)
    {
        if (!callback)
        {
            std::lock_guard<std::mutex> lock(capture_mutex);
            capture_callback = nullptr;
            return true;
        }
        int frequency = 0, count = 0;
        Uint16 format = 0;
        if (Mix_QuerySpec(&frequency, &format, &count) == 0 || format != AUDIO_S16SYS)
            return false;
        if (rate)
            *rate = frequency;
        if (channels)
            *channels = count;
        std::lock_guard<std::mutex> lock(capture_mutex);
        capture_channels = count;
        capture_callback = std::move(callback);
        return true;
    }

    /** Acquire and initialize the process-wide SDL_mixer instance. */
    bool acquire_mixer()
    {
        std::lock_guard<std::mutex> lock(backend_mutex);
        if (client_count++ != 0)
        {
            return true;
        }

        if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0)
        {
            if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
            {
                --client_count;
                return false;
            }
            owns_audio_subsystem = true;
        }

        if (Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 1024) != 0)
        {
            if (owns_audio_subsystem)
            {
                SDL_QuitSubSystem(SDL_INIT_AUDIO);
                owns_audio_subsystem = false;
            }
            --client_count;
            return false;
        }

        Mix_AllocateChannels(32);
        Mix_SetPostMix(post_mix, nullptr);
        return true;
    }

    /** Release one reference to the process-wide SDL_mixer instance. */
    void release_mixer()
    {
        std::lock_guard<std::mutex> lock(backend_mutex);
        if (client_count == 0 || --client_count != 0)
        {
            return;
        }

        std::lock_guard<std::mutex> mixer_lock(mixer_call_mutex);
        Mix_HaltChannel(-1);
        Mix_HaltMusic();
        Mix_CloseAudio();
        if (owns_audio_subsystem)
        {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            owns_audio_subsystem = false;
        }
    }

    /** Return the mutex protecting the shared mixer. */
    std::mutex &mixer_mutex()
    {
        return mixer_call_mutex;
    }

} // namespace sl::audio_detail
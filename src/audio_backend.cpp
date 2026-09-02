#include "audio_backend.h"

namespace rvoid::audio_detail {
namespace {

std::mutex backend_mutex;
std::mutex mixer_call_mutex;
unsigned int client_count = 0;
bool owns_audio_subsystem = false;

} // namespace

bool acquire_mixer() {
    std::lock_guard<std::mutex> lock(backend_mutex);
    if (client_count++ != 0) {
        return true;
    }

    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            --client_count;
            return false;
        }
        owns_audio_subsystem = true;
    }

    if (Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 1024) != 0) {
        if (owns_audio_subsystem) {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            owns_audio_subsystem = false;
        }
        --client_count;
        return false;
    }

    Mix_AllocateChannels(32);
    return true;
}

void release_mixer() {
    std::lock_guard<std::mutex> lock(backend_mutex);
    if (client_count == 0 || --client_count != 0) {
        return;
    }

    std::lock_guard<std::mutex> mixer_lock(mixer_call_mutex);
    Mix_HaltChannel(-1);
    Mix_HaltMusic();
    Mix_CloseAudio();
    if (owns_audio_subsystem) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        owns_audio_subsystem = false;
    }
}

std::mutex& mixer_mutex() {
    return mixer_call_mutex;
}

} // namespace rvoid::audio_detail
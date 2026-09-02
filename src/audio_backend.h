#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include <mutex>

namespace rvoid::audio_detail {

bool acquire_mixer();
void release_mixer();
std::mutex& mixer_mutex();

} // namespace rvoid::audio_detail
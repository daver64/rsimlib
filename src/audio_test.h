#pragma once

#include <SDL2/SDL_events.h>

namespace audio_test {

void initialize();
void handle_event(const SDL_Event& event);
void shutdown();

} // namespace audio_test
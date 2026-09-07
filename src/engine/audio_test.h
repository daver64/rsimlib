#pragma once

#include <SDL2/SDL_events.h>

namespace audio_test {

/** Initialize the interactive audio tests. */
void initialize();
/** Handle keyboard input for the interactive audio tests. */
void handle_event(const SDL_Event& event);
/** Shut down the interactive audio tests. */
void shutdown();

} // namespace audio_test
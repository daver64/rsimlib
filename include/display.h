#pragma once

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_stdinc.h>

namespace simlib::display {

/** Select the windowed auto-detected graphics driver. */
constexpr int GFX_AUTODETECT_WINDOWED = 1;

/** Create an OpenGL window and initialize the drawing screen. */
bool set_gfx_mode(int driver, int width, int height, int virtualWidth = 0, int virtualHeight = 0);
/** Apply window-related SDL events, including resize events. */
void handle_event(const SDL_Event& event);

/** Return the current drawable width in pixels. */
int screen_width();
/** Return the current drawable height in pixels. */
int screen_height();

/** Clear the current framebuffer using an RGBA colour. */
void clear_to_colour(Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha = 255);
/** Present the current OpenGL framebuffer to the window. */
void show_video_bitmap();
/** Release the OpenGL context, window, and SDL video state. */
void shutdown();

} // namespace simlib::display
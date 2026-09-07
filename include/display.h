#pragma once

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_video.h>

namespace simlib {

/** Select the windowed auto-detected graphics driver. */
constexpr int GFX_AUTODETECT_WINDOWED = 1;

/** Create an OpenGL window and initialize the drawing screen. */
bool set_gfx_mode(int driver, int width, int height, int virtualWidth = 0, int virtualHeight = 0);
/** Apply window-related SDL events, including resize events. */
void display_handle_event(const SDL_Event& event);
/** Set the window title. */
void set_window_title(const char* title);
/** Enable or disable desktop fullscreen mode. */
bool set_fullscreen(bool enabled);
/** Return whether the display is currently fullscreen. */
bool is_fullscreen();
/** Set the OpenGL swap interval; zero disables vsync. */
bool set_vsync(bool enabled);

/** Return the current drawable width in pixels. */
int screen_width();
/** Return the current drawable height in pixels. */
int screen_height();
/** Return the configured logical screen width. */
int virtual_screen_width();
/** Return the configured logical screen height. */
int virtual_screen_height();
/** Restore the GL viewport to the actual window size (used after rendering to an offscreen target). */
void restore_window_viewport();

namespace detail {
/** Override screen_width()/screen_height() while an offscreen render target is bound; 0 clears it. */
void set_render_target_size(int width, int height);
} // namespace detail

/** Clear the current framebuffer using an RGBA colour. */
void clear_to_colour(Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha = 255);
/** Present the current OpenGL framebuffer to the window. */
void show_video_bitmap();
/** Release the OpenGL context, window, and SDL video state. */
void display_shutdown();

/** Return the active SDL window, or nullptr if not initialized. */
SDL_Window* get_window();
/** Return the active OpenGL context, or nullptr if not initialized. */
SDL_GLContext get_gl_context();

} // namespace simlib
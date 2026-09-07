#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>

#include "audio.h"
#include "display.h"
#include "draw.h"
#include "font.h"
#include "gui.h"
#include "system.h"

#include <imgui.h>
#include <cstdint>
#include <string>

#include "config.h"
#include "display.h"
#include "draw.h"
#include "error.h"
#include "font.h"
#include "graphics_fx.h"
#include "input.h"
#include "noise.h"
#include "resource.h"
#include "scene3d.h"
#include "system.h"

/** @mainpage simlib
 *
 * simlib provides a small SDL2/OpenGL rendering layer with bitmap primitives,
 * text rendering, shader effects, and SDL_mixer audio helpers.
 *
 * A typical application initializes the display with
 * simlib::set_gfx_mode(), processes SDL events, renders through
 * simlib bitmap functions, and calls simlib::display_shutdown() during cleanup.
 */
namespace simlib {

} // namespace simlib
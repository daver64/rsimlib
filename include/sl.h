#pragma once

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
 * simlib::display::set_gfx_mode(), processes SDL events, renders through
 * simlib::draw, and calls simlib::display::shutdown() during cleanup.
 */
namespace simlib {

} // namespace simlib
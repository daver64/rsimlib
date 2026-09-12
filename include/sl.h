#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>

#include <imgui.h>
#include <cstdint>
#include <string>

#include "atlas.h"
#include "audio.h"
#include "config.h"
#include "display.h"
#include "draw.h"
#include "error.h"
#include "event.h"
#include "font.h"
#include "fluid.h"
#include "graphics_fx.h"
#include "gamepad.h"
#include "gui.h"
#include "input.h"
#include "lua_canvas.h"
#include "lua_runtime.h"
#include "noise.h"
#include "physics.h"
#include "rdb.h"
#include "rdb_drivers.h"
#include "rdb_unified.h"
#include "resource.h"
#include "scene3d.h"
#include "particles.h"
#include "system.h"

/** @mainpage simlib
 *
 * simlib provides a small SDL2/OpenGL rendering layer with bitmap primitives,
 * text rendering, shader effects, and SDL_mixer audio helpers.
 *
 * A typical application initializes the display with
 * sl::set_gfx_mode(), processes SDL events, renders through
 * sl bitmap functions, and calls sl::display_shutdown() during cleanup.
 */
namespace sl
{

} // namespace sl
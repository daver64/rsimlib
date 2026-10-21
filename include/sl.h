#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>

#ifdef SIMLIB_GUI_IMGUI
#include <imgui.h>
#endif
#include <cstdint>
#include <string>

#include "atlas.h"
#include "tilemap.h"
#include "audio.h"
#include "config.h"
#include "display.h"
#include "draw.h"
#include "error.h"
#include "event.h"
#include "font.h"
#include "fluid.h"
#include "fluid3d.h"
#include "flight.h"
#include "atmosphere.h"
#include "clutter.h"
#include "terrain.h"
#include "graphics_fx.h"
#include "gamepad.h"
#include "gui.h"
#include "input.h"
#include "input_actions.h"
#include "isometric.h"
#include "camera2d.h"
#include "collision2d.h"
#include "tween.h"
#include "pixel_scale.h"
#include "animation2d.h"
#include "lua_canvas.h"
#include "lua_runtime.h"
#include "noise.h"
#include "physics.h"
#include "resource.h"
#include "scene3d.h"
#include "surface3d.h"
#include "model3d.h"
#include "voxel.h"
#include "particles.h"
#include "precipitation.h"
#include "net.h"
#include "system.h"
#include "video.h"
#include "animated_sprite.h"

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
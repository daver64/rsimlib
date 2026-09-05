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

namespace game {
    extern std::atomic<bool> running;

    void gprintf(int x, int y, simlib::Colour colour, const char *fmt, ...);
    void shutdown();
    bool handle_events();
    bool initialise();
    void update_and_render();
}
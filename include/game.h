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

    enum class Mode {
        menu,playing,paused,help,gameover,settings
    };
    extern Mode current_mode;
    extern std::atomic<bool> running;

    void gprintf(int x, int y, simlib::Colour colour, const char *fmt, ...);
    void shutdown();
    bool handle_events();
    bool initialise();
    void update_and_render();
    void update_and_render_menu();
    void update_and_render_playing();
    void update_and_render_paused();
    void update_and_render_help();
    void update_and_render_gameover();
    void update_and_render_settings();
}
#pragma once

#include "sl.h"


namespace game {

    enum class Mode {
        menu,playing,paused,help,gameover,settings,lua_console
    };
    extern Mode current_mode;
    extern std::atomic<bool> running;

    extern simlib::Bitmap* red_balloon;
    extern simlib::Bitmap* blue_balloon;
    extern simlib::Bitmap* green_balloon;

    void shutdown();
    bool handle_events();
    bool initialise();
    void update_and_render();
    bool is_running();

    /// module input functions
    void handle_menu_input(SDL_Event event);
    void handle_playing_input(SDL_Event event);
    void handle_paused_input(SDL_Event event);
    void handle_help_input(SDL_Event event);
    void handle_gameover_input(SDL_Event event);
    void handle_settings_input(SDL_Event event);
    void handle_lua_console_input(SDL_Event event);

    // module rendering functions
    void update_and_render_menu();
    void update_and_render_playing();
    void update_and_render_paused();
    void update_and_render_help();
    void update_and_render_gameover();
    void update_and_render_settings();
    void update_and_render_lua_console();
}
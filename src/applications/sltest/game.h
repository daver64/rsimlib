#pragma once

#include "sl.h"


namespace game {

    /** @brief Screens that can be selected by the sample game's state machine. */
    enum class Mode {
        menu,playing,paused,help,gameover,settings,lua_console
    };
    /** @brief The mode whose input handler and renderer receive the next frame. */
    extern Mode current_mode;
    /** @brief Set to false to end the application's main loop. */
    extern std::atomic<bool> running;

    /** @brief Balloon textures shared by the physics playground. */
    extern simlib::Bitmap* red_balloon;
    /** @brief Balloon textures shared by the physics playground. */
    extern simlib::Bitmap* blue_balloon;
    /** @brief Balloon textures shared by the physics playground. */
    extern simlib::Bitmap* green_balloon;
    /** @brief Sky texture drawn behind the physics playground. */
    extern simlib::Bitmap* playing_background;
    /** @brief Sustained sound effect played while red-balloon thrust is active. */
    extern simlib::Sample* thrust_sound;
    /** @brief Sustained sound effect played while the red-balloon burner is active. */
    extern simlib::Sample* burner_sound;

    /** @brief Release game resources before simlib tears down its graphics context. */
    void shutdown();
    /** @brief Release Lua-console text caches and engine-owned Lua canvas resources. */
    void shutdown_lua_console();
    /** @brief Poll SDL and dispatch every event to the current mode and simlib backends. */
    bool handle_events();
    /** @brief Initialise display, GUI, audio, frame pacing, and shared textures. */
    bool initialise();
    /** @brief Update and render one frame for the current mode. */
    void update_and_render();
    /** @brief Return whether the main loop should continue running. */
    bool is_running();

    /** @name Mode input handlers
    * Each receives a simlib event after the central event loop has selected the active mode.
     * @{ */
    /** @brief Handle main-menu keyboard shortcuts. */
    void handle_menu_input(const simlib::Event &event);
    /** @brief Handle gameplay controls, including reset and return-to-menu. */
    void handle_playing_input(const simlib::Event &event);
    /** @brief Handle paused-screen navigation. */
    void handle_paused_input(const simlib::Event &event);
    /** @brief Handle help-screen navigation. */
    void handle_help_input(const simlib::Event &event);
    /** @brief Handle game-over-screen navigation. */
    void handle_gameover_input(const simlib::Event &event);
    /** @brief Handle settings-screen navigation. */
    void handle_settings_input(const simlib::Event &event);
    /** @brief Handle text entry and commands for the Lua REPL. */
    void handle_lua_console_input(const simlib::Event &event);
    /** @} */

    /** @name Mode renderers
     * Each updates its own state, draws a complete frame, presents it, and applies frame pacing.
     * @{ */
    /** @brief Draw the ImGui main menu. */
    void update_and_render_menu();
    /** @brief Advance and draw the physics and particle playground. */
    void update_and_render_playing();
    /** @brief Draw the paused screen. */
    void update_and_render_paused();
    /** @brief Draw the help screen. */
    void update_and_render_help();
    /** @brief Draw the game-over screen. */
    void update_and_render_gameover();
    /** @brief Draw the settings screen. */
    void update_and_render_settings();
    /** @brief Draw the Lua canvas and console overlay. */
    void update_and_render_lua_console();
    /** @} */
}
#include "game.h"

#include <sol/sol.hpp>

#include <algorithm>
#include <deque>
#include <string>

namespace game
{
    namespace
    {
        constexpr std::size_t max_console_lines = 1000;
        constexpr const char *prompt = "> ";

        struct ConsoleLine
        {
            std::string text;
            sl::TextCache *cache = nullptr;
        };

        sl::LuaCanvas lua_canvas;
        bool console_active = false;
        std::deque<ConsoleLine> console_lines;
        sl::TextCache *input_cache = nullptr;
        std::string input_line;

        /** @brief Split Lua output into cached scrollback lines, retaining the newest 1,000. */
        void console_append(const std::string &text)
        {
            std::size_t start = 0;
            while (true)
            {
                const std::size_t position = text.find('\n', start);
                const std::string line = position == std::string::npos ? text.substr(start) : text.substr(start, position - start);
                console_lines.push_back({line, sl::create_text_cache()});
                if (position == std::string::npos) break;
                start = position + 1;
            }
            while (console_lines.size() > max_console_lines)
            {
                sl::destroy_text_cache(console_lines.front().cache);
                console_lines.pop_front();
            }
        }

        /** @brief Initialise the engine Lua canvas and attach the console-only quit binding. */
        void console_ensure_lua_initialised()
        {
            if (lua_canvas.is_initialised()) return;
            lua_canvas.initialise(console_append);
            sol::table app = lua_canvas.runtime().state()["app"];
            app.set_function("quit", []() { running = false; });
            lua_canvas.runtime().state()["quit"] = app["quit"];
            console_append("Lua 5.4 console. Press ESC to return to the menu.");
        }

        /** @brief Echo and execute the pending command, forwarding script errors to scrollback. */
        void console_execute_input()
        {
            console_append(std::string(prompt) + input_line);
            if (!input_line.empty())
            {
                const sl::LuaScriptResult result = lua_canvas.run_text(input_line);
                if (!result.success) console_append(std::string("Error: ") + result.error);
            }
            input_line.clear();
        }
    }

    /**
     * @brief Convert SDL text and key events into console input.
     *
     * Escape stops SDL text input and leaves the console, Enter executes the current command,
     * and Backspace removes one byte from the pending UTF-8 input string.
     */
    void handle_lua_console_input(const sl::Event &event)
    {
        if (event.type() == sl::Event::Type::text_input)
        {
            input_line += event.text();
        }
        else if (event.type() == sl::Event::Type::key_down && !event.key_repeat())
        {
            switch (event.key())
            {
            case sl::Event::Key::escape:
                SDL_StopTextInput();
                console_active = false;
                request_mode(Mode::menu);
                break;
            case sl::Event::Key::return_key:
            case sl::Event::Key::keypad_enter:
                console_execute_input();
                break;
            case sl::Event::Key::backspace:
                if (!input_line.empty()) input_line.pop_back();
                break;
            }
        }
    }

    /**
     * @brief Render the persistent Lua canvas first, then overlay cached console scrollback.
     *
     * The first call starts SDL text input and initialises the Lua runtime; subsequent calls
     * replay engine-owned drawing and sprite commands before drawing the REPL prompt.
     */
    void update_and_render_lua_console()
    {
        if (!console_active)
        {
            console_active = true;
            console_ensure_lua_initialised();
            if (!input_cache) input_cache = sl::create_text_cache();
            SDL_StartTextInput();
        }

        sl::Font *font = sl::get_default_monospace_font();
        const int fontheight = sl::text_height(font);
        const sl::Colour text_colour{0, 255, 0};
        lua_canvas.render(sl::screen);

        const int margin = 1;
        const int visible_rows = std::max(1, sl::screen->height / fontheight - 1);
        const std::size_t total_lines = console_lines.size();
        const std::size_t first_line = total_lines > static_cast<std::size_t>(visible_rows)
            ? total_lines - static_cast<std::size_t>(visible_rows) : 0;

        int y = margin;
        for (std::size_t index = first_line; index < total_lines; ++index)
        {
            sl::textout_cached(console_lines[index].cache, font, margin, y, text_colour, console_lines[index].text);
            y += fontheight;
        }
        sl::textout_cached(input_cache, font, margin, y, text_colour, prompt + input_line + "_");
        apply_mode_fade();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    /** @brief Destroy Lua canvas GPU resources and cached console text before display shutdown. */
    void shutdown_lua_console()
    {
        lua_canvas.reset();
        for (ConsoleLine &line : console_lines) sl::destroy_text_cache(line.cache);
        console_lines.clear();
        sl::destroy_text_cache(input_cache);
        input_cache = nullptr;
    }
}

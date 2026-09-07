#include "game.h"

#include <sol/sol.hpp>

#include <algorithm>
#include <ctime>
#include <deque>
#include <string>

namespace game
{
    namespace
    {
        constexpr std::size_t max_console_lines = 1000;
        constexpr const char *prompt = "> ";

        // one line of scrollback plus its cached GPU texture, rebuilt only
        // when the line's text changes rather than on every frame
        struct ConsoleLine
        {
            std::string text;
            simlib::TextCache *cache = nullptr;
        };

        sol::state lua_state;
        bool lua_initialised = false;
        bool console_active = false;
        std::deque<ConsoleLine> console_lines;
        simlib::TextCache *input_cache = nullptr;
        std::string input_line;

        void console_append(const std::string &text)
        {
            std::size_t start = 0;
            while (true)
            {
                const std::size_t pos = text.find('\n', start);
                const std::string line = pos == std::string::npos
                    ? text.substr(start)
                    : text.substr(start, pos - start);
                console_lines.push_back(ConsoleLine{line, simlib::create_text_cache()});
                if (pos == std::string::npos)
                {
                    break;
                }
                start = pos + 1;
            }
            while (console_lines.size() > max_console_lines)
            {
                simlib::destroy_text_cache(console_lines.front().cache);
                console_lines.pop_front();
            }
        }

        void console_ensure_lua_initialised()
        {
            if (lua_initialised)
            {
                return;
            }
            lua_initialised = true;
            // sandboxed: no io/package/debug/os libraries, so scripts cannot touch
            // the filesystem or spawn processes beyond what we explicitly expose
            lua_state.open_libraries(
                sol::lib::base, sol::lib::string, sol::lib::math,
                sol::lib::table);
            lua_state["dofile"] = sol::nil;
            lua_state["loadfile"] = sol::nil;
            lua_state["load"] = sol::nil;

            sol::table os_table = lua_state.create_named_table("os");
            os_table.set_function("time", []() { return static_cast<lua_Integer>(std::time(nullptr)); });
            os_table.set_function("clock", []() { return static_cast<double>(std::clock()) / CLOCKS_PER_SEC; });

            // future app/library bindings must be attached under this table,
            // never to globals or to real io/os/package libraries
            sol::table app_table = lua_state.create_named_table("app");
            app_table.set_function("quit", []() { running = false; });
            lua_state["quit"] = app_table["quit"];

            lua_state.set_function("print", [](sol::this_state ts, sol::variadic_args args)
            {
                // convert in place with luaL_tolstring rather than calling back into
                // the global tostring: a nested call would shift the stack indices
                // that "args" points at and corrupt sibling arguments
                lua_State *L = ts;
                std::string line;
                for (auto arg : args)
                {
                    if (!line.empty())
                    {
                        line += '\t';
                    }
                    std::size_t length = 0;
                    const char *text = luaL_tolstring(L, arg.stack_index(), &length);
                    line.append(text, length);
                    lua_pop(L, 1);
                }
                console_append(line);
            });
            console_append("Lua 5.4 console. Press ESC to return to the menu.");
        }

        void console_execute_input()
        {
            console_append(std::string(prompt) + input_line);
            if (!input_line.empty())
            {
                sol::protected_function_result result =
                    lua_state.safe_script(input_line, sol::script_pass_on_error);
                if (!result.valid())
                {
                    sol::error error = result;
                    // sol always appends "stack traceback: ..." to the message; a
                    // console error only needs the first line
                    std::string message = error.what();
                    const std::size_t traceback_pos = message.find("\nstack traceback:");
                    if (traceback_pos != std::string::npos)
                    {
                        message.resize(traceback_pos);
                    }
                    console_append(std::string("Error: ") + message);
                }
            }
            input_line.clear();
        }
    }

    void handle_lua_console_input(SDL_Event event)
    {
        switch (event.type)
        {
            case SDL_TEXTINPUT:
                input_line += event.text.text;
                break;
            case SDL_KEYDOWN:
                // ignore OS auto-repeat: held Enter/Backspace would otherwise
                // resubmit the (already-cleared) line or delete repeatedly
                if (event.key.repeat)
                {
                    break;
                }
                switch (event.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                    SDL_StopTextInput();
                    console_active = false;
                    current_mode = Mode::menu;
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    console_execute_input();
                    break;
                case SDLK_BACKSPACE:
                    if (!input_line.empty())
                    {
                        input_line.pop_back();
                    }
                    break;
                }
                break;
        }
    }

    void update_and_render_lua_console()
    {
        if (!console_active)
        {
            console_active = true;
            console_ensure_lua_initialised();
            if (!input_cache)
            {
                input_cache = simlib::create_text_cache();
            }
            SDL_StartTextInput();
        }

        simlib::Font *font = simlib::get_default_monospace_font();
        const int fontheight = simlib::text_height(font);
        simlib::Colour text_colour{0, 255, 0};
        simlib::clear_to_colour(simlib::screen, simlib::Colour{0, 0, 0});

        const int margin = 1;
        const int visible_rows =
            std::max(1, simlib::screen->height / fontheight - 1);
        const std::size_t total_lines = console_lines.size();
        const std::size_t first_line =
            total_lines > static_cast<std::size_t>(visible_rows)
                ? total_lines - static_cast<std::size_t>(visible_rows)
                : 0;

        int y = margin;
        for (std::size_t i = first_line; i < total_lines; ++i)
        {
            simlib::textout_cached(console_lines[i].cache, font, margin, y, text_colour, console_lines[i].text);
            y += fontheight;
        }
        simlib::textout_cached(input_cache, font, margin, y, text_colour, prompt + input_line + "_");

        simlib::show_video_bitmap();
        simlib::end_frame();
    }
}

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

        sol::state lua_state;
        bool lua_initialised = false;
        bool console_active = false;
        std::deque<std::string> console_lines;
        std::string input_line;

        void console_append(const std::string &text)
        {
            std::size_t start = 0;
            while (true)
            {
                const std::size_t pos = text.find('\n', start);
                if (pos == std::string::npos)
                {
                    console_lines.push_back(text.substr(start));
                    break;
                }
                console_lines.push_back(text.substr(start, pos - start));
                start = pos + 1;
            }
            while (console_lines.size() > max_console_lines)
            {
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

            lua_state.set_function("print", [](sol::variadic_args args)
            {
                std::string line;
                for (auto arg : args)
                {
                    if (!line.empty())
                    {
                        line += '\t';
                    }
                    line += lua_state["tostring"](arg).get<std::string>();
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
                    lua_state.script(input_line, sol::script_pass_on_error);
                if (!result.valid())
                {
                    sol::error error = result;
                    console_append(std::string("Error: ") + error.what());
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
            gprintf(margin, y, text_colour, "%s", console_lines[i].c_str());
            y += fontheight;
        }
        gprintf(margin, y, text_colour, "%s%s_", prompt, input_line.c_str());

        simlib::show_video_bitmap();
        simlib::end_frame();
    }
}

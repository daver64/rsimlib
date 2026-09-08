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
            simlib::TextCache *cache = nullptr;
        };

        simlib::LuaCanvas lua_canvas;
        bool console_active = false;
        std::deque<ConsoleLine> console_lines;
        simlib::TextCache *input_cache = nullptr;
        std::string input_line;

        void console_append(const std::string &text)
        {
            std::size_t start = 0;
            while (true)
            {
                const std::size_t position = text.find('\n', start);
                const std::string line = position == std::string::npos ? text.substr(start) : text.substr(start, position - start);
                console_lines.push_back({line, simlib::create_text_cache()});
                if (position == std::string::npos) break;
                start = position + 1;
            }
            while (console_lines.size() > max_console_lines)
            {
                simlib::destroy_text_cache(console_lines.front().cache);
                console_lines.pop_front();
            }
        }

        void console_ensure_lua_initialised()
        {
            if (lua_canvas.is_initialised()) return;
            lua_canvas.initialise(console_append);
            sol::table app = lua_canvas.runtime().state()["app"];
            app.set_function("quit", []() { running = false; });
            lua_canvas.runtime().state()["quit"] = app["quit"];
            console_append("Lua 5.4 console. Press ESC to return to the menu.");
        }

        void console_execute_input()
        {
            console_append(std::string(prompt) + input_line);
            if (!input_line.empty())
            {
                const simlib::LuaScriptResult result = lua_canvas.run_text(input_line);
                if (!result.success) console_append(std::string("Error: ") + result.error);
            }
            input_line.clear();
        }
    }

    void handle_lua_console_input(SDL_Event event)
    {
        if (event.type == SDL_TEXTINPUT)
        {
            input_line += event.text.text;
        }
        else if (event.type == SDL_KEYDOWN && !event.key.repeat)
        {
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
                if (!input_line.empty()) input_line.pop_back();
                break;
            }
        }
    }

    void update_and_render_lua_console()
    {
        if (!console_active)
        {
            console_active = true;
            console_ensure_lua_initialised();
            if (!input_cache) input_cache = simlib::create_text_cache();
            SDL_StartTextInput();
        }

        simlib::Font *font = simlib::get_default_monospace_font();
        const int fontheight = simlib::text_height(font);
        const simlib::Colour text_colour{0, 255, 0};
        lua_canvas.render(simlib::screen);

        const int margin = 1;
        const int visible_rows = std::max(1, simlib::screen->height / fontheight - 1);
        const std::size_t total_lines = console_lines.size();
        const std::size_t first_line = total_lines > static_cast<std::size_t>(visible_rows)
            ? total_lines - static_cast<std::size_t>(visible_rows) : 0;

        int y = margin;
        for (std::size_t index = first_line; index < total_lines; ++index)
        {
            simlib::textout_cached(console_lines[index].cache, font, margin, y, text_colour, console_lines[index].text);
            y += fontheight;
        }
        simlib::textout_cached(input_cache, font, margin, y, text_colour, prompt + input_line + "_");
        simlib::show_video_bitmap();
        simlib::end_frame();
    }

    void shutdown_lua_console()
    {
        lua_canvas.reset();
        for (ConsoleLine &line : console_lines) simlib::destroy_text_cache(line.cache);
        console_lines.clear();
        simlib::destroy_text_cache(input_cache);
        input_cache = nullptr;
    }
}

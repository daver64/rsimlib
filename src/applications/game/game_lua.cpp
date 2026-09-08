#include "game.h"

#include <sol/sol.hpp>

#include <algorithm>
#include <ctime>
#include <deque>
#include <filesystem>
#include <string>
#include <unordered_map>

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

        enum class DrawingType
        {
            pixel,
            line,
            circle,
            circlefill,
            rect,
            rectfill,
            ellipse,
            ellipsefill,
            triangle,
            trianglefill,
            sprite,
            sprite_stretched
        };

        struct DrawingCommand
        {
            DrawingType type;
            float x1 = 0.0f;
            float y1 = 0.0f;
            float x2 = 0.0f;
            float y2 = 0.0f;
            float x3 = 0.0f;
            float y3 = 0.0f;
            simlib::Colour colour;
            std::string sprite_id;
        };

        sol::state lua_state;
        bool lua_initialised = false;
        bool console_active = false;
        std::deque<ConsoleLine> console_lines;
        std::vector<DrawingCommand> drawing_commands;
        std::unordered_map<std::string, simlib::Bitmap *> sprites;
        simlib::TextCache *input_cache = nullptr;
        std::string input_line;

        simlib::Colour make_colour(int red, int green, int blue, sol::optional<int> alpha)
        {
            return {
                static_cast<Uint8>(std::clamp(red, 0, 255)),
                static_cast<Uint8>(std::clamp(green, 0, 255)),
                static_cast<Uint8>(std::clamp(blue, 0, 255)),
                static_cast<Uint8>(std::clamp(alpha.value_or(255), 0, 255))};
        }

        void add_drawing(
            DrawingType type, float x1, float y1, float x2, float y2,
            float x3, float y3, simlib::Colour colour)
        {
            drawing_commands.push_back({type, x1, y1, x2, y2, x3, y3, colour});
        }

        bool is_asset_relative_path(const std::string &path)
        {
            const std::filesystem::path asset_path{path};
            if (path.empty() || asset_path.is_absolute())
            {
                return false;
            }
            for (const std::filesystem::path &component : asset_path)
            {
                if (component == "..")
                {
                    return false;
                }
            }
            return true;
        }

        std::pair<bool, std::string> load_sprite(const std::string &id, const std::string &path)
        {
            if (id.empty())
            {
                return {false, "Sprite ID must not be empty."};
            }
            if (!is_asset_relative_path(path))
            {
                return {false, "Sprite paths must be relative to assets/."};
            }
            if (sprites.find(id) != sprites.end())
            {
                return {false, "A sprite with that ID is already loaded."};
            }

            simlib::Bitmap *sprite = simlib::load_bitmap((std::filesystem::path{"assets"} / path).string());
            if (!sprite)
            {
                return {false, "Unable to load sprite."};
            }
            sprites.emplace(id, sprite);
            return {true, {}};
        }

        bool add_sprite_drawing(const std::string &id, float x, float y, float width = 0.0f, float height = 0.0f)
        {
            if (sprites.find(id) == sprites.end())
            {
                return false;
            }
            drawing_commands.push_back({
                width == 0.0f && height == 0.0f ? DrawingType::sprite : DrawingType::sprite_stretched,
                x, y, width, height, 0.0f, 0.0f, {}, id});
            return true;
        }

        void remove_sprite_drawings(const std::string &id)
        {
            drawing_commands.erase(
                std::remove_if(
                    drawing_commands.begin(), drawing_commands.end(),
                    [&id](const DrawingCommand &command)
                    {
                        return (command.type == DrawingType::sprite || command.type == DrawingType::sprite_stretched) &&
                               command.sprite_id == id;
                    }),
                drawing_commands.end());
        }

        bool unload_sprite(const std::string &id)
        {
            const auto sprite = sprites.find(id);
            if (sprite == sprites.end())
            {
                return false;
            }
            remove_sprite_drawings(id);
            simlib::destroy_bitmap(sprite->second);
            sprites.erase(sprite);
            return true;
        }

        void clear_sprites()
        {
            drawing_commands.erase(
                std::remove_if(
                    drawing_commands.begin(), drawing_commands.end(),
                    [](const DrawingCommand &command)
                    {
                        return command.type == DrawingType::sprite || command.type == DrawingType::sprite_stretched;
                    }),
                drawing_commands.end());
            for (const auto &[id, sprite] : sprites)
            {
                simlib::destroy_bitmap(sprite);
            }
            sprites.clear();
        }

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
            app_table.set_function("load_sprite", load_sprite);
            app_table.set_function(
                "sprite",
                [](const std::string &id, float x, float y)
                {
                    return add_sprite_drawing(id, x, y);
                });
            app_table.set_function(
                "sprite_stretched",
                [](const std::string &id, float x, float y, float width, float height)
                {
                    return add_sprite_drawing(id, x, y, width, height);
                });
            app_table.set_function("unload_sprite", unload_sprite);
            app_table.set_function("clear_sprites", clear_sprites);
            app_table.set_function(
                "pixel",
                [](float x, float y, int red, int green, int blue, sol::optional<int> alpha)
                {
                    add_drawing(DrawingType::pixel, x, y, 0.0f, 0.0f, 0.0f, 0.0f, make_colour(red, green, blue, alpha));
                });
            app_table.set_function(
                "line",
                [](float x1, float y1, float x2, float y2, int red, int green, int blue, sol::optional<int> alpha)
                {
                    add_drawing(DrawingType::line, x1, y1, x2, y2, 0.0f, 0.0f, make_colour(red, green, blue, alpha));
                });
            app_table.set_function(
                "circle",
                [](float x, float y, float radius, int red, int green, int blue, sol::optional<int> alpha)
                {
                    add_drawing(DrawingType::circle, x, y, radius, 0.0f, 0.0f, 0.0f, make_colour(red, green, blue, alpha));
                });
            app_table.set_function(
                "circlefill",
                [](float x, float y, float radius, int red, int green, int blue, sol::optional<int> alpha)
                {
                    add_drawing(DrawingType::circlefill, x, y, radius, 0.0f, 0.0f, 0.0f, make_colour(red, green, blue, alpha));
                });
            app_table.set_function(
                "rect",
                [](float left, float top, float right, float bottom, int red, int green, int blue, sol::optional<int> alpha)
                {
                    add_drawing(DrawingType::rect, left, top, right, bottom, 0.0f, 0.0f, make_colour(red, green, blue, alpha));
                });
            app_table.set_function(
                "rectfill",
                [](float left, float top, float right, float bottom, int red, int green, int blue, sol::optional<int> alpha)
                {
                    add_drawing(DrawingType::rectfill, left, top, right, bottom, 0.0f, 0.0f, make_colour(red, green, blue, alpha));
                });
            app_table.set_function(
                "ellipse",
                [](float x, float y, float radius_x, float radius_y, int red, int green, int blue, sol::optional<int> alpha)
                {
                    add_drawing(DrawingType::ellipse, x, y, radius_x, radius_y, 0.0f, 0.0f, make_colour(red, green, blue, alpha));
                });
            app_table.set_function(
                "ellipsefill",
                [](float x, float y, float radius_x, float radius_y, int red, int green, int blue, sol::optional<int> alpha)
                {
                    add_drawing(DrawingType::ellipsefill, x, y, radius_x, radius_y, 0.0f, 0.0f, make_colour(red, green, blue, alpha));
                });
            app_table.set_function(
                "triangle",
                [](float x1, float y1, float x2, float y2, float x3, float y3, int red, int green, int blue, sol::optional<int> alpha)
                {
                    add_drawing(DrawingType::triangle, x1, y1, x2, y2, x3, y3, make_colour(red, green, blue, alpha));
                });
            app_table.set_function(
                "trianglefill",
                [](float x1, float y1, float x2, float y2, float x3, float y3, int red, int green, int blue, sol::optional<int> alpha)
                {
                    add_drawing(DrawingType::trianglefill, x1, y1, x2, y2, x3, y3, make_colour(red, green, blue, alpha));
                });
            app_table.set_function("clear_drawings", []() { drawing_commands.clear(); });
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

        for (const DrawingCommand &command : drawing_commands)
        {
            switch (command.type)
            {
            case DrawingType::pixel:
                simlib::putpixel(simlib::screen, static_cast<int>(command.x1), static_cast<int>(command.y1), command.colour);
                break;
            case DrawingType::line:
                simlib::line(simlib::screen, command.x1, command.y1, command.x2, command.y2, command.colour);
                break;
            case DrawingType::circle:
                simlib::circle(simlib::screen, command.x1, command.y1, command.x2, command.colour);
                break;
            case DrawingType::circlefill:
                simlib::circlefill(simlib::screen, command.x1, command.y1, command.x2, command.colour);
                break;
            case DrawingType::rect:
                simlib::rect(simlib::screen, command.x1, command.y1, command.x2, command.y2, command.colour);
                break;
            case DrawingType::rectfill:
                simlib::rectfill(simlib::screen, command.x1, command.y1, command.x2, command.y2, command.colour);
                break;
            case DrawingType::ellipse:
                simlib::ellipse(simlib::screen, command.x1, command.y1, command.x2, command.y2, command.colour);
                break;
            case DrawingType::ellipsefill:
                simlib::ellipsefill(simlib::screen, command.x1, command.y1, command.x2, command.y2, command.colour);
                break;
            case DrawingType::triangle:
                simlib::triangle(simlib::screen, command.x1, command.y1, command.x2, command.y2, command.x3, command.y3, command.colour);
                break;
            case DrawingType::trianglefill:
                simlib::trianglefill(simlib::screen, command.x1, command.y1, command.x2, command.y2, command.x3, command.y3, command.colour);
                break;
            case DrawingType::sprite:
            {
                const auto sprite = sprites.find(command.sprite_id);
                if (sprite != sprites.end())
                {
                    simlib::draw_sprite(sprite->second, command.x1, command.y1);
                }
                break;
            }
            case DrawingType::sprite_stretched:
            {
                const auto sprite = sprites.find(command.sprite_id);
                if (sprite != sprites.end())
                {
                    simlib::draw_sprite_stretched(
                        sprite->second, command.x1, command.y1,
                        static_cast<int>(command.x2), static_cast<int>(command.y2));
                }
                break;
            }
            }
        }

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

    void shutdown_lua_console()
    {
        clear_sprites();
        drawing_commands.clear();
        for (ConsoleLine &line : console_lines)
        {
            simlib::destroy_text_cache(line.cache);
        }
        console_lines.clear();
        simlib::destroy_text_cache(input_cache);
        input_cache = nullptr;
        lua_state = sol::state{};
        lua_initialised = false;
    }
}

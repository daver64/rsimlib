#include "lua_canvas.h"

#include <sol/sol.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace simlib
{
    namespace
    {
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
            sprite_stretched,
            sprite_rotated,
            sprite_rotated_stretched
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
            Colour colour{};
            std::string sprite_id;
        };

        Colour make_colour(int red, int green, int blue, sol::optional<int> alpha)
        {
            return {
                static_cast<Uint8>(std::clamp(red, 0, 255)),
                static_cast<Uint8>(std::clamp(green, 0, 255)),
                static_cast<Uint8>(std::clamp(blue, 0, 255)),
                static_cast<Uint8>(std::clamp(alpha.value_or(255), 0, 255))};
        }
    }

    struct LuaCanvas::Implementation
    {
        LuaRuntime runtime;
        Colour background{0, 0, 0};
        std::vector<DrawingCommand> commands;
        std::filesystem::path asset_root{"assets"};
        std::unordered_map<std::string, Bitmap *> sprites;

        void add_drawing(
            DrawingType type, float x1, float y1, float x2, float y2,
            float x3, float y3, Colour colour)
        {
            commands.push_back({type, x1, y1, x2, y2, x3, y3, colour});
        }

        bool load_sprite(const std::string &id, const std::string &path)
        {
            const std::filesystem::path asset_path{path};
            if (id.empty() || path.empty() || asset_path.is_absolute() ||
                std::find(asset_path.begin(), asset_path.end(), std::filesystem::path{".."}) != asset_path.end() ||
                sprites.find(id) != sprites.end())
            {
                return false;
            }
            Bitmap *sprite = simlib::load_bitmap((asset_root / asset_path).string());
            if (!sprite)
            {
                return false;
            }
            sprites.emplace(id, sprite);
            return true;
        }

        bool add_sprite(const std::string &id, float x, float y, float width = 0.0f, float height = 0.0f)
        {
            if (sprites.find(id) == sprites.end())
            {
                return false;
            }
            commands.push_back({width == 0.0f && height == 0.0f ? DrawingType::sprite : DrawingType::sprite_stretched,
                                x,
                                y,
                                width,
                                height,
                                0.0f,
                                0.0f,
                                {},
                                id});
            return true;
        }

        bool add_rotated_sprite(const std::string &id, float center_x, float center_y, float angle_degrees, float width = 0.0f, float height = 0.0f)
        {
            if (sprites.find(id) == sprites.end())
            {
                return false;
            }
            commands.push_back({width == 0.0f && height == 0.0f ? DrawingType::sprite_rotated : DrawingType::sprite_rotated_stretched,
                                center_x,
                                center_y,
                                width,
                                height,
                                angle_degrees,
                                0.0f,
                                {},
                                id});
            return true;
        }

        bool unload_sprite(const std::string &id)
        {
            const auto sprite = sprites.find(id);
            if (sprite == sprites.end())
            {
                return false;
            }
            commands.erase(
                std::remove_if(commands.begin(), commands.end(), [&id](const DrawingCommand &command)
                               { return (command.type == DrawingType::sprite || command.type == DrawingType::sprite_stretched ||
                                         command.type == DrawingType::sprite_rotated || command.type == DrawingType::sprite_rotated_stretched) &&
                                        command.sprite_id == id; }),
                commands.end());
            simlib::destroy_bitmap(sprite->second);
            sprites.erase(sprite);
            return true;
        }

        void clear_sprites()
        {
            commands.erase(
                std::remove_if(commands.begin(), commands.end(), [](const DrawingCommand &command)
                               { return command.type == DrawingType::sprite || command.type == DrawingType::sprite_stretched ||
                                        command.type == DrawingType::sprite_rotated || command.type == DrawingType::sprite_rotated_stretched; }),
                commands.end());
            for (const auto &sprite : sprites)
            {
                simlib::destroy_bitmap(sprite.second);
            }
            sprites.clear();
        }
    };

    LuaCanvas::LuaCanvas()
        : implementation_(new Implementation)
    {
    }

    LuaCanvas::~LuaCanvas()
    {
        reset();
        delete implementation_;
    }

    void LuaCanvas::initialise(LuaRuntime::OutputHandler output_handler)
    {
        if (implementation_->runtime.is_initialised())
        {
            return;
        }

        implementation_->runtime.initialise(std::move(output_handler));
        sol::table app = implementation_->runtime.state()["app"];
        app.set_function(
            "clear_screen",
            [this](int red, int green, int blue, sol::optional<int> alpha)
            {
                implementation_->background = make_colour(red, green, blue, alpha);
            });
        app.set_function("clear_drawings", [this]()
                         { implementation_->commands.clear(); });
        app.set_function("load_sprite", [this](const std::string &id, const std::string &path)
                         { return implementation_->load_sprite(id, path); });
        app.set_function("sprite", [this](const std::string &id, float x, float y)
                         { return implementation_->add_sprite(id, x, y); });
        app.set_function("sprite_stretched", [this](const std::string &id, float x, float y, float width, float height)
                         { return implementation_->add_sprite(id, x, y, width, height); });
        app.set_function("sprite_rotated", [this](const std::string &id, float center_x, float center_y, float angle_degrees)
                         { return implementation_->add_rotated_sprite(id, center_x, center_y, angle_degrees); });
        app.set_function("sprite_rotated_stretched", [this](const std::string &id, float center_x, float center_y, float angle_degrees, float width, float height)
                         { return implementation_->add_rotated_sprite(id, center_x, center_y, angle_degrees, width, height); });
        app.set_function("unload_sprite", [this](const std::string &id)
                         { return implementation_->unload_sprite(id); });
        app.set_function("clear_sprites", [this]()
                         { implementation_->clear_sprites(); });
        app.set_function(
            "pixel",
            [this](float x, float y, int red, int green, int blue, sol::optional<int> alpha)
            {
                implementation_->add_drawing(DrawingType::pixel, x, y, 0.0f, 0.0f, 0.0f, 0.0f, make_colour(red, green, blue, alpha));
            });
        app.set_function(
            "line",
            [this](float x1, float y1, float x2, float y2, int red, int green, int blue, sol::optional<int> alpha)
            {
                implementation_->add_drawing(DrawingType::line, x1, y1, x2, y2, 0.0f, 0.0f, make_colour(red, green, blue, alpha));
            });
        app.set_function(
            "circle",
            [this](float x, float y, float radius, int red, int green, int blue, sol::optional<int> alpha)
            {
                implementation_->add_drawing(DrawingType::circle, x, y, radius, 0.0f, 0.0f, 0.0f, make_colour(red, green, blue, alpha));
            });
        app.set_function(
            "circlefill",
            [this](float x, float y, float radius, int red, int green, int blue, sol::optional<int> alpha)
            {
                implementation_->add_drawing(DrawingType::circlefill, x, y, radius, 0.0f, 0.0f, 0.0f, make_colour(red, green, blue, alpha));
            });
        app.set_function(
            "rect",
            [this](float left, float top, float right, float bottom, int red, int green, int blue, sol::optional<int> alpha)
            {
                implementation_->add_drawing(DrawingType::rect, left, top, right, bottom, 0.0f, 0.0f, make_colour(red, green, blue, alpha));
            });
        app.set_function(
            "rectfill",
            [this](float left, float top, float right, float bottom, int red, int green, int blue, sol::optional<int> alpha)
            {
                implementation_->add_drawing(DrawingType::rectfill, left, top, right, bottom, 0.0f, 0.0f, make_colour(red, green, blue, alpha));
            });
        app.set_function(
            "ellipse",
            [this](float x, float y, float radius_x, float radius_y, int red, int green, int blue, sol::optional<int> alpha)
            {
                implementation_->add_drawing(DrawingType::ellipse, x, y, radius_x, radius_y, 0.0f, 0.0f, make_colour(red, green, blue, alpha));
            });
        app.set_function(
            "ellipsefill",
            [this](float x, float y, float radius_x, float radius_y, int red, int green, int blue, sol::optional<int> alpha)
            {
                implementation_->add_drawing(DrawingType::ellipsefill, x, y, radius_x, radius_y, 0.0f, 0.0f, make_colour(red, green, blue, alpha));
            });
        app.set_function(
            "triangle",
            [this](float x1, float y1, float x2, float y2, float x3, float y3, int red, int green, int blue, sol::optional<int> alpha)
            {
                implementation_->add_drawing(DrawingType::triangle, x1, y1, x2, y2, x3, y3, make_colour(red, green, blue, alpha));
            });
        app.set_function(
            "trianglefill",
            [this](float x1, float y1, float x2, float y2, float x3, float y3, int red, int green, int blue, sol::optional<int> alpha)
            {
                implementation_->add_drawing(DrawingType::trianglefill, x1, y1, x2, y2, x3, y3, make_colour(red, green, blue, alpha));
            });
    }

    bool LuaCanvas::is_initialised() const
    {
        return implementation_->runtime.is_initialised();
    }

    LuaScriptResult LuaCanvas::run_text(const std::string &script)
    {
        if (!is_initialised())
        {
            initialise();
        }
        return implementation_->runtime.execute(script);
    }

    LuaScriptResult LuaCanvas::run_file(const std::string &path)
    {
        std::ifstream file(path);
        if (!file)
        {
            return {false, "Unable to open Lua script: " + path};
        }
        std::ostringstream script;
        script << file.rdbuf();
        return run_text(script.str());
    }

    LuaScriptResult LuaCanvas::dispatch_keypress(const std::string &key)
    {
        if (!is_initialised())
        {
            initialise();
        }
        return implementation_->runtime.call("on_keypress", key);
    }

    LuaRuntime &LuaCanvas::runtime()
    {
        if (!is_initialised())
        {
            initialise();
        }
        return implementation_->runtime;
    }

    void LuaCanvas::set_asset_root(const std::string &path)
    {
        implementation_->asset_root = path;
    }

    void LuaCanvas::render(Bitmap *target) const
    {
        if (!target)
        {
            return;
        }
        clear_to_colour(target, implementation_->background);
        for (const DrawingCommand &command : implementation_->commands)
        {
            switch (command.type)
            {
            case DrawingType::pixel:
                putpixel(target, static_cast<int>(command.x1), static_cast<int>(command.y1), command.colour);
                break;
            case DrawingType::line:
                line(target, command.x1, command.y1, command.x2, command.y2, command.colour);
                break;
            case DrawingType::circle:
                circle(target, command.x1, command.y1, command.x2, command.colour);
                break;
            case DrawingType::circlefill:
                circlefill(target, command.x1, command.y1, command.x2, command.colour);
                break;
            case DrawingType::rect:
                rect(target, command.x1, command.y1, command.x2, command.y2, command.colour);
                break;
            case DrawingType::rectfill:
                rectfill(target, command.x1, command.y1, command.x2, command.y2, command.colour);
                break;
            case DrawingType::ellipse:
                ellipse(target, command.x1, command.y1, command.x2, command.y2, command.colour);
                break;
            case DrawingType::ellipsefill:
                ellipsefill(target, command.x1, command.y1, command.x2, command.y2, command.colour);
                break;
            case DrawingType::triangle:
                triangle(target, command.x1, command.y1, command.x2, command.y2, command.x3, command.y3, command.colour);
                break;
            case DrawingType::trianglefill:
                trianglefill(target, command.x1, command.y1, command.x2, command.y2, command.x3, command.y3, command.colour);
                break;
            case DrawingType::sprite:
            {
                const auto sprite = implementation_->sprites.find(command.sprite_id);
                if (sprite != implementation_->sprites.end())
                    draw_sprite(sprite->second, command.x1, command.y1);
                break;
            }
            case DrawingType::sprite_stretched:
            {
                const auto sprite = implementation_->sprites.find(command.sprite_id);
                if (sprite != implementation_->sprites.end())
                    draw_sprite_stretched(sprite->second, command.x1, command.y1, static_cast<int>(command.x2), static_cast<int>(command.y2));
                break;
            }
            case DrawingType::sprite_rotated:
            {
                const auto sprite = implementation_->sprites.find(command.sprite_id);
                if (sprite != implementation_->sprites.end())
                    draw_sprite_rotated(sprite->second, command.x1, command.y1, command.x3);
                break;
            }
            case DrawingType::sprite_rotated_stretched:
            {
                const auto sprite = implementation_->sprites.find(command.sprite_id);
                if (sprite != implementation_->sprites.end())
                    draw_sprite_rotated_stretched(sprite->second, command.x1, command.y1, command.x3, static_cast<int>(command.x2), static_cast<int>(command.y2));
                break;
            }
            }
        }
    }

    void LuaCanvas::clear()
    {
        implementation_->background = {0, 0, 0};
        implementation_->commands.clear();
    }

    void LuaCanvas::reset()
    {
        clear();
        implementation_->clear_sprites();
        implementation_->runtime.reset();
    }
}
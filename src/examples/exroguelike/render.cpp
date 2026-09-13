#include "render.h"
#include "game_logic.h"
#include "world_gen.h"

void draw_cell(sl::Bitmap *target, int cell_x, int cell_y, char character, const sl::Colour &foreground, const sl::Colour &background)
{
    const int pixel_x = MAP_X + cell_x * CELL_WIDTH;
    const int pixel_y = MAP_Y + cell_y * CELL_HEIGHT;

    if (background.red != 0 || background.green != 0 || background.blue != 0)
    {
        sl::rectfill(target, static_cast<float>(pixel_x), static_cast<float>(pixel_y),
                     static_cast<float>(pixel_x + CELL_WIDTH - 1), static_cast<float>(pixel_y + CELL_HEIGHT - 1), background);
    }

    if (character != ' ' && character != '\0')
    {
        char text[2] = {character, '\0'};
        sl::textout(g_map_font, pixel_x + 2, pixel_y, foreground, text);
    }
}

void draw_text(sl::Bitmap *target, int x, int y, const std::string &text, const sl::Colour &colour)
{
    sl::textout(g_ui_font, x, y, colour, text);
}

void render_tile(sl::Bitmap *target, Game &game, int world_x, int world_y, int screen_x, int screen_y)
{
    Tile *tile = current_tile(game, world_x, world_y);

    if (!tile)
    {
        draw_cell(target, screen_x, screen_y, ' ', Colours::Black);
        return;
    }

    if (!tile->explored)
    {
        draw_cell(target, screen_x, screen_y, ' ', Colours::Black);
        return;
    }

    sl::Colour colour = terrain_colour(tile->terrain, world_x, world_y);

    if (!tile->visible)
    {
        colour = multiply_colour(colour, 0.35f);
    }

    draw_cell(target, screen_x, screen_y, terrain_glyph(tile->terrain), colour);
}

void render_actors(sl::Bitmap *target, Game &game)
{
    for (const Actor &actor : game.actors)
    {
        if (!actor.alive)
            continue;

        Tile *tile = current_tile(game, actor.position.x, actor.position.y);
        if (!tile || !tile->visible)
            continue;

        const int screen_x = actor.position.x - game.camera.x;
        const int screen_y = actor.position.y - game.camera.y;

        if (screen_x < 0 || screen_y < 0 || screen_x >= MAP_WIDTH || screen_y >= MAP_HEIGHT)
        {
            continue;
        }

        draw_cell(target, screen_x, screen_y, actor.glyph.character, actor.glyph.foreground);
    }

    {
        const int screen_x = game.player.position.x - game.camera.x;
        const int screen_y = game.player.position.y - game.camera.y;

        if (screen_x >= 0 && screen_y >= 0 && screen_x < MAP_WIDTH && screen_y < MAP_HEIGHT)
        {
            draw_cell(target, screen_x, screen_y, game.player.glyph.character, game.player.glyph.foreground);
        }
    }
}

void render_map(sl::Bitmap *target, Game &game)
{
    update_camera(game);

    for (int screen_y = 0; screen_y < MAP_HEIGHT; ++screen_y)
    {
        for (int screen_x = 0; screen_x < MAP_WIDTH; ++screen_x)
        {
            const int world_x = game.camera.x + screen_x;
            const int world_y = game.camera.y + screen_y;

            render_tile(target, game, world_x, world_y, screen_x, screen_y);
        }
    }

    render_actors(target, game);
}

void render_header(sl::Bitmap *target, Game &game)
{
    std::stringstream title;
    title << "REALMS BELOW";

    if (game.area_type == AreaType::Overworld)
    {
        title << "   WILDERNESS";
    }
    else
    {
        title << "   DUNGEON " << (game.current_depth + 1);
    }

    title << "   TURN " << game.turn;

    draw_text(target, 10, 10, title.str(), Colours::Gold);
}

void render_environment_panel(sl::Bitmap *target, Game &game)
{
    const int panel_left = MAP_X + MAP_WIDTH * CELL_WIDTH + 16;
    int y = MAP_Y;

    sl::rectfill(target, static_cast<float>(panel_left - 8), static_cast<float>(y - 4),
                 static_cast<float>(SCREEN_WIDTH - 10), static_cast<float>(y + MAP_HEIGHT * CELL_HEIGHT), sl::Colour{18, 24, 35, 240});
    sl::rect(target, static_cast<float>(panel_left - 8), static_cast<float>(y - 4),
             static_cast<float>(SCREEN_WIDTH - 10), static_cast<float>(y + MAP_HEIGHT * CELL_HEIGHT), sl::Colour{60, 90, 120, 255});

    draw_text(target, panel_left, y, "ENVIRONMENT", Colours::Gold);
    y += 24;

    Tile *tile = current_tile(game, game.player.position.x, game.player.position.y);
    const char *t_name = tile ? terrain_name(tile->terrain) : "Unknown";

    draw_text(target, panel_left, y, "Terrain: " + std::string(t_name), Colours::White);
    y += 20;

    if (game.area_type == AreaType::Overworld && game.rworld_generator)
    {
        float lon = 0.0f;
        float lat = 0.0f;
        grid_to_geo(game.player.position.x, game.player.position.y, lon, lat);

        const float height = game.rworld_generator->get_terrain_height(lon, lat);
        const rworld::BiomeType biome = game.rworld_generator->get_biome(lon, lat, std::max(0.0f, height));
        const float temp = game.rworld_generator->get_temperature(lon, lat, std::max(0.0f, height));
        const float precip = game.rworld_generator->get_precipitation(lon, lat, std::max(0.0f, height));
        const float humidity = game.rworld_generator->get_humidity(lon, lat, std::max(0.0f, height));
        const float wind_speed = game.rworld_generator->get_wind_speed(lon, lat, std::max(0.0f, height));
        const rworld::PrecipitationType precip_type = game.rworld_generator->get_precipitation_type(lon, lat, std::max(0.0f, height));

        std::stringstream ss;
        ss << "Biome: " << rworld::biome_to_string(biome);
        draw_text(target, panel_left, y, ss.str(), Colours::LightBlue);
        y += 20;

        ss.str("");
        ss << "Altitude: " << static_cast<int>(height) << " m";
        draw_text(target, panel_left, y, ss.str(), Colours::Grey);
        y += 20;

        ss.str("");
        ss << "Temp: " << static_cast<int>(temp) << " C";
        draw_text(target, panel_left, y, ss.str(), Colours::Orange);
        y += 20;

        ss.str("");
        ss << "Humidity: " << static_cast<int>(humidity * 100.0f) << " %";
        draw_text(target, panel_left, y, ss.str(), Colours::LightGreen);
        y += 20;

        ss.str("");
        ss << "Precip: " << static_cast<int>(precip) << " mm/yr";
        draw_text(target, panel_left, y, ss.str(), Colours::Cyan);
        y += 20;

        const char *pt_str = "None";
        if (precip_type == rworld::PrecipitationType::RAIN)
            pt_str = "Rain";
        else if (precip_type == rworld::PrecipitationType::SNOW)
            pt_str = "Snow";
        else if (precip_type == rworld::PrecipitationType::SLEET)
            pt_str = "Sleet";

        ss.str("");
        ss << "Weather: " << pt_str;
        draw_text(target, panel_left, y, ss.str(), Colours::Grey);
        y += 20;

        ss.str("");
        ss << "Wind: " << static_cast<int>(wind_speed) << " km/h";
        draw_text(target, panel_left, y, ss.str(), Colours::White);
        y += 20;
    }
    else if (game.area_type == AreaType::Dungeon)
    {
        draw_text(target, panel_left, y, "Biome: Subterranean", Colours::Purple);
        y += 20;

        std::stringstream ss;
        ss << "Depth: Level " << (game.current_depth + 1);
        draw_text(target, panel_left, y, ss.str(), Colours::Grey);
        y += 20;

        draw_text(target, panel_left, y, "Temp: 12 C", Colours::Orange);
        y += 20;

        draw_text(target, panel_left, y, "Humidity: 85 %", Colours::LightGreen);
        y += 20;

        draw_text(target, panel_left, y, "Weather: Calm", Colours::Grey);
        y += 20;
    }
}

void render_status(sl::Bitmap *target, Game &game)
{
    std::stringstream stream;
    stream << "HP " << game.player.health.current << "/" << game.player.health.maximum
           << "   ATK " << game.player.combat.attack
           << "   DEF " << game.player.combat.defence;

    draw_text(target, 10, SCREEN_HEIGHT - 70, stream.str(), Colours::White);

    std::stringstream light_stream;
    int shadow_count = 0;

    for (int slot : game.lighting.shadow_slots)
    {
        if (slot >= 0)
        {
            ++shadow_count;
        }
    }

    light_stream << "LIGHTS " << game.lighting.lights.size()
                 << "   SHADOWS " << shadow_count << "/8";

    draw_text(target, 420, SCREEN_HEIGHT - 70, light_stream.str(), Colours::Yellow);
}

void render_messages(sl::Bitmap *target, Game &game)
{
    int y = SCREEN_HEIGHT - 45;

    for (const std::string &message : game.messages)
    {
        draw_text(target, 10, y, message, Colours::Grey);
        y += 14;
    }
}

void render_ui(sl::Bitmap *target, Game &game)
{
    render_header(target, game);
    render_environment_panel(target, game);
    render_status(target, game);
    render_messages(target, game);

    if (game.state == GameState::GameOver)
    {
        draw_text(target, SCREEN_WIDTH / 2 - 80, SCREEN_HEIGHT / 2, "YOU HAVE DIED", Colours::Red);
    }
}

void render_game(sl::Bitmap *target, Game &game)
{
    render_map(target, game);
    render_ui(target, game);
}

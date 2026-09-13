#include "game_logic.h"
#include "world_gen.h"

const char *terrain_name(Terrain terrain)
{
    switch (terrain)
    {
    case Terrain::DeepWater: return "Deep Water";
    case Terrain::Water: return "Water";
    case Terrain::Beach: return "Beach";
    case Terrain::Plains: return "Plains";
    case Terrain::Grass: return "Grassland";
    case Terrain::Forest: return "Forest";
    case Terrain::Hills: return "Hills";
    case Terrain::Mountain: return "Mountain";
    case Terrain::Swamp: return "Swamp";
    case Terrain::Snow: return "Snow/Tundra";
    case Terrain::Ruins: return "Ruins";
    case Terrain::DungeonEntrance: return "Dungeon Entrance";
    case Terrain::DungeonFloor: return "Dungeon Floor";
    case Terrain::DungeonWall: return "Dungeon Wall";
    case Terrain::Door: return "Door";
    case Terrain::StairsUp: return "Stairs Up";
    case Terrain::StairsDown: return "Stairs Down";
    case Terrain::Torch: return "Torch";
    case Terrain::Brazier: return "Brazier";
    case Terrain::Crystal: return "Crystal";
    default: return "Unknown";
    }
}

char terrain_glyph(Terrain terrain)
{
    switch (terrain)
    {
    case Terrain::DeepWater: return '~';
    case Terrain::Water: return '~';
    case Terrain::Beach: return ',';
    case Terrain::Plains: return '.';
    case Terrain::Grass: return '"';
    case Terrain::Forest: return 'T';
    case Terrain::Hills: return '^';
    case Terrain::Mountain: return '^';
    case Terrain::Swamp: return ';';
    case Terrain::Snow: return '*';
    case Terrain::Ruins: return '&';
    case Terrain::DungeonEntrance: return '>';
    case Terrain::DungeonFloor: return '.';
    case Terrain::DungeonWall: return '#';
    case Terrain::Door: return '+';
    case Terrain::StairsUp: return '<';
    case Terrain::StairsDown: return '>';
    case Terrain::Torch: return 't';
    case Terrain::Brazier: return 'B';
    case Terrain::Crystal: return '*';
    default: return ' ';
    }
}

sl::Colour get_torch_colour(int x, int y)
{
    std::uint32_t hash = static_cast<std::uint32_t>(x * 73856093 ^ y * 19349663);
    int variant = hash % 10;
    switch (variant)
    {
    case 0: return sl::Colour{240, 60, 50, 255};   // Red torch
    case 1: return sl::Colour{60, 180, 255, 255};  // Blue torch
    case 2: return sl::Colour{80, 230, 90, 255};   // Green torch
    case 3: return sl::Colour{255, 240, 200, 255}; // Bright Warm White torch
    case 4: return sl::Colour{255, 220, 100, 255}; // Yellowish torch
    default: return Colours::TorchLight;          // Classic Torch (amber)
    }
}

sl::Colour terrain_colour(Terrain terrain, int x, int y)
{
    if (terrain == Terrain::Torch)
    {
        return get_torch_colour(x, y);
    }

    switch (terrain)
    {
    case Terrain::DeepWater: return Colours::DarkBlue;
    case Terrain::Water: return Colours::Blue;
    case Terrain::Beach: return Colours::Yellow;
    case Terrain::Plains: return Colours::LightGreen;
    case Terrain::Grass: return Colours::Green;
    case Terrain::Forest: return Colours::DarkGreen;
    case Terrain::Hills: return Colours::Brown;
    case Terrain::Mountain: return Colours::Grey;
    case Terrain::Swamp: return Colours::DarkGreen;
    case Terrain::Snow: return Colours::White;
    case Terrain::Ruins: return Colours::Grey;
    case Terrain::DungeonEntrance: return Colours::Gold;
    case Terrain::DungeonFloor: return Colours::Floor;
    case Terrain::DungeonWall: return Colours::Wall;
    case Terrain::Door: return Colours::Brown;
    case Terrain::StairsUp: return Colours::Cyan;
    case Terrain::StairsDown: return Colours::Gold;
    case Terrain::Torch: return Colours::Yellow;
    case Terrain::Brazier: return Colours::Orange;
    case Terrain::Crystal: return Colours::Magenta;
    default: return Colours::White;
    }
}

bool terrain_blocks_movement(Terrain terrain)
{
    switch (terrain)
    {
    case Terrain::DeepWater:
    case Terrain::Water:
    case Terrain::Mountain:
    case Terrain::DungeonWall:
        return true;
    default:
        return false;
    }
}

bool terrain_blocks_sight(Terrain terrain)
{
    switch (terrain)
    {
    case Terrain::Mountain:
    case Terrain::Forest:
    case Terrain::DungeonWall:
        return true;
    default:
        return false;
    }
}

void set_tile(Tile &tile, Terrain terrain)
{
    tile.terrain = terrain;
    tile.blocks_movement = terrain_blocks_movement(terrain);
    tile.blocks_sight = terrain_blocks_sight(terrain);
}

Dungeon *find_dungeon_at(Game &game, int x, int y)
{
    for (auto &dungeon : game.dungeons)
    {
        if (dungeon->overworld_position.x == x && dungeon->overworld_position.y == y)
        {
            return dungeon.get();
        }
    }
    return nullptr;
}

void enter_dungeon(Game &game)
{
    const Position position = game.player.position;
    Dungeon *dungeon = find_dungeon_at(game, position.x, position.y);

    if (!dungeon)
    {
        auto new_dungeon = std::make_unique<Dungeon>();
        new_dungeon->id = static_cast<int>(game.dungeons.size());
        new_dungeon->overworld_position = position;
        new_dungeon->levels.push_back(generate_dungeon_level(1));

        dungeon = new_dungeon.get();
        game.dungeons.push_back(std::move(new_dungeon));
    }

    game.current_dungeon = dungeon->id;
    game.current_depth = 0;
    game.current_level = dungeon->levels[0].get();
    game.area_type = AreaType::Dungeon;

    game.player.position = game.current_level->stairs_up;
    game.player_position = game.player.position;

    game.message("You descend into darkness.");
}

void descend_dungeon(Game &game)
{
    if (game.current_dungeon < 0) return;

    Dungeon &dungeon = *game.dungeons[game.current_dungeon];
    const int next_depth = game.current_depth + 1;

    if (next_depth >= static_cast<int>(dungeon.levels.size()))
    {
        dungeon.levels.push_back(generate_dungeon_level(next_depth + 1));
    }

    game.current_depth = next_depth;
    game.current_level = dungeon.levels[next_depth].get();

    game.player.position = game.current_level->stairs_up;
    game.player_position = game.player.position;

    game.message("You descend deeper.");
}

void ascend_dungeon(Game &game)
{
    if (game.current_depth > 0)
    {
        Dungeon &dungeon = *game.dungeons[game.current_dungeon];
        --game.current_depth;

        game.current_level = dungeon.levels[game.current_depth].get();
        game.player.position = game.current_level->stairs_down;
        game.player_position = game.player.position;

        game.message("You climb upwards.");
        return;
    }

    Dungeon &dungeon = *game.dungeons[game.current_dungeon];
    game.area_type = AreaType::Overworld;
    game.current_level = nullptr;

    game.player.position = dungeon.overworld_position;
    game.player_position = game.player.position;
    game.current_dungeon = -1;

    game.message("You return to the wilderness.");
}

Tile *current_tile(Game &game, int x, int y)
{
    if (game.area_type == AreaType::Overworld)
    {
        if (!game.overworld.inside(x, y)) return nullptr;
        return &game.overworld.at(x, y);
    }

    if (!game.current_level) return nullptr;
    if (!game.current_level->inside(x, y)) return nullptr;

    return &game.current_level->at(x, y);
}

Actor *actor_at(Game &game, int x, int y)
{
    for (Actor &actor : game.actors)
    {
        if (actor.alive && actor.blocks_movement && actor.position.x == x && actor.position.y == y)
        {
            return &actor;
        }
    }
    return nullptr;
}

void attack(Game &game, Actor &attacker, Actor &defender)
{
    int damage = attacker.combat.attack + random_int(-1, 2) - defender.combat.defence;
    damage = std::max(damage, 1);

    defender.health.current -= damage;

    {
        std::stringstream stream;
        stream << attacker.glyph.character << " hits " << defender.glyph.character << " for " << damage << " damage.";
        game.message(stream.str());
    }

    if (defender.health.current <= 0)
    {
        defender.alive = false;
        if (defender.player)
        {
            game.state = GameState::GameOver;
            game.message("You have died.");
        }
        else
        {
            game.message("The creature dies.");
        }
    }
}

bool try_player_move(Game &game, int dx, int dy)
{
    if (game.state != GameState::Playing) return false;

    const int x = game.player.position.x + dx;
    const int y = game.player.position.y + dy;

    Tile *tile = current_tile(game, x, y);
    if (!tile) return false;

    Actor *actor = actor_at(game, x, y);
    if (actor)
    {
        attack(game, game.player, *actor);
        return true;
    }

    if (tile->blocks_movement) return false;

    game.player.position = {x, y};
    game.player_position = game.player.position;

    return true;
}

bool line_of_sight(Game &game, int x0, int y0, int x1, int y1)
{
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int error = dx - dy;

    int x = x0;
    int y = y0;

    while (true)
    {
        if (x == x1 && y == y1) return true;

        if (x != x0 || y != y0)
        {
            Tile *tile = current_tile(game, x, y);
            if (!tile || tile->blocks_sight) return false;
        }

        const int error2 = error * 2;
        if (error2 > -dy)
        {
            error -= dy;
            x += sx;
        }
        if (error2 < dx)
        {
            error += dx;
            y += sy;
        }
    }
}

void clear_visibility(Game &game)
{
    if (game.area_type == AreaType::Overworld)
    {
        for (Tile &tile : game.overworld.tiles)
        {
            tile.visible = false;
        }
    }
    else if (game.current_level)
    {
        for (Tile &tile : game.current_level->tiles)
        {
            tile.visible = false;
        }
    }
}

void update_fov(Game &game)
{
    clear_visibility(game);
    const Position origin = game.player.position;

    for (int y = origin.y - PLAYER_FOV_RADIUS; y <= origin.y + PLAYER_FOV_RADIUS; ++y)
    {
        for (int x = origin.x - PLAYER_FOV_RADIUS; x <= origin.x + PLAYER_FOV_RADIUS; ++x)
        {
            const int dx = x - origin.x;
            const int dy = y - origin.y;

            if (dx * dx + dy * dy > PLAYER_FOV_RADIUS * PLAYER_FOV_RADIUS) continue;

            Tile *tile = current_tile(game, x, y);
            if (!tile) continue;

            if (line_of_sight(game, origin.x, origin.y, x, y))
            {
                tile->visible = true;
                tile->explored = true;
            }
        }
    }
}

void monster_turns(Game &game)
{
    if (game.area_type != AreaType::Dungeon) return;

    for (Actor &monster : game.actors)
    {
        if (!monster.alive) continue;

        const int dx = game.player.position.x - monster.position.x;
        const int dy = game.player.position.y - monster.position.y;
        const int distance_squared = dx * dx + dy * dy;

        if (distance_squared <= 2)
        {
            attack(game, monster, game.player);
            continue;
        }

        if (distance_squared <= 100 && line_of_sight(game, monster.position.x, monster.position.y, game.player.position.x, game.player.position.y))
        {
            const int move_x = dx == 0 ? 0 : (dx > 0 ? 1 : -1);
            const int move_y = dy == 0 ? 0 : (dy > 0 ? 1 : -1);

            const int target_x = monster.position.x + move_x;
            const int target_y = monster.position.y + move_y;

            Tile *tile = current_tile(game, target_x, target_y);
            if (tile && !tile->blocks_movement && !actor_at(game, target_x, target_y))
            {
                if (target_x != game.player.position.x || target_y != game.player.position.y)
                {
                    monster.position = {target_x, target_y};
                }
            }
        }
        else if (random_chance(0.25f))
        {
            const int move_x = random_int(-1, 1);
            const int move_y = random_int(-1, 1);

            Tile *tile = current_tile(game, monster.position.x + move_x, monster.position.y + move_y);
            if (tile && !tile->blocks_movement)
            {
                monster.position.x += move_x;
                monster.position.y += move_y;
            }
        }
    }
}

void advance_turn(Game &game)
{
    ++game.turn;
    monster_turns(game);
    update_fov(game);
}

bool use_stairs(Game &game)
{
    Tile *tile = current_tile(game, game.player.position.x, game.player.position.y);
    if (!tile) return false;

    if (game.area_type == AreaType::Overworld)
    {
        if (tile->terrain == Terrain::DungeonEntrance)
        {
            enter_dungeon(game);
            populate_dungeon(game);
            update_camera(game);
            update_fov(game);
            build_lights(game);
            return true;
        }
    }
    else
    {
        if (tile->terrain == Terrain::StairsDown)
        {
            descend_dungeon(game);
            populate_dungeon(game);
            update_camera(game);
            update_fov(game);
            build_lights(game);
            return true;
        }

        if (tile->terrain == Terrain::StairsUp)
        {
            ascend_dungeon(game);
            populate_dungeon(game);
            update_camera(game);
            update_fov(game);
            build_lights(game);
            return true;
        }
    }

    return false;
}

void update_camera(Game &game)
{
    game.camera.x = game.player.position.x - MAP_WIDTH / 2;
    game.camera.y = game.player.position.y - MAP_HEIGHT / 2;

    if (game.area_type == AreaType::Overworld)
    {
        game.camera.x = std::clamp(game.camera.x, 0, WORLD_WIDTH - MAP_WIDTH);
        game.camera.y = std::clamp(game.camera.y, 0, WORLD_HEIGHT - MAP_HEIGHT);
    }
    else if (game.current_level)
    {
        game.camera.x = std::clamp(game.camera.x, 0, std::max(0, game.current_level->width - MAP_WIDTH));
        game.camera.y = std::clamp(game.camera.y, 0, std::max(0, game.current_level->height - MAP_HEIGHT));
    }
}

void build_lights(Game &game)
{
    game.lighting.clear();

    game.lighting.add_point_light(
        static_cast<float>(game.player.position.x) + 0.5f,
        static_cast<float>(game.player.position.y) + 0.5f,
        9.0f,
        Colours::TorchLight,
        1000.0f,
        true,
        true);

    if (game.area_type == AreaType::Dungeon && game.current_level)
    {
        for (int y = 0; y < game.current_level->height; ++y)
        {
            for (int x = 0; x < game.current_level->width; ++x)
            {
                const Tile &tile = game.current_level->at(x, y);

                switch (tile.terrain)
                {
                case Terrain::Torch:
                    game.lighting.add_point_light(x + 0.5f, y + 0.5f, 7.0f, get_torch_colour(x, y), 250.0f, true, true);
                    break;
                case Terrain::Brazier:
                    game.lighting.add_point_light(x + 0.5f, y + 0.5f, 10.0f, Colours::BlueFire, 500.0f, true, true);
                    break;
                case Terrain::Crystal:
                    game.lighting.add_point_light(x + 0.5f, y + 0.5f, 8.0f, Colours::Purple, 150.0f, false, false);
                    game.lighting.add_spotlight(x + 0.5f, y + 0.5f, 12.0f, game.game_time * 0.5f, 35.0f, Colours::Magenta, 300.0f, true);
                    break;
                default:
                    break;
                }
            }
        }
    }

    game.lighting.update(
        static_cast<float>(game.player.position.x) + 0.5f,
        static_cast<float>(game.player.position.y) + 0.5f,
        game.game_time);
}

std::vector<sl::Light> build_simlib_lights(Game &game)
{
    std::vector<sl::Light> lights;
    std::vector<bool> added(game.lighting.lights.size(), false);

    for (int slot : game.lighting.shadow_slots)
    {
        if (slot < 0 || slot >= static_cast<int>(game.lighting.lights.size())) continue;
        const GameLight &light = game.lighting.lights[slot];
        if (!light.active) continue;

        sl::Light sl_light;
        sl_light.x = static_cast<float>(MAP_X) + (light.x - static_cast<float>(game.camera.x)) * CELL_WIDTH;
        sl_light.y = static_cast<float>(MAP_Y) + (light.y - static_cast<float>(game.camera.y)) * CELL_HEIGHT;
        sl_light.radius = light.radius * CELL_WIDTH;
        sl_light.intensity = light.intensity;
        sl_light.shadow_softness = 2.0f;
        sl_light.colour = light.colour;

        if (light.type == LightType::Spotlight)
        {
            sl_light.direction_x = std::cos(light.direction);
            sl_light.direction_y = std::sin(light.direction);
            sl_light.inner_angle = light.cone_angle * 0.5f;
            sl_light.outer_angle = light.cone_angle;
        }

        lights.push_back(sl_light);
        added[slot] = true;
    }

    for (std::size_t i = 0; i < game.lighting.lights.size(); ++i)
    {
        if (added[i]) continue;
        const GameLight &light = game.lighting.lights[i];
        if (!light.active) continue;

        float screen_x = static_cast<float>(MAP_X) + (light.x - static_cast<float>(game.camera.x)) * CELL_WIDTH;
        float screen_y = static_cast<float>(MAP_Y) + (light.y - static_cast<float>(game.camera.y)) * CELL_HEIGHT;
        float radius_px = light.radius * CELL_WIDTH;

        if (screen_x + radius_px < 0 || screen_x - radius_px > SCREEN_WIDTH ||
            screen_y + radius_px < 0 || screen_y - radius_px > SCREEN_HEIGHT)
        {
            continue;
        }

        sl::Light sl_light;
        sl_light.x = screen_x;
        sl_light.y = screen_y;
        sl_light.radius = radius_px;
        sl_light.intensity = light.intensity;
        sl_light.shadow_softness = 0.0f;
        sl_light.colour = light.colour;

        if (light.type == LightType::Spotlight)
        {
            sl_light.direction_x = std::cos(light.direction);
            sl_light.direction_y = std::sin(light.direction);
            sl_light.inner_angle = light.cone_angle * 0.5f;
            sl_light.outer_angle = light.cone_angle;
        }

        lights.push_back(sl_light);
    }

    return lights;
}

std::vector<sl::ShadowCaster> build_simlib_shadow_casters(Game &game)
{
    std::vector<sl::ShadowCaster> casters;
    if (game.area_type != AreaType::Dungeon)
    {
        return casters;
    }

    for (int screen_y = 0; screen_y < MAP_HEIGHT; ++screen_y)
    {
        for (int screen_x = 0; screen_x < MAP_WIDTH; ++screen_x)
        {
            const int world_x = game.camera.x + screen_x;
            const int world_y = game.camera.y + screen_y;

            Tile *tile = current_tile(game, world_x, world_y);
            if (tile && tile->visible && tile->blocks_sight)
            {
                bool near_light = false;
                for (int slot : game.lighting.shadow_slots)
                {
                    if (slot < 0 || slot >= static_cast<int>(game.lighting.lights.size())) continue;
                    const GameLight &l = game.lighting.lights[slot];
                    if (!l.active) continue;
                    float dx = (world_x + 0.5f) - l.x;
                    float dy = (world_y + 0.5f) - l.y;
                    if (dx * dx + dy * dy <= (l.radius + 1.0f) * (l.radius + 1.0f))
                    {
                        near_light = true;
                        break;
                    }
                }

                if (near_light)
                {
                    float px = static_cast<float>(MAP_X + screen_x * CELL_WIDTH);
                    float py = static_cast<float>(MAP_Y + screen_y * CELL_HEIGHT);
                    casters.push_back(sl::make_rectangle_shadow_caster(px, py, px + CELL_WIDTH, py + CELL_HEIGHT));
                }
            }
        }
    }
    return casters;
}

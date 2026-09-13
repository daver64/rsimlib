#define _RWORLD_IMPLEMENTATION
#include "world_gen.h"
#include "game_logic.h"

void grid_to_geo(int x, int y, float &lon, float &lat)
{
    constexpr float lon_span = 24.0f;
    constexpr float lat_span = 24.0f;
    constexpr float center_lat = 35.0f;

    lon = ((static_cast<float>(x) / static_cast<float>(WORLD_WIDTH)) - 0.5f) * lon_span;
    lat = ((static_cast<float>(y) / static_cast<float>(WORLD_HEIGHT)) - 0.5f) * lat_span + center_lat;
}

void ensure_overworld_connectivity(
    Game &game,
    const std::vector<Position> &entrances)
{
    std::vector<bool> reachable(WORLD_WIDTH * WORLD_HEIGHT, false);
    std::vector<Position> queue;
    queue.reserve(WORLD_WIDTH * WORLD_HEIGHT);

    auto index = [](int x, int y) { return static_cast<std::size_t>(y) * WORLD_WIDTH + static_cast<std::size_t>(x); };

    const int start_x = game.player.position.x;
    const int start_y = game.player.position.y;

    if (game.overworld.inside(start_x, start_y) && !game.overworld.at(start_x, start_y).blocks_movement)
    {
        reachable[index(start_x, start_y)] = true;
        queue.push_back({start_x, start_y});
    }

    std::size_t head = 0;
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};

    while (head < queue.size())
    {
        Position curr = queue[head++];
        for (int i = 0; i < 4; ++i)
        {
            int nx = curr.x + dx[i];
            int ny = curr.y + dy[i];
            if (game.overworld.inside(nx, ny))
            {
                std::size_t idx = index(nx, ny);
                if (!reachable[idx] && !game.overworld.at(nx, ny).blocks_movement)
                {
                    reachable[idx] = true;
                    queue.push_back({nx, ny});
                }
            }
        }
    }

    for (const Position &entrance : entrances)
    {
        if (reachable[index(entrance.x, entrance.y)])
        {
            continue;
        }

        int best_x = start_x;
        int best_y = start_y;
        int min_dist = std::numeric_limits<int>::max();

        for (std::size_t i = 0; i < queue.size(); ++i)
        {
            int dist = std::abs(queue[i].x - entrance.x) + std::abs(queue[i].y - entrance.y);
            if (dist < min_dist)
            {
                min_dist = dist;
                best_x = queue[i].x;
                best_y = queue[i].y;
            }
        }

        int cx = best_x;
        int cy = best_y;

        auto carve_tile = [&](int x, int y) {
            Tile &tile = game.overworld.at(x, y);
            if (tile.blocks_movement)
            {
                if (tile.terrain == Terrain::DeepWater || tile.terrain == Terrain::Water)
                {
                    set_tile(tile, Terrain::Beach);
                }
                else if (tile.terrain == Terrain::Mountain)
                {
                    set_tile(tile, Terrain::Hills);
                }
                else
                {
                    set_tile(tile, Terrain::Plains);
                }
            }
            std::size_t idx = index(x, y);
            if (!reachable[idx])
            {
                reachable[idx] = true;
                queue.push_back({x, y});
            }
        };

        while (cx != entrance.x)
        {
            cx += (entrance.x > cx) ? 1 : -1;
            carve_tile(cx, cy);
        }
        while (cy != entrance.y)
        {
            cy += (entrance.y > cy) ? 1 : -1;
            carve_tile(cx, cy);
        }

        while (head < queue.size())
        {
            Position curr = queue[head++];
            for (int i = 0; i < 4; ++i)
            {
                int nx = curr.x + dx[i];
                int ny = curr.y + dy[i];
                if (game.overworld.inside(nx, ny))
                {
                    std::size_t idx = index(nx, ny);
                    if (!reachable[idx] && !game.overworld.at(nx, ny).blocks_movement)
                    {
                        reachable[idx] = true;
                        queue.push_back({nx, ny});
                    }
                }
            }
        }
    }
}

void generate_overworld(Game &game)
{
    rworld::WorldConfig config;
    config.seed = static_cast<uint64_t>(random_int(1, 1000000));
    config.world_scale = 0.5f;
    config.terrain_frequency = 0.0008f;
    config.moisture_frequency = 0.0008f;

    game.rworld_generator = std::make_shared<rworld::World>(config);
    rworld::World &world = *game.rworld_generator;

    for (int y = 0; y < WORLD_HEIGHT; ++y)
    {
        for (int x = 0; x < WORLD_WIDTH; ++x)
        {
            float lon = 0.0f;
            float lat = 0.0f;
            grid_to_geo(x, y, lon, lat);

            const float height = world.get_terrain_height(lon, lat);
            const rworld::BiomeType biome = world.get_biome(lon, lat, std::max(0.0f, height));

            Terrain terrain = Terrain::Plains;

            if (height < -500.0f)
            {
                terrain = Terrain::DeepWater;
            }
            else if (height < 0.0f)
            {
                terrain = Terrain::Water;
            }
            else if (height < 100.0f && biome == rworld::BiomeType::BEACH)
            {
                terrain = Terrain::Beach;
            }
            else
            {
                switch (biome)
                {
                case rworld::BiomeType::SNOW:
                case rworld::BiomeType::ICE:
                case rworld::BiomeType::TUNDRA:
                    terrain = Terrain::Snow;
                    break;
                case rworld::BiomeType::TAIGA:
                case rworld::BiomeType::TEMPERATE_DECIDUOUS_FOREST:
                case rworld::BiomeType::TEMPERATE_RAINFOREST:
                case rworld::BiomeType::TROPICAL_RAINFOREST:
                case rworld::BiomeType::TROPICAL_SEASONAL_FOREST:
                case rworld::BiomeType::MOUNTAIN_FOREST:
                    terrain = Terrain::Forest;
                    break;
                case rworld::BiomeType::GRASSLAND:
                case rworld::BiomeType::SAVANNA:
                    terrain = Terrain::Grass;
                    break;
                case rworld::BiomeType::DESERT:
                case rworld::BiomeType::COLD_DESERT:
                    terrain = Terrain::Beach;
                    break;
                case rworld::BiomeType::MOUNTAIN_TUNDRA:
                    terrain = Terrain::Hills;
                    break;
                case rworld::BiomeType::MOUNTAIN_PEAK:
                    terrain = Terrain::Mountain;
                    break;
                default:
                    if (height > 3000.0f) terrain = Terrain::Mountain;
                    else if (height > 1500.0f) terrain = Terrain::Hills;
                    else terrain = Terrain::Plains;
                    break;
                }
            }

            set_tile(game.overworld.at(x, y), terrain);
        }
    }

    for (int y = 246; y <= 266; ++y)
    {
        for (int x = 246; x <= 266; ++x)
        {
            set_tile(game.overworld.at(x, y), Terrain::Grass);
        }
    }

    std::vector<Position> entrance_positions;
    int entrances = 0;

    while (entrances < 24)
    {
        const int x = random_int(10, WORLD_WIDTH - 11);
        const int y = random_int(10, WORLD_HEIGHT - 11);

        Tile &tile = game.overworld.at(x, y);

        if (tile.terrain == Terrain::Mountain ||
            tile.terrain == Terrain::Forest ||
            tile.terrain == Terrain::Hills)
        {
            set_tile(tile, Terrain::DungeonEntrance);
            entrance_positions.push_back({x, y});
            ++entrances;
        }
    }

    game.player.position = {256, 256};
    game.player_position = game.player.position;

    ensure_overworld_connectivity(game, entrance_positions);

    game.message("You enter the wilderness.");
}

void initialise_dungeon(DungeonLevel &level)
{
    for (int y = 0; y < level.height; ++y)
    {
        for (int x = 0; x < level.width; ++x)
        {
            set_tile(level.at(x, y), Terrain::DungeonWall);
        }
    }
}

void carve_floor(DungeonLevel &level, int x, int y)
{
    if (!level.inside(x, y)) return;
    set_tile(level.at(x, y), Terrain::DungeonFloor);
}

void carve_room(DungeonLevel &level, const Room &room)
{
    for (int y = room.y; y < room.y + room.height; ++y)
    {
        for (int x = room.x; x < room.x + room.width; ++x)
        {
            carve_floor(level, x, y);
        }
    }
}

void carve_horizontal(DungeonLevel &level, int x1, int x2, int y)
{
    if (x1 > x2) std::swap(x1, x2);
    for (int x = x1; x <= x2; ++x)
    {
        carve_floor(level, x, y);
    }
}

void carve_vertical(DungeonLevel &level, int y1, int y2, int x)
{
    if (y1 > y2) std::swap(y1, y2);
    for (int y = y1; y <= y2; ++y)
    {
        carve_floor(level, x, y);
    }
}

void connect_rooms(DungeonLevel &level, const Room &a, const Room &b)
{
    const int x1 = a.centre_x();
    const int y1 = a.centre_y();
    const int x2 = b.centre_x();
    const int y2 = b.centre_y();

    if (random_chance(0.5f))
    {
        carve_horizontal(level, x1, x2, y1);
        carve_vertical(level, y1, y2, x2);
    }
    else
    {
        carve_vertical(level, y1, y2, x1);
        carve_horizontal(level, x1, x2, y2);
    }
}

Room make_room_in_sector(int sector_x, int sector_y)
{
    const int sector_width = DUNGEON_WIDTH / DUNGEON_SECTORS_X;
    const int sector_height = DUNGEON_HEIGHT / DUNGEON_SECTORS_Y;
    const int sx = sector_x * sector_width;
    const int sy = sector_y * sector_height;
    const int padding = 2;
    const int maximum_width = sector_width - padding * 2;
    const int maximum_height = sector_height - padding * 2;

    Room room;
    room.width = random_int(std::max(6, maximum_width / 2), maximum_width);
    room.height = random_int(5, std::max(5, maximum_height));
    room.x = sx + random_int(padding, std::max(padding, sector_width - room.width - padding));
    room.y = sy + random_int(padding, std::max(padding, sector_height - room.height - padding));

    const int type = random_int(0, 9);
    if (type == 0) room.type = RoomType::Crypt;
    else if (type == 1) room.type = RoomType::Temple;
    else if (type == 2) room.type = RoomType::Large;
    else room.type = RoomType::Normal;

    return room;
}

std::unique_ptr<DungeonLevel> generate_dungeon_level(int depth)
{
    auto level = std::make_unique<DungeonLevel>();
    initialise_dungeon(*level);

    const int room_count = random_int(5, DUNGEON_MAX_ROOMS);
    std::array<bool, DUNGEON_MAX_ROOMS> used_sectors{};
    used_sectors.fill(false);

    for (int room_index = 0; room_index < room_count; ++room_index)
    {
        int sector;
        do
        {
            sector = random_int(0, DUNGEON_MAX_ROOMS - 1);
        } while (used_sectors[sector]);

        used_sectors[sector] = true;
        const int sector_x = sector % DUNGEON_SECTORS_X;
        const int sector_y = sector / DUNGEON_SECTORS_X;

        Room room = make_room_in_sector(sector_x, sector_y);
        carve_room(*level, room);
        level->rooms.push_back(room);
    }

    if (level->rooms.empty())
    {
        Room room;
        room.x = 5;
        room.y = 5;
        room.width = 12;
        room.height = 8;
        carve_room(*level, room);
        level->rooms.push_back(room);
    }

    for (size_t i = 1; i < level->rooms.size(); ++i)
    {
        connect_rooms(*level, level->rooms[i - 1], level->rooms[i]);
    }

    {
        const Room &room = level->rooms.front();
        level->stairs_up = {room.centre_x(), room.centre_y()};
        set_tile(level->at(level->stairs_up.x, level->stairs_up.y), Terrain::StairsUp);
    }

    {
        const Room &room = level->rooms.back();
        level->stairs_down = {room.centre_x(), room.centre_y()};
        set_tile(level->at(level->stairs_down.x, level->stairs_down.y), Terrain::StairsDown);
    }

    {
        const int d_width = level->width;
        const int d_height = level->height;
        std::vector<bool> d_reachable(static_cast<std::size_t>(d_width) * static_cast<std::size_t>(d_height), false);
        std::vector<Position> d_queue;
        d_queue.reserve(static_cast<std::size_t>(d_width) * static_cast<std::size_t>(d_height));

        auto d_index = [d_width](int x, int y) {
            return static_cast<std::size_t>(y) * static_cast<std::size_t>(d_width) + static_cast<std::size_t>(x);
        };

        if (level->inside(level->stairs_up.x, level->stairs_up.y))
        {
            d_reachable[d_index(level->stairs_up.x, level->stairs_up.y)] = true;
            d_queue.push_back(level->stairs_up);
        }

        std::size_t d_head = 0;
        const int dx_d[4] = {1, -1, 0, 0};
        const int dy_d[4] = {0, 0, 1, -1};

        while (d_head < d_queue.size())
        {
            Position curr = d_queue[d_head++];
            for (int i = 0; i < 4; ++i)
            {
                int nx = curr.x + dx_d[i];
                int ny = curr.y + dy_d[i];
                if (level->inside(nx, ny))
                {
                    std::size_t idx = d_index(nx, ny);
                    if (!d_reachable[idx] && !level->at(nx, ny).blocks_movement)
                    {
                        d_reachable[idx] = true;
                        d_queue.push_back({nx, ny});
                    }
                }
            }
        }

        if (!d_reachable[d_index(level->stairs_down.x, level->stairs_down.y)])
        {
            connect_rooms(*level, level->rooms.front(), level->rooms.back());
        }
    }

    for (const Room &room : level->rooms)
    {
        if (random_chance(0.75f))
        {
            set_tile(level->at(room.x + 1, room.centre_y()), Terrain::Torch);
        }

        if (random_chance(0.60f))
        {
            set_tile(level->at(room.x + room.width - 2, room.centre_y()), Terrain::Torch);
        }

        if (room.type == RoomType::Crypt)
        {
            set_tile(level->at(room.centre_x(), room.centre_y()), Terrain::Brazier);
        }

        if (room.type == RoomType::Temple)
        {
            set_tile(level->at(room.centre_x(), room.centre_y()), Terrain::Crystal);
        }
    }

    return level;
}

Actor make_monster(Game &game, MonsterType type, int x, int y)
{
    Actor actor;
    actor.id = game.next_actor_id++;
    actor.position = {x, y};
    actor.monster_type = type;

    switch (type)
    {
    case MonsterType::Rat:
        actor.glyph = {'r', Colours::Rat};
        actor.health = {4, 4};
        actor.combat = {2, 0};
        break;

    case MonsterType::Goblin:
        actor.glyph = {'g', Colours::Goblin};
        actor.health = {8, 8};
        actor.combat = {4, 1};
        break;

    case MonsterType::Orc:
        actor.glyph = {'o', Colours::Orc};
        actor.health = {15, 15};
        actor.combat = {6, 2};
        break;

    case MonsterType::Zombie:
        actor.glyph = {'Z', Colours::Zombie};
        actor.health = {12, 12};
        actor.combat = {5, 1};
        break;

    case MonsterType::Skeleton:
        actor.glyph = {'s', Colours::Skeleton};
        actor.health = {9, 9};
        actor.combat = {5, 2};
        break;
    }

    return actor;
}

void populate_dungeon(Game &game)
{
    if (!game.current_level) return;

    game.actors.clear();
    const int depth = game.current_depth + 1;

    for (const Room &room : game.current_level->rooms)
    {
        const int monster_count = random_int(0, std::min(3 + depth / 2, 6));

        for (int i = 0; i < monster_count; ++i)
        {
            const int x = random_int(room.x + 1, room.x + room.width - 2);
            const int y = random_int(room.y + 1, room.y + room.height - 2);

            if (x == game.player.position.x && y == game.player.position.y)
            {
                continue;
            }

            MonsterType type;
            const int roll = random_int(0, 100);

            if (depth <= 2)
            {
                type = roll < 65 ? MonsterType::Rat : MonsterType::Goblin;
            }
            else if (depth <= 4)
            {
                type = roll < 45 ? MonsterType::Goblin : MonsterType::Orc;
            }
            else
            {
                type = roll < 50 ? MonsterType::Zombie : MonsterType::Skeleton;
            }

            game.actors.push_back(make_monster(game, type, x, y));
        }
    }
}

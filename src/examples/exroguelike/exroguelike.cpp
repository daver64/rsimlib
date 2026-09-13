// ============================================================================
// exroguelike.cpp
//
// A coloured ASCII roguelike example for simlib.
//
// Optional libraries:
//     rworld  - procedural overworld generation
//     recs    - entity component system
//
// This example deliberately lives in ONE source file so that it is easy to
// drop into simlib's examples directory and experiment with.
//
// Major features:
//
//     * 512 x 512 overworld
//     * coloured ASCII rendering
//     * procedural dungeons
//     * 3 x 3 dungeon sectors
//     * up to 9 rooms per dungeon level
//     * turn based movement
//     * monsters and combat
//     * field of view
//     * explored map
//     * point lights
//     * spotlights
//     * shadow-light prioritisation
//     * day/night ambient lighting
//
// ============================================================================

// ============================================================================
// INCLUDES
// ============================================================================

#include "sl.h"
#include "recs.h"

// #include "rworld.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <limits>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// ============================================================================
// CONFIGURATION
// ============================================================================

constexpr int SCREEN_WIDTH = 1280;
constexpr int SCREEN_HEIGHT = 800;

constexpr int CELL_WIDTH = 12;
constexpr int CELL_HEIGHT = 16;

static sl::Font *g_font = nullptr;

constexpr int MAP_X = 10;
constexpr int MAP_Y = 42;

constexpr int MAP_WIDTH = 80;
constexpr int MAP_HEIGHT = 42;

constexpr int UI_HEIGHT = 80;

// ============================================================================
// OVERWORLD
// ============================================================================

constexpr int WORLD_WIDTH = 512;
constexpr int WORLD_HEIGHT = 512;

// ============================================================================
// DUNGEONS
// ============================================================================

constexpr int DUNGEON_WIDTH = 78;
constexpr int DUNGEON_HEIGHT = 39;

constexpr int DUNGEON_SECTORS_X = 3;
constexpr int DUNGEON_SECTORS_Y = 3;

constexpr int DUNGEON_MAX_ROOMS = 9;

// ============================================================================
// GAMEPLAY
// ============================================================================

constexpr int PLAYER_FOV_RADIUS = 12;

constexpr int MAX_MESSAGES = 6;

constexpr int MAX_SHADOW_LIGHTS = 8;

// ============================================================================
// RANDOM
// ============================================================================

static std::mt19937 rng(
    static_cast<uint32_t>(std::time(nullptr)));

static int random_int(int minimum, int maximum)
{
    std::uniform_int_distribution<int> distribution(
        minimum,
        maximum);

    return distribution(rng);
}

static float random_float(float minimum, float maximum)
{
    std::uniform_real_distribution<float> distribution(
        minimum,
        maximum);

    return distribution(rng);
}

static bool random_chance(float chance)
{
    return random_float(0.0f, 1.0f) < chance;
}

// ============================================================================
// COLOURS
// ============================================================================

namespace Colours
{
    const sl::Colour Black{0, 0, 0};
    const sl::Colour White{255, 255, 255};
    const sl::Colour Grey{150, 150, 150};
    const sl::Colour DarkGrey{70, 70, 80};

    const sl::Colour Red{220, 60, 50};
    const sl::Colour DarkRed{110, 20, 20};

    const sl::Colour Green{60, 200, 80};
    const sl::Colour DarkGreen{20, 90, 40};

    const sl::Colour Blue{60, 120, 255};
    const sl::Colour DarkBlue{20, 30, 90};

    const sl::Colour Yellow{255, 220, 70};
    const sl::Colour Orange{255, 140, 40};

    const sl::Colour Brown{140, 90, 40};

    const sl::Colour Cyan{60, 220, 220};
    const sl::Colour Magenta{220, 70, 220};

    const sl::Colour Purple{150, 70, 220};

    const sl::Colour LightBlue{140, 190, 255};
    const sl::Colour LightGreen{130, 255, 130};

    const sl::Colour Gold{255, 200, 40};

    const sl::Colour Floor{110, 105, 95};
    const sl::Colour Wall{120, 120, 140};

    const sl::Colour Explored{45, 45, 55};

    const sl::Colour Player{255, 255, 255};

    const sl::Colour Goblin{80, 220, 80};
    const sl::Colour Orc{30, 150, 60};
    const sl::Colour Rat{150, 100, 60};

    const sl::Colour Zombie{110, 160, 90};
    const sl::Colour Skeleton{220, 220, 200};

    const sl::Colour Fire{255, 140, 30};

    const sl::Colour TorchLight{255, 150, 60};
    const sl::Colour BlueFire{80, 140, 255};
}

// ============================================================================
// COLOUR HELPERS
// ============================================================================

static sl::Colour multiply_colour(
    const sl::Colour &colour,
    float amount)
{
    return sl::Colour{
        static_cast<Uint8>(std::clamp(colour.red * amount, 0.0f, 255.0f)),
        static_cast<Uint8>(std::clamp(colour.green * amount, 0.0f, 255.0f)),
        static_cast<Uint8>(std::clamp(colour.blue * amount, 0.0f, 255.0f)),
        colour.alpha};
}

// ============================================================================
// ENUMERATIONS
// ============================================================================

enum class GameState
{
    Playing,

    Inventory,

    GameOver
};

enum class AreaType
{
    Overworld,

    Dungeon
};

enum class Terrain
{
    Unknown,

    DeepWater,
    Water,
    Beach,

    Plains,
    Grass,
    Forest,

    Hills,
    Mountain,

    Swamp,
    Snow,

    Ruins,

    DungeonEntrance,

    DungeonFloor,
    DungeonWall,

    Door,

    StairsUp,
    StairsDown,

    Torch,
    Brazier,

    Crystal
};

enum class MonsterType
{
    Rat,

    Goblin,

    Orc,

    Zombie,

    Skeleton
};

enum class LightType
{
    Point,

    Spotlight
};

enum class RoomType
{
    Empty,

    Normal,

    Large,

    Cavern,

    Crypt,

    Temple
};

// ============================================================================
// BASIC POSITION
// ============================================================================

struct Position
{
    int x = 0;
    int y = 0;
};

static bool operator==(
    const Position &a,
    const Position &b)
{
    return a.x == b.x &&
           a.y == b.y;
}

// ============================================================================
// TILE
// ============================================================================

struct Tile
{
    Terrain terrain = Terrain::Unknown;

    bool explored = false;

    bool visible = false;

    bool blocks_movement = false;

    bool blocks_sight = false;
};

// ============================================================================
// ROOM
// ============================================================================

struct Room
{
    int x = 0;
    int y = 0;

    int width = 0;
    int height = 0;

    RoomType type = RoomType::Normal;

    int centre_x() const
    {
        return x + width / 2;
    }

    int centre_y() const
    {
        return y + height / 2;
    }
};

// ============================================================================
// OVERWORLD
// ============================================================================

struct Overworld
{
    std::array<
        Tile,
        WORLD_WIDTH * WORLD_HEIGHT>
        tiles;

    Tile &at(
        int x,
        int y)
    {
        return tiles[y * WORLD_WIDTH + x];
    }

    const Tile &at(
        int x,
        int y) const
    {
        return tiles[y * WORLD_WIDTH + x];
    }

    bool inside(
        int x,
        int y) const
    {
        return x >= 0 &&
               y >= 0 &&
               x < WORLD_WIDTH &&
               y < WORLD_HEIGHT;
    }
};

// ============================================================================
// DUNGEON LEVEL
// ============================================================================

struct DungeonLevel
{
    int width = DUNGEON_WIDTH;
    int height = DUNGEON_HEIGHT;

    std::vector<Tile> tiles;

    std::vector<Room> rooms;

    Position stairs_up;
    Position stairs_down;

    DungeonLevel(int w = DUNGEON_WIDTH, int h = DUNGEON_HEIGHT)
        : width(w), height(h)
    {
        if (width > 0 && height > 0)
        {
            const std::size_t sw = static_cast<std::size_t>(width);
            const std::size_t sh = static_cast<std::size_t>(height);
            if (sw <= std::numeric_limits<std::size_t>::max() / sh)
            {
                tiles.resize(sw * sh);
            }
        }
    }

    Tile &at(
        int x,
        int y)
    {
        return tiles[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
    }

    const Tile &at(
        int x,
        int y) const
    {
        return tiles[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
    }

    bool inside(
        int x,
        int y) const
    {
        return x >= 0 &&
               y >= 0 &&
               x < width &&
               y < height;
    }
};

// ============================================================================
// DUNGEON
// ============================================================================

struct Dungeon
{
    int id = 0;

    Position overworld_position;

    std::vector<
        std::unique_ptr<DungeonLevel>>
        levels;
};

// ============================================================================
// CAMERA
// ============================================================================

struct Camera
{
    int x = 0;
    int y = 0;
};

// ============================================================================
// GAME LIGHT
//
// These are GAME lights.
//
// They are later translated to simlib lights.
//
// The first 8 selected lights are intended to use simlib's shadow casting
// light slots.
// ============================================================================

struct GameLight
{
    LightType type =
        LightType::Point;

    float x = 0.0f;
    float y = 0.0f;

    float radius = 100.0f;

    sl::Colour colour;

    float intensity = 1.0f;

    float direction = 0.0f;

    float cone_angle = 45.0f;

    float priority = 0.0f;

    bool cast_shadows = false;

    bool flicker = false;

    bool active = true;

    int owner = -1;
};

// ============================================================================
// LIGHTING SYSTEM
// ============================================================================

struct LightingSystem
{
    std::vector<GameLight> lights;

    std::array<
        int,
        MAX_SHADOW_LIGHTS>
        shadow_slots;

    LightingSystem()
    {
        shadow_slots.fill(-1);
    }

    void clear()
    {
        lights.clear();

        shadow_slots.fill(-1);
    }

    int add_point_light(
        float x,
        float y,
        float radius,
        const sl::Colour &colour,
        float priority,
        bool shadows,
        bool flicker)
    {
        GameLight light;

        light.type =
            LightType::Point;

        light.x = x;
        light.y = y;

        light.radius = radius;

        light.colour = colour;

        light.priority = priority;

        light.cast_shadows = shadows;

        light.flicker = flicker;

        lights.push_back(light);

        return static_cast<int>(
            lights.size() - 1);
    }

    int add_spotlight(
        float x,
        float y,

        float radius,

        float direction,

        float cone_angle,

        const sl::Colour &colour,

        float priority,

        bool shadows)
    {
        GameLight light;

        light.type =
            LightType::Spotlight;

        light.x = x;
        light.y = y;

        light.radius = radius;

        light.direction = direction;

        light.cone_angle = cone_angle;

        light.colour = colour;

        light.priority = priority;

        light.cast_shadows = shadows;

        lights.push_back(light);

        return static_cast<int>(
            lights.size() - 1);
    }

    void update(
        float player_x,
        float player_y,
        float time)
    {
        std::vector<
            std::pair<float, int>>
            candidates;

        for (
            int i = 0;
            i < static_cast<int>(lights.size());
            ++i)
        {
            GameLight &light =
                lights[i];

            if (!light.active)
            {
                continue;
            }

            if (light.flicker)
            {
                const float flicker =
                    std::sin(
                        time * 8.0f +
                        i * 1.37f) *
                    0.08f;

                light.intensity =
                    1.0f + flicker;
            }

            if (!light.cast_shadows)
            {
                continue;
            }

            const float dx =
                light.x - player_x;

            const float dy =
                light.y - player_y;

            const float distance =
                std::sqrt(
                    dx * dx +
                    dy * dy);

            const float score =
                light.priority -
                distance * 0.05f;

            candidates.emplace_back(
                score,
                i);
        }

        std::sort(
            candidates.begin(),
            candidates.end(),

            [](
                const auto &a,
                const auto &b)
            {
                return a.first >
                       b.first;
            });

        shadow_slots.fill(-1);

        for (
            int i = 0;

            i < MAX_SHADOW_LIGHTS &&
            i < static_cast<int>(
                    candidates.size());

            ++i)
        {
            shadow_slots[i] =
                candidates[i].second;
        }
    }

    bool uses_shadow_slot(
        int light_index) const
    {
        for (
            int slot :
            shadow_slots)
        {
            if (
                slot ==
                light_index)
            {
                return true;
            }
        }

        return false;
    }
};

// ============================================================================
// ECS-LIKE COMPONENT DATA
//
// The actual RECS calls should be concentrated in the entity creation and
// iteration sections below.
//
// Keeping component definitions ordinary and simple makes the game architecture
// easy to understand even when reading this as one source file.
// ============================================================================

struct Glyph
{
    char character = '?';

    sl::Colour foreground =
        Colours::White;
};

struct Health
{
    int current = 1;

    int maximum = 1;
};

struct Combat
{
    int attack = 1;

    int defence = 0;
};

struct Monster
{
    MonsterType type =
        MonsterType::Rat;
};

struct PlayerTag
{
};

struct MonsterTag
{
};

struct BlocksMovement
{
};

struct LightSource
{
    int light_index = -1;
};

struct Spotlight
{
    float direction = 0.0f;

    float cone_angle = 45.0f;
};

// ============================================================================
// SIMPLE ACTOR
//
// This is deliberately used as a fallback-friendly layer.
//
// If you want to move everything directly into RECS iteration, this structure
// is the exact section to replace.
//
// The game design still maps directly onto ECS components.
// ============================================================================

struct Actor
{
    int id = 0;

    Position position;

    Glyph glyph;

    Health health;

    Combat combat;

    bool player = false;

    bool blocks_movement = true;

    bool alive = true;

    MonsterType monster_type =
        MonsterType::Rat;
};

// ============================================================================
// GAME
// ============================================================================

struct Game
{
    GameState state =
        GameState::Playing;

    AreaType area_type =
        AreaType::Overworld;

    Overworld overworld;

    std::vector<
        std::unique_ptr<Dungeon>>
        dungeons;

    DungeonLevel *current_level =
        nullptr;

    int current_dungeon = -1;

    int current_depth = 0;

    Position player_position;

    Actor player;

    std::vector<Actor> actors;

    Camera camera;

    LightingSystem lighting;

    std::deque<std::string>
        messages;

    uint64_t turn = 0;

    float game_time = 0.0f;

    bool night = false;

    int next_actor_id = 1;

    Game()
    {
        player.id =
            next_actor_id++;

        player.player = true;

        player.position =
            {256, 256};

        player.glyph.character =
            '@';

        player.glyph.foreground =
            Colours::Player;

        player.health =
            {30, 30};

        player.combat =
            {7, 2};

        player_position =
            player.position;
    }

    void message(
        const std::string &text)
    {
        messages.push_front(text);

        while (
            messages.size() >
            MAX_MESSAGES)
        {
            messages.pop_back();
        }
    }
};

// ============================================================================
// TERRAIN HELPERS
// ============================================================================

static bool terrain_blocks_movement(
    Terrain terrain)
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

static bool terrain_blocks_sight(
    Terrain terrain)
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

// ============================================================================
// TERRAIN GLYPH
// ============================================================================

static char terrain_glyph(
    Terrain terrain)
{
    switch (terrain)
    {
    case Terrain::DeepWater:
        return '~';

    case Terrain::Water:
        return '~';

    case Terrain::Beach:
        return ',';

    case Terrain::Plains:
        return '.';

    case Terrain::Grass:
        return '"';

    case Terrain::Forest:
        return 'T';

    case Terrain::Hills:
        return '^';

    case Terrain::Mountain:
        return '^';

    case Terrain::Swamp:
        return ';';

    case Terrain::Snow:
        return '*';

    case Terrain::Ruins:
        return '&';

    case Terrain::DungeonEntrance:
        return '>';

    case Terrain::DungeonFloor:
        return '.';

    case Terrain::DungeonWall:
        return '#';

    case Terrain::Door:
        return '+';

    case Terrain::StairsUp:
        return '<';

    case Terrain::StairsDown:
        return '>';

    case Terrain::Torch:
        return 't';

    case Terrain::Brazier:
        return 'B';

    case Terrain::Crystal:
        return '*';

    default:
        return ' ';
    }
}

// ============================================================================
// TERRAIN COLOUR
// ============================================================================

static sl::Colour terrain_colour(
    Terrain terrain)
{
    switch (terrain)
    {
    case Terrain::DeepWater:
        return Colours::DarkBlue;

    case Terrain::Water:
        return Colours::Blue;

    case Terrain::Beach:
        return Colours::Yellow;

    case Terrain::Plains:
        return Colours::LightGreen;

    case Terrain::Grass:
        return Colours::Green;

    case Terrain::Forest:
        return Colours::DarkGreen;

    case Terrain::Hills:
        return Colours::Brown;

    case Terrain::Mountain:
        return Colours::Grey;

    case Terrain::Swamp:
        return Colours::DarkGreen;

    case Terrain::Snow:
        return Colours::White;

    case Terrain::Ruins:
        return Colours::Grey;

    case Terrain::DungeonEntrance:
        return Colours::Gold;

    case Terrain::DungeonFloor:
        return Colours::Floor;

    case Terrain::DungeonWall:
        return Colours::Wall;

    case Terrain::Door:
        return Colours::Brown;

    case Terrain::StairsUp:
        return Colours::Cyan;

    case Terrain::StairsDown:
        return Colours::Gold;

    case Terrain::Torch:
        return Colours::Yellow;

    case Terrain::Brazier:
        return Colours::Orange;

    case Terrain::Crystal:
        return Colours::Magenta;

    default:
        return Colours::White;
    }
}

// ============================================================================
// INITIALISE TILE
// ============================================================================

static void set_tile(
    Tile &tile,
    Terrain terrain)
{
    tile.terrain =
        terrain;

    tile.blocks_movement =
        terrain_blocks_movement(
            terrain);

    tile.blocks_sight =
        terrain_blocks_sight(
            terrain);
}

// ============================================================================
// OVERWORLD GENERATION
//
// This is intentionally a simple fallback generator.
//
// Replace generate_overworld() internals with your rworld calls once the exact
// current API is wired in.
//
// The game expects the final result to populate:
//     WORLD_WIDTH x WORLD_HEIGHT
//
// terrain categories.
// ============================================================================

static void generate_overworld(
    Game &game)
{
    for (
        int y = 0;
        y < WORLD_HEIGHT;
        ++y)
    {
        for (
            int x = 0;
            x < WORLD_WIDTH;
            ++x)
        {
            const float nx =
                static_cast<float>(x) /
                WORLD_WIDTH;

            const float ny =
                static_cast<float>(y) /
                WORLD_HEIGHT;

            const float waves =
                std::sin(nx * 18.0f) *
                    0.25f +

                std::cos(ny * 13.0f) *
                    0.25f +

                std::sin(
                    nx * 7.0f +
                    ny * 11.0f) *
                    0.20f;

            const float noise =
                waves +
                random_float(
                    -0.15f,
                    0.15f);

            Terrain terrain;

            if (noise < -0.42f)
            {
                terrain =
                    Terrain::DeepWater;
            }

            else if (noise < -0.28f)
            {
                terrain =
                    Terrain::Water;
            }

            else if (noise < -0.20f)
            {
                terrain =
                    Terrain::Beach;
            }

            else if (noise < 0.10f)
            {
                terrain =
                    Terrain::Plains;
            }

            else if (noise < 0.28f)
            {
                terrain =
                    Terrain::Grass;
            }

            else if (noise < 0.48f)
            {
                terrain =
                    Terrain::Forest;
            }

            else if (noise < 0.68f)
            {
                terrain =
                    Terrain::Hills;
            }

            else
            {
                terrain =
                    Terrain::Mountain;
            }

            set_tile(
                game.overworld.at(x, y),
                terrain);
        }
    }

    // ------------------------------------------------------------------------
    // Guarantee a playable area around the player.
    // ------------------------------------------------------------------------

    for (
        int y = 246;
        y <= 266;
        ++y)
    {
        for (
            int x = 246;
            x <= 266;
            ++x)
        {
            set_tile(
                game.overworld.at(x, y),
                Terrain::Grass);
        }
    }

    // ------------------------------------------------------------------------
    // Generate dungeon entrances.
    // ------------------------------------------------------------------------

    int entrances = 0;

    while (entrances < 24)
    {
        const int x =
            random_int(
                10,
                WORLD_WIDTH - 11);

        const int y =
            random_int(
                10,
                WORLD_HEIGHT - 11);

        Tile &tile =
            game.overworld.at(
                x,
                y);

        if (
            tile.terrain ==
                Terrain::Mountain ||

            tile.terrain ==
                Terrain::Forest ||

            tile.terrain ==
                Terrain::Hills)
        {
            set_tile(
                tile,
                Terrain::DungeonEntrance);

            ++entrances;
        }
    }

    game.player.position =
        {256, 256};

    game.player_position =
        game.player.position;

    game.message(
        "You enter the wilderness.");
}

// ============================================================================
// DUNGEON INITIALISATION
// ============================================================================

static void initialise_dungeon(
    DungeonLevel &level)
{
    for (
        int y = 0;
        y < level.height;
        ++y)
    {
        for (
            int x = 0;
            x < level.width;
            ++x)
        {
            set_tile(
                level.at(x, y),
                Terrain::DungeonWall);
        }
    }
}

// ============================================================================
// CARVE FLOOR
// ============================================================================

static void carve_floor(
    DungeonLevel &level,
    int x,
    int y)
{
    if (!level.inside(x, y))
    {
        return;
    }

    set_tile(
        level.at(x, y),
        Terrain::DungeonFloor);
}

// ============================================================================
// CARVE ROOM
// ============================================================================

static void carve_room(
    DungeonLevel &level,
    const Room &room)
{
    for (
        int y = room.y;
        y < room.y + room.height;
        ++y)
    {
        for (
            int x = room.x;
            x < room.x + room.width;
            ++x)
        {
            carve_floor(
                level,
                x,
                y);
        }
    }
}

// ============================================================================
// CARVE CORRIDOR
// ============================================================================

static void carve_horizontal(
    DungeonLevel &level,

    int x1,
    int x2,

    int y)
{
    if (x1 > x2)
    {
        std::swap(
            x1,
            x2);
    }

    for (
        int x = x1;
        x <= x2;
        ++x)
    {
        carve_floor(
            level,
            x,
            y);
    }
}

static void carve_vertical(
    DungeonLevel &level,

    int y1,
    int y2,

    int x)
{
    if (y1 > y2)
    {
        std::swap(
            y1,
            y2);
    }

    for (
        int y = y1;
        y <= y2;
        ++y)
    {
        carve_floor(
            level,
            x,
            y);
    }
}

static void connect_rooms(
    DungeonLevel &level,

    const Room &a,
    const Room &b)
{
    const int x1 =
        a.centre_x();

    const int y1 =
        a.centre_y();

    const int x2 =
        b.centre_x();

    const int y2 =
        b.centre_y();

    if (random_chance(0.5f))
    {
        carve_horizontal(
            level,
            x1,
            x2,
            y1);

        carve_vertical(
            level,
            y1,
            y2,
            x2);
    }

    else
    {
        carve_vertical(
            level,
            y1,
            y2,
            x1);

        carve_horizontal(
            level,
            x1,
            x2,
            y2);
    }
}

// ============================================================================
// ROOM CREATION IN A SECTOR
// ============================================================================

static Room make_room_in_sector(
    int sector_x,
    int sector_y)
{
    const int sector_width =
        DUNGEON_WIDTH /
        DUNGEON_SECTORS_X;

    const int sector_height =
        DUNGEON_HEIGHT /
        DUNGEON_SECTORS_Y;

    const int sx =
        sector_x *
        sector_width;

    const int sy =
        sector_y *
        sector_height;

    const int padding = 2;

    const int maximum_width =
        sector_width -
        padding * 2;

    const int maximum_height =
        sector_height -
        padding * 2;

    Room room;

    room.width =
        random_int(
            std::max(6, maximum_width / 2),
            maximum_width);

    room.height =
        random_int(
            5,
            std::max(
                5,
                maximum_height));

    room.x =
        sx +
        random_int(
            padding,
            std::max(
                padding,
                sector_width -
                    room.width -
                    padding));

    room.y =
        sy +
        random_int(
            padding,
            std::max(
                padding,
                sector_height -
                    room.height -
                    padding));

    const int type =
        random_int(
            0,
            9);

    if (type == 0)
    {
        room.type =
            RoomType::Crypt;
    }

    else if (type == 1)
    {
        room.type =
            RoomType::Temple;
    }

    else if (type == 2)
    {
        room.type =
            RoomType::Large;
    }

    else
    {
        room.type =
            RoomType::Normal;
    }

    return room;
}

// ============================================================================
// DUNGEON GENERATION
//
// Up to one room in each of the 3 x 3 sectors.
//
// Therefore:
//
//     minimum 1 room
//     maximum 9 rooms
//
// ============================================================================

static std::unique_ptr<DungeonLevel>
generate_dungeon_level(
    int depth)
{
    auto level =
        std::make_unique<
            DungeonLevel>();

    initialise_dungeon(
        *level);

    const int room_count =
        random_int(
            5,
            DUNGEON_MAX_ROOMS);

    std::array<
        bool,
        DUNGEON_MAX_ROOMS>
        used_sectors{};

    used_sectors.fill(false);

    for (
        int room_index = 0;
        room_index < room_count;
        ++room_index)
    {
        int sector;

        do
        {
            sector =
                random_int(
                    0,
                    DUNGEON_MAX_ROOMS - 1);
        } while (
            used_sectors[sector]);

        used_sectors[sector] = true;

        const int sector_x =
            sector %
            DUNGEON_SECTORS_X;

        const int sector_y =
            sector /
            DUNGEON_SECTORS_X;

        Room room =
            make_room_in_sector(
                sector_x,
                sector_y);

        carve_room(
            *level,
            room);

        level->rooms.push_back(
            room);
    }

    // ------------------------------------------------------------------------
    // Ensure at least one room.
    // ------------------------------------------------------------------------

    if (level->rooms.empty())
    {
        Room room;

        room.x = 5;
        room.y = 5;

        room.width = 12;
        room.height = 8;

        carve_room(
            *level,
            room);

        level->rooms.push_back(
            room);
    }

    // ------------------------------------------------------------------------
    // Connect rooms.
    // ------------------------------------------------------------------------

    for (
        size_t i = 1;
        i < level->rooms.size();
        ++i)
    {
        connect_rooms(
            *level,

            level->rooms[i - 1],

            level->rooms[i]);
    }

    // ------------------------------------------------------------------------
    // Add stairs.
    // ------------------------------------------------------------------------

    {
        const Room &room =
            level->rooms.front();

        level->stairs_up =
            {
                room.centre_x(),
                room.centre_y()};

        set_tile(
            level->at(
                level->stairs_up.x,
                level->stairs_up.y),
            Terrain::StairsUp);
    }

    {
        const Room &room =
            level->rooms.back();

        level->stairs_down =
            {
                room.centre_x(),
                room.centre_y()};

        set_tile(
            level->at(
                level->stairs_down.x,
                level->stairs_down.y),
            Terrain::StairsDown);
    }

    // ------------------------------------------------------------------------
    // Room decoration.
    // ------------------------------------------------------------------------

    for (
        const Room &room :
        level->rooms)
    {
        // ------------------------------------------------------------
        // Torches.
        // ------------------------------------------------------------

        if (random_chance(0.75f))
        {
            const int x =
                room.x + 1;

            const int y =
                room.centre_y();

            set_tile(
                level->at(x, y),
                Terrain::Torch);
        }

        if (random_chance(0.60f))
        {
            const int x =
                room.x +
                room.width -
                2;

            const int y =
                room.centre_y();

            set_tile(
                level->at(x, y),
                Terrain::Torch);
        }

        // ------------------------------------------------------------
        // Special rooms.
        // ------------------------------------------------------------

        if (
            room.type ==
            RoomType::Crypt)
        {
            const int x =
                room.centre_x();

            const int y =
                room.centre_y();

            set_tile(
                level->at(x, y),
                Terrain::Brazier);
        }

        if (
            room.type ==
            RoomType::Temple)
        {
            const int x =
                room.centre_x();

            const int y =
                room.centre_y();

            set_tile(
                level->at(x, y),
                Terrain::Crystal);
        }
    }

    return level;
}

// ============================================================================
// FIND DUNGEON
// ============================================================================

static Dungeon *
find_dungeon_at(
    Game &game,

    int x,
    int y)
{
    for (
        auto &dungeon :
        game.dungeons)
    {
        if (
            dungeon->overworld_position.x ==
                x &&

            dungeon->overworld_position.y ==
                y)
        {
            return dungeon.get();
        }
    }

    return nullptr;
}

// ============================================================================
// ENTER DUNGEON
// ============================================================================

static void enter_dungeon(
    Game &game)
{
    const Position position =
        game.player.position;

    Dungeon *dungeon =
        find_dungeon_at(
            game,
            position.x,
            position.y);

    if (!dungeon)
    {
        auto new_dungeon =
            std::make_unique<
                Dungeon>();

        new_dungeon->id =
            static_cast<int>(
                game.dungeons.size());

        new_dungeon
            ->overworld_position =
            position;

        new_dungeon
            ->levels.push_back(
                generate_dungeon_level(
                    1));

        dungeon =
            new_dungeon.get();

        game.dungeons.push_back(
            std::move(
                new_dungeon));
    }

    game.current_dungeon =
        dungeon->id;

    game.current_depth =
        0;

    game.current_level =
        dungeon->levels[0].get();

    game.area_type =
        AreaType::Dungeon;

    game.player.position =
        game.current_level
            ->stairs_up;

    game.player_position =
        game.player.position;

    game.message(
        "You descend into darkness.");
}

// ============================================================================
// DESCEND DUNGEON
// ============================================================================

static void descend_dungeon(
    Game &game)
{
    if (
        game.current_dungeon < 0)
    {
        return;
    }

    Dungeon &dungeon =
        *game.dungeons[game.current_dungeon];

    const int next_depth =
        game.current_depth + 1;

    if (
        next_depth >=
        static_cast<int>(
            dungeon.levels.size()))
    {
        dungeon.levels.push_back(
            generate_dungeon_level(
                next_depth + 1));
    }

    game.current_depth =
        next_depth;

    game.current_level =
        dungeon.levels[next_depth].get();

    game.player.position =
        game.current_level
            ->stairs_up;

    game.player_position =
        game.player.position;

    game.message(
        "You descend deeper.");
}

// ============================================================================
// ASCEND DUNGEON
// ============================================================================

static void ascend_dungeon(
    Game &game)
{
    if (
        game.current_depth > 0)
    {
        Dungeon &dungeon =
            *game.dungeons[game.current_dungeon];

        --game.current_depth;

        game.current_level =
            dungeon.levels[game.current_depth].get();

        game.player.position =
            game.current_level
                ->stairs_down;

        game.player_position =
            game.player.position;

        game.message(
            "You climb upwards.");

        return;
    }

    Dungeon &dungeon =
        *game.dungeons[game.current_dungeon];

    game.area_type =
        AreaType::Overworld;

    game.current_level =
        nullptr;

    game.player.position =
        dungeon.overworld_position;

    game.player_position =
        game.player.position;

    game.current_dungeon =
        -1;

    game.message(
        "You return to the wilderness.");
}

// ============================================================================
// ACTOR CREATION
// ============================================================================

static Actor make_monster(
    Game &game,

    MonsterType type,

    int x,
    int y)
{
    Actor actor;

    actor.id =
        game.next_actor_id++;

    actor.position =
        {x, y};

    actor.monster_type =
        type;

    switch (type)
    {
    case MonsterType::Rat:

        actor.glyph =
            {
                'r',
                Colours::Rat};

        actor.health =
            {4, 4};

        actor.combat =
            {2, 0};

        break;

    case MonsterType::Goblin:

        actor.glyph =
            {
                'g',
                Colours::Goblin};

        actor.health =
            {8, 8};

        actor.combat =
            {4, 1};

        break;

    case MonsterType::Orc:

        actor.glyph =
            {
                'o',
                Colours::Orc};

        actor.health =
            {15, 15};

        actor.combat =
            {6, 2};

        break;

    case MonsterType::Zombie:

        actor.glyph =
            {
                'Z',
                Colours::Zombie};

        actor.health =
            {12, 12};

        actor.combat =
            {5, 1};

        break;

    case MonsterType::Skeleton:

        actor.glyph =
            {
                's',
                Colours::Skeleton};

        actor.health =
            {9, 9};

        actor.combat =
            {5, 2};

        break;
    }

    return actor;
}

// ============================================================================
// SPAWN MONSTERS
// ============================================================================

static void populate_dungeon(
    Game &game)
{
    if (
        !game.current_level)
    {
        return;
    }

    game.actors.clear();

    const int depth =
        game.current_depth + 1;

    for (
        const Room &room :
        game.current_level->rooms)
    {
        const int monster_count =
            random_int(
                0,
                std::min(
                    3 + depth / 2,
                    6));

        for (
            int i = 0;
            i < monster_count;
            ++i)
        {
            const int x =
                random_int(
                    room.x + 1,
                    room.x +
                        room.width -
                        2);

            const int y =
                random_int(
                    room.y + 1,
                    room.y +
                        room.height -
                        2);

            if (
                x ==
                    game.player.position.x &&

                y ==
                    game.player.position.y)
            {
                continue;
            }

            MonsterType type;

            const int roll =
                random_int(
                    0,
                    100);

            if (
                depth <= 2)
            {
                type =
                    roll < 65
                        ? MonsterType::Rat
                        : MonsterType::Goblin;
            }

            else if (
                depth <= 4)
            {
                type =
                    roll < 45
                        ? MonsterType::Goblin
                        : MonsterType::Orc;
            }

            else
            {
                type =
                    roll < 50
                        ? MonsterType::Zombie
                        : MonsterType::Skeleton;
            }

            game.actors.push_back(
                make_monster(
                    game,
                    type,
                    x,
                    y));
        }
    }
}

// ============================================================================
// CURRENT TILE
// ============================================================================

static Tile *
current_tile(
    Game &game,

    int x,
    int y)
{
    if (
        game.area_type ==
        AreaType::Overworld)
    {
        if (
            !game.overworld.inside(
                x,
                y))
        {
            return nullptr;
        }

        return &game.overworld.at(
            x,
            y);
    }

    if (
        !game.current_level)
    {
        return nullptr;
    }

    if (
        !game.current_level->inside(
            x,
            y))
    {
        return nullptr;
    }

    return &game.current_level->at(
        x,
        y);
}

// ============================================================================
// ACTOR AT
// ============================================================================

static Actor *
actor_at(
    Game &game,

    int x,
    int y)
{
    for (
        Actor &actor :
        game.actors)
    {
        if (
            actor.alive &&

            actor.blocks_movement &&

            actor.position.x == x &&

            actor.position.y == y)
        {
            return &actor;
        }
    }

    return nullptr;
}

// ============================================================================
// ATTACK
// ============================================================================

static void attack(
    Game &game,

    Actor &attacker,

    Actor &defender)
{
    int damage =
        attacker.combat.attack +
        random_int(
            -1,
            2) -
        defender.combat.defence;

    damage =
        std::max(
            damage,
            1);

    defender.health.current -=
        damage;

    {
        std::stringstream stream;

        stream
            << attacker.glyph.character
            << " hits "
            << defender.glyph.character
            << " for "
            << damage
            << " damage.";

        game.message(
            stream.str());
    }

    if (
        defender.health.current <= 0)
    {
        defender.alive = false;

        if (defender.player)
        {
            game.state =
                GameState::GameOver;

            game.message(
                "You have died.");
        }

        else
        {
            game.message(
                "The creature dies.");
        }
    }
}

// ============================================================================
// TRY PLAYER MOVE
// ============================================================================

static bool try_player_move(
    Game &game,

    int dx,
    int dy)
{
    if (
        game.state !=
        GameState::Playing)
    {
        return false;
    }

    const int x =
        game.player.position.x +
        dx;

    const int y =
        game.player.position.y +
        dy;

    Tile *tile =
        current_tile(
            game,
            x,
            y);

    if (!tile)
    {
        return false;
    }

    Actor *actor =
        actor_at(
            game,
            x,
            y);

    if (actor)
    {
        attack(
            game,
            game.player,
            *actor);

        return true;
    }

    if (
        tile->blocks_movement)
    {
        return false;
    }

    game.player.position =
        {x, y};

    game.player_position =
        game.player.position;

    return true;
}

// ============================================================================
// LINE OF SIGHT
//
// Bresenham line.
//
// Used for simple FOV and monster vision.
// ============================================================================

static bool line_of_sight(
    Game &game,

    int x0,
    int y0,

    int x1,
    int y1)
{
    int dx =
        std::abs(
            x1 - x0);

    int dy =
        std::abs(
            y1 - y0);

    int sx =
        x0 < x1
            ? 1
            : -1;

    int sy =
        y0 < y1
            ? 1
            : -1;

    int error =
        dx - dy;

    int x = x0;
    int y = y0;

    while (true)
    {
        if (
            x == x1 &&
            y == y1)
        {
            return true;
        }

        if (
            x != x0 ||
            y != y0)
        {
            Tile *tile =
                current_tile(
                    game,
                    x,
                    y);

            if (
                !tile ||
                tile->blocks_sight)
            {
                return false;
            }
        }

        const int error2 =
            error * 2;

        if (
            error2 > -dy)
        {
            error -= dy;

            x += sx;
        }

        if (
            error2 < dx)
        {
            error += dx;

            y += sy;
        }
    }
}

// ============================================================================
// CLEAR VISIBILITY
// ============================================================================

static void clear_visibility(
    Game &game)
{
    if (
        game.area_type ==
        AreaType::Overworld)
    {
        for (
            Tile &tile :
            game.overworld.tiles)
        {
            tile.visible = false;
        }
    }

    else if (
        game.current_level)
    {
        for (
            Tile &tile :
            game.current_level->tiles)
        {
            tile.visible = false;
        }
    }
}

// ============================================================================
// FOV
// ============================================================================

static void update_fov(
    Game &game)
{
    clear_visibility(
        game);

    const Position origin =
        game.player.position;

    for (
        int y =
            origin.y -
            PLAYER_FOV_RADIUS;

        y <=
        origin.y +
            PLAYER_FOV_RADIUS;

        ++y)
    {
        for (
            int x =
                origin.x -
                PLAYER_FOV_RADIUS;

            x <=
            origin.x +
                PLAYER_FOV_RADIUS;

            ++x)
        {
            const int dx =
                x - origin.x;

            const int dy =
                y - origin.y;

            if (
                dx * dx +
                    dy * dy >

                PLAYER_FOV_RADIUS *
                    PLAYER_FOV_RADIUS)
            {
                continue;
            }

            Tile *tile =
                current_tile(
                    game,
                    x,
                    y);

            if (!tile)
            {
                continue;
            }

            if (
                line_of_sight(
                    game,

                    origin.x,
                    origin.y,

                    x,
                    y))
            {
                tile->visible =
                    true;

                tile->explored =
                    true;
            }
        }
    }
}

// ============================================================================
// MONSTER TURN
// ============================================================================

static void monster_turns(
    Game &game)
{
    if (
        game.area_type !=
        AreaType::Dungeon)
    {
        return;
    }

    for (
        Actor &monster :
        game.actors)
    {
        if (!monster.alive)
        {
            continue;
        }

        const int dx =
            game.player.position.x -
            monster.position.x;

        const int dy =
            game.player.position.y -
            monster.position.y;

        const int distance_squared =
            dx * dx +
            dy * dy;

        if (
            distance_squared <= 2)
        {
            attack(
                game,
                monster,
                game.player);

            continue;
        }

        if (
            distance_squared <= 100 &&
            line_of_sight(
                game,

                monster.position.x,
                monster.position.y,

                game.player.position.x,
                game.player.position.y))
        {
            const int move_x =
                dx == 0
                    ? 0
                : dx > 0
                    ? 1
                    : -1;

            const int move_y =
                dy == 0
                    ? 0
                : dy > 0
                    ? 1
                    : -1;

            const int target_x =
                monster.position.x +
                move_x;

            const int target_y =
                monster.position.y +
                move_y;

            Tile *tile =
                current_tile(
                    game,
                    target_x,
                    target_y);

            if (
                tile &&
                !tile->blocks_movement &&
                !actor_at(
                    game,
                    target_x,
                    target_y))
            {
                if (
                    target_x !=
                        game.player.position.x ||

                    target_y !=
                        game.player.position.y)
                {
                    monster.position =
                        {
                            target_x,
                            target_y};
                }
            }
        }

        else if (
            random_chance(0.25f))
        {
            const int move_x =
                random_int(
                    -1,
                    1);

            const int move_y =
                random_int(
                    -1,
                    1);

            Tile *tile =
                current_tile(
                    game,

                    monster.position.x +
                        move_x,

                    monster.position.y +
                        move_y);

            if (
                tile &&
                !tile->blocks_movement)
            {
                monster.position.x +=
                    move_x;

                monster.position.y +=
                    move_y;
            }
        }
    }
}

// ============================================================================
// ADVANCE TURN
// ============================================================================

static void advance_turn(
    Game &game)
{
    ++game.turn;

    monster_turns(
        game);

    update_fov(
        game);
}

// ============================================================================
// ENTER / DESCEND / ASCEND
// ============================================================================

static bool use_stairs(
    Game &game)
{
    Tile *tile =
        current_tile(
            game,

            game.player.position.x,
            game.player.position.y);

    if (!tile)
    {
        return false;
    }

    if (
        game.area_type ==
        AreaType::Overworld)
    {
        if (
            tile->terrain ==
            Terrain::DungeonEntrance)
        {
            enter_dungeon(
                game);

            populate_dungeon(
                game);

            update_fov(
                game);

            return true;
        }
    }

    else
    {
        if (
            tile->terrain ==
            Terrain::StairsDown)
        {
            descend_dungeon(
                game);

            populate_dungeon(
                game);

            update_fov(
                game);

            return true;
        }

        if (
            tile->terrain ==
            Terrain::StairsUp)
        {
            ascend_dungeon(
                game);

            populate_dungeon(
                game);

            update_fov(
                game);

            return true;
        }
    }

    return false;
}

// ============================================================================
// CAMERA
// ============================================================================

static void update_camera(
    Game &game)
{
    game.camera.x =
        game.player.position.x -
        MAP_WIDTH / 2;

    game.camera.y =
        game.player.position.y -
        MAP_HEIGHT / 2;

    if (
        game.area_type ==
        AreaType::Overworld)
    {
        game.camera.x =
            std::clamp(
                game.camera.x,

                0,

                WORLD_WIDTH -
                    MAP_WIDTH);

        game.camera.y =
            std::clamp(
                game.camera.y,

                0,

                WORLD_HEIGHT -
                    MAP_HEIGHT);
    }

    else if (
        game.current_level)
    {
        game.camera.x =
            std::clamp(
                game.camera.x,

                0,

                std::max(
                    0,

                    game.current_level
                            ->width -
                        MAP_WIDTH));

        game.camera.y =
            std::clamp(
                game.camera.y,

                0,

                std::max(
                    0,

                    game.current_level
                            ->height -
                        MAP_HEIGHT));
    }
}

// ============================================================================
// BUILD GAME LIGHTS
//
// This collects lights from:
//
//     player torch
//     dungeon torches
//     braziers
//     crystals
//     special spotlight effects
//
// The LightingSystem then selects the best 8 shadow casting lights.
// ============================================================================

static void build_lights(
    Game &game)
{
    game.lighting.clear();

    // ------------------------------------------------------------------------
    // PLAYER TORCH
    //
    // Highest priority.
    //
    // This should almost always become shadow slot 0.
    // ------------------------------------------------------------------------

    game.lighting.add_point_light(

        static_cast<float>(
            game.player.position.x),

        static_cast<float>(
            game.player.position.y),

        9.0f,

        Colours::TorchLight,

        1000.0f,

        true,

        true);

    // ------------------------------------------------------------------------
    // DUNGEON LIGHTS
    // ------------------------------------------------------------------------

    if (
        game.area_type ==
            AreaType::Dungeon &&

        game.current_level)
    {
        for (
            int y = 0;
            y <
            game.current_level
                ->height;
            ++y)
        {
            for (
                int x = 0;
                x <
                game.current_level
                    ->width;
                ++x)
            {
                const Tile &tile =
                    game.current_level
                        ->at(x, y);

                switch (
                    tile.terrain)
                {
                case Terrain::Torch:

                    game.lighting
                        .add_point_light(

                            x + 0.5f,

                            y + 0.5f,

                            7.0f,

                            Colours::TorchLight,

                            250.0f,

                            true,

                            true);

                    break;

                case Terrain::Brazier:

                    game.lighting
                        .add_point_light(

                            x + 0.5f,

                            y + 0.5f,

                            10.0f,

                            Colours::BlueFire,

                            500.0f,

                            true,

                            true);

                    break;

                case Terrain::Crystal:

                    game.lighting
                        .add_point_light(

                            x + 0.5f,

                            y + 0.5f,

                            8.0f,

                            Colours::Purple,

                            150.0f,

                            false,

                            false);

                    // ----------------------------------------------------
                    // Magical crystal spotlight.
                    //
                    // This deliberately demonstrates simlib spotlight
                    // support.
                    // ----------------------------------------------------

                    game.lighting
                        .add_spotlight(

                            x + 0.5f,

                            y + 0.5f,

                            12.0f,

                            game.game_time *
                                0.5f,

                            35.0f,

                            Colours::Magenta,

                            300.0f,

                            true);

                    break;

                default:

                    break;
                }
            }
        }
    }

    game.lighting.update(

        game.player.position.x,

        game.player.position.y,

        game.game_time);
}

// ============================================================================
// SIMLIB LIGHTING ADAPTER
//
// THIS IS ONE OF THE MAIN API ADAPTATION POINTS.
//
// Wire these calls to the exact current simlib lighting API.
//
// The game deliberately already gives you:
//
//     * point or spotlight
//     * position
//     * radius
//     * colour
//     * intensity
//     * direction
//     * cone angle
//     * whether this light won a shadow slot
//
// The first eight selected lights:
//
//     lighting.uses_shadow_slot(index)
//
// should be submitted to simlib as its shadow-casting lights.
// ============================================================================

static std::vector<sl::Light> build_simlib_lights(Game &game)
{
    std::vector<sl::Light> lights;
    for (int slot : game.lighting.shadow_slots)
    {
        if (slot < 0 || slot >= static_cast<int>(game.lighting.lights.size()))
            continue;
        const GameLight &light = game.lighting.lights[slot];
        if (!light.active)
            continue;

        sl::Light sl_light;
        sl_light.x = static_cast<float>(MAP_X) + (light.x - static_cast<float>(game.camera.x)) * CELL_WIDTH + CELL_WIDTH * 0.5f;
        sl_light.y = static_cast<float>(MAP_Y) + (light.y - static_cast<float>(game.camera.y)) * CELL_HEIGHT + CELL_HEIGHT * 0.5f;
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
    }
    return lights;
}

static std::vector<sl::ShadowCaster> build_simlib_shadow_casters(Game &game)
{
    std::vector<sl::ShadowCaster> casters;
    for (int screen_y = 0; screen_y < MAP_HEIGHT; ++screen_y)
    {
        for (int screen_x = 0; screen_x < MAP_WIDTH; ++screen_x)
        {
            const int world_x = game.camera.x + screen_x;
            const int world_y = game.camera.y + screen_y;

            Tile *tile = current_tile(game, world_x, world_y);
            if (tile && tile->visible && tile->blocks_sight)
            {
                float px = static_cast<float>(MAP_X + screen_x * CELL_WIDTH);
                float py = static_cast<float>(MAP_Y + screen_y * CELL_HEIGHT);
                casters.push_back(sl::make_rectangle_shadow_caster(px, py, px + CELL_WIDTH, py + CELL_HEIGHT));
            }
        }
    }
    return casters;
}

static void draw_cell(
    int cell_x,
    int cell_y,
    char character,
    const sl::Colour &foreground,
    const sl::Colour &background = Colours::Black)
{
    const int pixel_x = MAP_X + cell_x * CELL_WIDTH;
    const int pixel_y = MAP_Y + cell_y * CELL_HEIGHT;

    if (background.red != 0 || background.green != 0 || background.blue != 0)
    {
        sl::rectfill(sl::screen, static_cast<float>(pixel_x), static_cast<float>(pixel_y),
                     static_cast<float>(pixel_x + CELL_WIDTH - 1), static_cast<float>(pixel_y + CELL_HEIGHT - 1), background);
    }

    if (character != ' ' && character != '\0')
    {
        char text[2] = {character, '\0'};
        sl::textout(g_font, pixel_x + 1, pixel_y, foreground, text);
    }
}

static void draw_text(
    int x,
    int y,
    const std::string &text,
    const sl::Colour &colour)
{
    sl::textout(g_font, x, y, colour, text);
}

// ============================================================================
// RENDER TILE
// ============================================================================

static void render_tile(
    Game &game,

    int world_x,
    int world_y,

    int screen_x,
    int screen_y)
{
    Tile *tile =
        current_tile(
            game,
            world_x,
            world_y);

    if (!tile)
    {
        draw_cell(

            screen_x,
            screen_y,

            ' ',

            Colours::Black);

        return;
    }

    if (!tile->explored)
    {
        draw_cell(

            screen_x,
            screen_y,

            ' ',

            Colours::Black);

        return;
    }

    sl::Colour colour =
        terrain_colour(
            tile->terrain);

    if (!tile->visible)
    {
        colour =
            multiply_colour(
                colour,
                0.35f);
    }

    draw_cell(

        screen_x,
        screen_y,

        terrain_glyph(
            tile->terrain),

        colour);
}

// ============================================================================
// RENDER ACTORS
// ============================================================================

static void render_actors(
    Game &game)
{
    // ------------------------------------------------------------------------
    // Monsters.
    // ------------------------------------------------------------------------

    for (
        const Actor &actor :
        game.actors)
    {
        if (!actor.alive)
        {
            continue;
        }

        Tile *tile =
            current_tile(
                game,

                actor.position.x,

                actor.position.y);

        if (
            !tile ||
            !tile->visible)
        {
            continue;
        }

        const int screen_x =
            actor.position.x -
            game.camera.x;

        const int screen_y =
            actor.position.y -
            game.camera.y;

        if (
            screen_x < 0 ||
            screen_y < 0 ||

            screen_x >= MAP_WIDTH ||
            screen_y >= MAP_HEIGHT)
        {
            continue;
        }

        draw_cell(

            screen_x,
            screen_y,

            actor.glyph.character,

            actor.glyph.foreground);
    }

    // ------------------------------------------------------------------------
    // Player.
    // ------------------------------------------------------------------------

    {
        const int screen_x =
            game.player.position.x -
            game.camera.x;

        const int screen_y =
            game.player.position.y -
            game.camera.y;

        if (
            screen_x >= 0 &&
            screen_y >= 0 &&

            screen_x < MAP_WIDTH &&
            screen_y < MAP_HEIGHT)
        {
            draw_cell(

                screen_x,
                screen_y,

                game.player
                    .glyph
                    .character,

                game.player
                    .glyph
                    .foreground);
        }
    }
}

// ============================================================================
// RENDER MAP
// ============================================================================

static void render_map(
    Game &game)
{
    update_camera(
        game);

    for (
        int screen_y = 0;
        screen_y < MAP_HEIGHT;
        ++screen_y)
    {
        for (
            int screen_x = 0;
            screen_x < MAP_WIDTH;
            ++screen_x)
        {
            const int world_x =
                game.camera.x +
                screen_x;

            const int world_y =
                game.camera.y +
                screen_y;

            render_tile(

                game,

                world_x,
                world_y,

                screen_x,
                screen_y);
        }
    }

    render_actors(
        game);
}

// ============================================================================
// RENDER HEADER
// ============================================================================

static void render_header(
    Game &game)
{
    std::stringstream title;

    title
        << "REALMS BELOW";

    if (
        game.area_type ==
        AreaType::Overworld)
    {
        title
            << "   WILDERNESS";
    }

    else
    {
        title
            << "   DUNGEON "
            << game.current_depth + 1;
    }

    title
        << "   TURN "
        << game.turn;

    draw_text(

        10,
        10,

        title.str(),

        Colours::Gold);
}

// ============================================================================
// RENDER STATUS
// ============================================================================

static void render_status(
    Game &game)
{
    std::stringstream stream;

    stream
        << "HP "
        << game.player.health.current
        << "/"
        << game.player.health.maximum

        << "   ATK "
        << game.player.combat.attack

        << "   DEF "
        << game.player.combat.defence;

    draw_text(

        10,

        SCREEN_HEIGHT - 70,

        stream.str(),

        Colours::White);

    std::stringstream light_stream;

    int shadow_count = 0;

    for (
        int slot :
        game.lighting.shadow_slots)
    {
        if (slot >= 0)
        {
            ++shadow_count;
        }
    }

    light_stream
        << "LIGHTS "
        << game.lighting.lights.size()

        << "   SHADOWS "
        << shadow_count

        << "/8";

    draw_text(

        420,

        SCREEN_HEIGHT - 70,

        light_stream.str(),

        Colours::Yellow);
}

// ============================================================================
// RENDER MESSAGES
// ============================================================================

static void render_messages(
    Game &game)
{
    int y =
        SCREEN_HEIGHT - 45;

    for (
        const std::string &message :
        game.messages)
    {
        draw_text(

            10,

            y,

            message,

            Colours::Grey);

        y += 14;
    }
}

// ============================================================================
// RENDER GAME
// ============================================================================

static void render_game(
    Game &game)
{
    render_header(
        game);

    render_map(
        game);

    render_status(
        game);

    render_messages(
        game);

    if (
        game.state ==
        GameState::GameOver)
    {
        draw_text(

            SCREEN_WIDTH / 2 - 80,

            SCREEN_HEIGHT / 2,

            "YOU HAVE DIED",

            Colours::Red);
    }
}

// ============================================================================
// INPUT
//
// Replace the commented simlib calls with the exact current key API.
//
// The intended controls:
//
//     arrows / numpad / hjkl
//
//     > descend / enter dungeon
//     < ascend
//
//     . wait
//     Q quit
//
// ============================================================================

static bool handle_input(
    Game &game,
    const sl::Event &event)
{
    if (event.type() != sl::Event::Type::key_down)
    {
        return false;
    }

    const sl::Event::Key key = event.key();

    if (key == sl::Event::Key::arrow_left || key == sl::Event::Key::letter_h || key == sl::Event::Key::keypad_4)
        return try_player_move(game, -1, 0);

    if (key == sl::Event::Key::arrow_right || key == sl::Event::Key::letter_l || key == sl::Event::Key::keypad_6)
        return try_player_move(game, 1, 0);

    if (key == sl::Event::Key::arrow_up || key == sl::Event::Key::letter_k || key == sl::Event::Key::keypad_8)
        return try_player_move(game, 0, -1);

    if (key == sl::Event::Key::arrow_down || key == sl::Event::Key::letter_j || key == sl::Event::Key::keypad_2)
        return try_player_move(game, 0, 1);

    if (key == sl::Event::Key::letter_y || key == sl::Event::Key::keypad_7)
        return try_player_move(game, -1, -1);

    if (key == sl::Event::Key::letter_u || key == sl::Event::Key::keypad_9)
        return try_player_move(game, 1, -1);

    if (key == sl::Event::Key::letter_b || key == sl::Event::Key::keypad_1)
        return try_player_move(game, -1, 1);

    if (key == sl::Event::Key::letter_n || key == sl::Event::Key::keypad_3)
        return try_player_move(game, 1, 1);

    if (key == sl::Event::Key::period || key == sl::Event::Key::keypad_5)
        return true;

    if (key == sl::Event::Key::greater || key == sl::Event::Key::less ||
        key == sl::Event::Key::return_key || key == sl::Event::Key::keypad_enter)
    {
        use_stairs(game);
        return false;
    }

    return false;
}

// ============================================================================
// INITIALISE GAME
// ============================================================================

static void initialise_game(
    Game &game)
{
    generate_overworld(
        game);

    update_fov(
        game);

    build_lights(
        game);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, SCREEN_WIDTH, SCREEN_HEIGHT))
    {
        return -1;
    }

    sl::set_window_title("simlib - ASCII Roguelike (exroguelike)");
    sl::set_fps(60);

    g_font = sl::open_monospace_font(12);
    if (!g_font)
    {
        g_font = sl::get_default_monospace_font();
    }

    sl::Bitmap *scene = sl::create_render_target(SCREEN_WIDTH, SCREEN_HEIGHT);
    sl::LightingPass lighting_pass;
    lighting_pass.initialise();
    lighting_pass.set_ambient(0.25f);

    Game game;

    initialise_game(
        game);

    bool running = true;
    bool dirty = true;

    while (running)
    {
        sl::Event event;
        bool player_acted = false;

        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
            {
                running = false;
            }

            if (handle_input(game, event))
            {
                player_acted = true;
            }

            sl::display_handle_event(event);
        }

        if (player_acted)
        {
            game.game_time += 0.1f;
            advance_turn(game);
            update_fov(game);
            build_lights(game);
            dirty = true;
        }

        if (dirty)
        {
            if (scene)
            {
                sl::begin_render_target(scene);
                sl::clear_render_target(sl::Colour{10, 10, 15, 255});

                render_game(game);

                sl::end_render_target();

                auto lights = build_simlib_lights(game);
                auto casters = build_simlib_shadow_casters(game);
                lighting_pass.apply(scene, lights, MAP_X, MAP_Y, MAP_WIDTH * CELL_WIDTH, MAP_HEIGHT * CELL_HEIGHT, casters);

                sl::clear_to_colour(sl::screen, sl::Colour{0, 0, 0, 255});
                sl::draw_sprite(scene, 0.0f, 0.0f);
            }
            else
            {
                sl::clear_to_colour(sl::screen, sl::Colour{10, 10, 15, 255});
                render_game(game);
            }

            sl::show_video_bitmap();
            sl::end_frame();
            dirty = false;
        }
        else
        {
            sl::rest(10);
        }
    }

    if (scene)
    {
        sl::destroy_bitmap(scene);
    }

    lighting_pass.shutdown();
    sl::shutdown();

    return 0;
}
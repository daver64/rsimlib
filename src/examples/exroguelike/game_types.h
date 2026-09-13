#pragma once

#include "sl.h"
#include "recs.h"
#include "rworld.h"

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

constexpr int CELL_WIDTH = 24;
constexpr int CELL_HEIGHT = 32;

extern sl::Font *g_map_font;
extern sl::Font *g_ui_font;

constexpr int MAP_X = 10;
constexpr int MAP_Y = 42;

constexpr int MAP_WIDTH = 40;
constexpr int MAP_HEIGHT = 21;

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
// RANDOM HELPERS
// ============================================================================

int random_int(int minimum, int maximum);
float random_float(float minimum, float maximum);
bool random_chance(float chance);

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

sl::Colour multiply_colour(const sl::Colour &colour, float amount);

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
// BASIC POSITION & GRID STRUCTURES
// ============================================================================

struct Position
{
    int x = 0;
    int y = 0;
};

bool operator==(const Position &a, const Position &b);

struct Tile
{
    Terrain terrain = Terrain::Unknown;
    bool explored = false;
    bool visible = false;
    bool blocks_movement = false;
    bool blocks_sight = false;
};

struct Room
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    RoomType type = RoomType::Normal;

    int centre_x() const { return x + width / 2; }
    int centre_y() const { return y + height / 2; }
};

struct Overworld
{
    std::array<Tile, WORLD_WIDTH * WORLD_HEIGHT> tiles;

    Tile &at(int x, int y) { return tiles[y * WORLD_WIDTH + x]; }
    const Tile &at(int x, int y) const { return tiles[y * WORLD_WIDTH + x]; }
    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < WORLD_WIDTH && y < WORLD_HEIGHT; }
};

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

    Tile &at(int x, int y) { return tiles[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)]; }
    const Tile &at(int x, int y) const { return tiles[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)]; }
    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }
};

struct Dungeon
{
    int id = 0;
    Position overworld_position;
    std::vector<std::unique_ptr<DungeonLevel>> levels;
};

struct Camera
{
    int x = 0;
    int y = 0;
};

struct GameLight
{
    LightType type = LightType::Point;
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

struct LightingSystem
{
    std::vector<GameLight> lights;
    std::array<int, MAX_SHADOW_LIGHTS> shadow_slots;

    LightingSystem();
    void clear();
    int add_point_light(float x, float y, float radius, const sl::Colour &colour, float priority, bool shadows, bool flicker);
    int add_spotlight(float x, float y, float radius, float direction, float cone_angle, const sl::Colour &colour, float priority, bool shadows);
    void update(float player_x, float player_y, float time);
    bool uses_shadow_slot(int light_index) const;
};

// ============================================================================
// ECS COMPONENT & ACTOR STRUCTURES
// ============================================================================

struct Glyph
{
    char character = '?';
    sl::Colour foreground = Colours::White;
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
    MonsterType type = MonsterType::Rat;
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
    MonsterType monster_type = MonsterType::Rat;
};

struct Game
{
    GameState state = GameState::Playing;
    AreaType area_type = AreaType::Overworld;
    Overworld overworld;
    std::shared_ptr<rworld::World> rworld_generator;
    std::vector<std::unique_ptr<Dungeon>> dungeons;
    DungeonLevel *current_level = nullptr;
    int current_dungeon = -1;
    int current_depth = 0;

    Position player_position;
    Actor player;
    std::vector<Actor> actors;
    Camera camera;
    LightingSystem lighting;
    std::deque<std::string> messages;

    uint64_t turn = 0;
    float game_time = 0.0f;
    bool night = false;
    int next_actor_id = 1;

    Game();
    void message(const std::string &text);
};

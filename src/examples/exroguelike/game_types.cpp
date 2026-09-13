#include "game_types.h"

sl::Font *g_map_font = nullptr;
sl::Font *g_ui_font = nullptr;

static std::mt19937 rng(static_cast<uint32_t>(std::time(nullptr)));

int random_int(int minimum, int maximum)
{
    std::uniform_int_distribution<int> distribution(minimum, maximum);
    return distribution(rng);
}

float random_float(float minimum, float maximum)
{
    std::uniform_real_distribution<float> distribution(minimum, maximum);
    return distribution(rng);
}

bool random_chance(float chance)
{
    return random_float(0.0f, 1.0f) < chance;
}

sl::Colour multiply_colour(const sl::Colour &colour, float amount)
{
    return sl::Colour{
        static_cast<Uint8>(std::clamp(colour.red * amount, 0.0f, 255.0f)),
        static_cast<Uint8>(std::clamp(colour.green * amount, 0.0f, 255.0f)),
        static_cast<Uint8>(std::clamp(colour.blue * amount, 0.0f, 255.0f)),
        colour.alpha};
}

bool operator==(const Position &a, const Position &b)
{
    return a.x == b.x && a.y == b.y;
}

LightingSystem::LightingSystem()
{
    shadow_slots.fill(-1);
}

void LightingSystem::clear()
{
    lights.clear();
    shadow_slots.fill(-1);
}

int LightingSystem::add_point_light(float x, float y, float radius, const sl::Colour &colour, float priority, bool shadows, bool flicker)
{
    GameLight light;
    light.type = LightType::Point;
    light.x = x;
    light.y = y;
    light.radius = radius;
    light.colour = colour;
    light.priority = priority;
    light.cast_shadows = shadows;
    light.flicker = flicker;

    lights.push_back(light);
    return static_cast<int>(lights.size() - 1);
}

int LightingSystem::add_spotlight(float x, float y, float radius, float direction, float cone_angle, const sl::Colour &colour, float priority, bool shadows)
{
    GameLight light;
    light.type = LightType::Spotlight;
    light.x = x;
    light.y = y;
    light.radius = radius;
    light.direction = direction;
    light.cone_angle = cone_angle;
    light.colour = colour;
    light.priority = priority;
    light.cast_shadows = shadows;

    lights.push_back(light);
    return static_cast<int>(lights.size() - 1);
}

void LightingSystem::update(float player_x, float player_y, float time)
{
    std::vector<std::pair<float, int>> candidates;

    for (int i = 0; i < static_cast<int>(lights.size()); ++i)
    {
        GameLight &light = lights[i];
        if (!light.active)
            continue;

        if (light.flicker)
        {
            const float flicker = std::sin(time * 8.0f + i * 1.37f) * 0.08f;
            light.intensity = 1.0f + flicker;
        }

        if (!light.cast_shadows)
            continue;

        const float dx = light.x - player_x;
        const float dy = light.y - player_y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        const float score = light.priority - distance * 0.05f;

        candidates.emplace_back(score, i);
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto &a, const auto &b)
              { return a.first > b.first; });

    shadow_slots.fill(-1);

    for (int i = 0; i < MAX_SHADOW_LIGHTS && i < static_cast<int>(candidates.size()); ++i)
    {
        shadow_slots[i] = candidates[i].second;
    }
}

bool LightingSystem::uses_shadow_slot(int light_index) const
{
    for (int slot : shadow_slots)
    {
        if (slot == light_index)
            return true;
    }
    return false;
}

Game::Game()
{
    player.id = next_actor_id++;
    player.player = true;
    player.position = {256, 256};
    player.glyph.character = '@';
    player.glyph.foreground = Colours::Player;
    player.health = {30, 30};
    player.combat = {7, 2};
    player_position = player.position;
}

void Game::message(const std::string &text)
{
    messages.push_front(text);
    while (messages.size() > MAX_MESSAGES)
    {
        messages.pop_back();
    }
}

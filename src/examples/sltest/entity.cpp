#include "entity.h"

#include "display.h"
#include "physics.h"

#include <algorithm>
#include <cmath>

namespace game {

namespace {

constexpr float ambient_temperature = 293.0f;
constexpr float minimum_balloon_volume = 0.4f;
constexpr float maximum_balloon_volume = 3.0f;
constexpr float sea_level_air_density = 1.225f;
constexpr float minimum_air_density = 0.25f;
constexpr float balloon_gas_relative_density = 0.15f;
constexpr float thermal_lift_multiplier = 8.0f;
constexpr float temperature_cooling_rate = 0.035f;
constexpr float burner_heating_rate = 8.5f;
constexpr float wind_response_rate = 0.7f;

sl::PhysicsWorld *physics_world = nullptr;
std::vector<sl::PhysicsBody *> physics_bodies;

float wind_speed(float y)
{
    constexpr float layer_speeds[] = {18.0f, -14.0f, 22.0f, -17.0f};
    const float height = std::max(1.0f, static_cast<float>(sl::screen_height()));
    const int layer = std::min(3, std::max(0, static_cast<int>(4.0f * y / height)));
    return layer_speeds[layer];
}

void apply_wind(GameObject& object, float dt_seconds)
{
    if (!object.is_balloon)
    {
        return;
    }
    object.vx += (wind_speed(object.y) - object.vx) * wind_response_rate * dt_seconds;
}

float ambient_air_density(float y)
{
    const float height = std::max(1.0f, static_cast<float>(sl::screen_height()));
    const float altitude_fraction = std::clamp((height - y) / height, 0.0f, 1.0f);
    return std::max(minimum_air_density, sea_level_air_density * (1.0f - 0.65f * altitude_fraction));
}

void apply_buoyancy(GameObject& object, float dt_seconds, float gravity)
{
    if (!object.is_balloon)
    {
        return;
    }

    const float heating = object.burner_active ? burner_heating_rate : 0.0f;
    object.gas_temperature += (heating - (object.gas_temperature - ambient_temperature) * temperature_cooling_rate) * dt_seconds;
    object.gas_temperature = std::max(ambient_temperature, object.gas_temperature);

    const float air_density = ambient_air_density(object.y);
    const float thermal_expansion = 1.0f - ambient_temperature / object.gas_temperature;
    const float effective_gas_density = balloon_gas_relative_density * std::max(
        0.0f, 1.0f - thermal_lift_multiplier * thermal_expansion);
    const float gas_density = air_density * effective_gas_density;
    const float displaced_air_mass = air_density * object.gas_bag_volume;
    const float gas_mass = gas_density * object.gas_bag_volume;
    const float buoyant_acceleration = gravity * (displaced_air_mass - gas_mass) / std::max(object.mass, 0.01f);
    object.vy -= buoyant_acceleration * dt_seconds;
}

} // namespace

void shutdown_physics()
{
    for (sl::PhysicsBody *body : physics_bodies)
    {
        sl::destroy_physics_body(body);
    }
    physics_bodies.clear();
    sl::destroy_physics_world(physics_world);
    physics_world = nullptr;
}

void reset_physics(std::vector<GameObject>& objects)
{
    shutdown_physics();
    physics_world = sl::create_physics_world({0.0f, 0.0f});
    if (!physics_world)
    {
        return;
    }
    for (const GameObject &object : objects)
    {
        sl::PhysicsBody *body = sl::create_physics_body(
            physics_world,
            object.is_static ? sl::BodyType::static_body : sl::BodyType::dynamic_body,
            {object.x, object.y});
        if (!body)
        {
            shutdown_physics();
            return;
        }
        const bool fixture_created = object.shape == ColliderShape::circle
            ? sl::add_circle_fixture(body, object.radius, object.mass, 0.5f, object.restitution)
            : sl::add_box_fixture(body, object.width, object.height, object.mass, 0.5f, object.restitution);
        if (!fixture_created)
        {
            shutdown_physics();
            return;
        }
        physics_bodies.push_back(body);
    }
}

/** @brief Create a circular physics object centred at @p x, @p y using @p bitmap as its sprite. */
GameObject make_circle_object(sl::Bitmap* bitmap, float x, float y, float radius, float mass) {
    GameObject object;
    object.bitmap = bitmap;
    object.x = x;
    object.y = y;
    object.shape = ColliderShape::circle;
    object.radius = radius;
    object.width = radius * 2.0f;
    object.height = radius * 2.0f;
    object.mass = mass;
    return object;
}

/** @brief Create an axis-aligned box physics object centred at @p x, @p y. */
GameObject make_aabb_object(sl::Bitmap* bitmap, float x, float y, float width, float height, float mass) {
    GameObject object;
    object.bitmap = bitmap;
    object.x = x;
    object.y = y;
    object.shape = ColliderShape::aabb;
    object.width = width;
    object.height = height;
    object.mass = mass;
    return object;
}

namespace {

/** @brief Return zero for immovable objects or the reciprocal mass used by impulse resolution. */
float inverse_mass(const GameObject& object) {
    if (object.is_static || object.mass <= 0.0f) {
        return 0.0f;
    }
    return 1.0f / object.mass;
}

/** Closest point on an AABB (centred at ax, ay) to a given point. */
void closest_point_on_aabb(const GameObject& box, float pointX, float pointY, float& outX, float& outY) {
    const float halfWidth = box.width * 0.5f;
    const float halfHeight = box.height * 0.5f;
    outX = std::clamp(pointX, box.x - halfWidth, box.x + halfWidth);
    outY = std::clamp(pointY, box.y - halfHeight, box.y + halfHeight);
}

/** Test two objects for overlap, returning the separation normal (pointing from a to b) and penetration depth. */
bool compute_overlap(const GameObject& a, const GameObject& b, float& normalX, float& normalY, float& penetration) {
    if (a.shape == ColliderShape::circle && b.shape == ColliderShape::circle) {
        const float deltaX = b.x - a.x;
        const float deltaY = b.y - a.y;
        const float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
        const float radiusSum = a.radius + b.radius;
        if (distance >= radiusSum || distance <= 0.0001f) {
            if (distance <= 0.0001f && radiusSum > 0.0f) {
                normalX = 1.0f;
                normalY = 0.0f;
                penetration = radiusSum;
                return true;
            }
            return false;
        }
        normalX = deltaX / distance;
        normalY = deltaY / distance;
        penetration = radiusSum - distance;
        return true;
    }

    if (a.shape == ColliderShape::aabb && b.shape == ColliderShape::aabb) {
        const float halfWidthSum = (a.width + b.width) * 0.5f;
        const float halfHeightSum = (a.height + b.height) * 0.5f;
        const float deltaX = b.x - a.x;
        const float deltaY = b.y - a.y;
        const float overlapX = halfWidthSum - std::abs(deltaX);
        const float overlapY = halfHeightSum - std::abs(deltaY);
        if (overlapX <= 0.0f || overlapY <= 0.0f) {
            return false;
        }
        if (overlapX < overlapY) {
            normalX = deltaX < 0.0f ? -1.0f : 1.0f;
            normalY = 0.0f;
            penetration = overlapX;
        } else {
            normalX = 0.0f;
            normalY = deltaY < 0.0f ? -1.0f : 1.0f;
            penetration = overlapY;
        }
        return true;
    }

    // one circle, one AABB: normalize so `circle` is the circle and remember to flip the normal if swapped
    const GameObject& circle = a.shape == ColliderShape::circle ? a : b;
    const GameObject& box = a.shape == ColliderShape::circle ? b : a;
    const bool swapped = a.shape != ColliderShape::circle;

    float closestX = 0.0f;
    float closestY = 0.0f;
    closest_point_on_aabb(box, circle.x, circle.y, closestX, closestY);
    const float deltaX = circle.x - closestX;
    const float deltaY = circle.y - closestY;
    const float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
    if (distance >= circle.radius) {
        return false;
    }
    float nx = distance > 0.0001f ? deltaX / distance : 0.0f;
    float ny = distance > 0.0001f ? deltaY / distance : -1.0f;
    penetration = circle.radius - distance;
    // normal must point from a to b
    if (!swapped) {
        normalX = -nx;
        normalY = -ny;
    } else {
        normalX = nx;
        normalY = ny;
    }
    return true;
}

} // namespace

/**
 * @brief Integrate gravity, drag, and velocity into dynamic object positions.
 * @param objects Mutable simulation objects.
 * @param dt_seconds Elapsed frame time, normally clamped by the caller.
 * @param gravity Downward acceleration in screen pixels per second squared.
 */
void physics_step(std::vector<GameObject>& objects, float dt_seconds, float gravity) {
    if (!physics_world || physics_bodies.size() != objects.size())
    {
        reset_physics(objects);
    }
    if (!physics_world || physics_bodies.size() != objects.size())
    {
        return;
    }
    for (std::size_t index = 0; index < objects.size(); ++index)
    {
        GameObject &object = objects[index];
        if (object.is_static)
        {
            continue;
        }
        apply_buoyancy(object, dt_seconds, gravity);
        apply_wind(object, dt_seconds);
        object.vy += gravity * object.gravity_scale * dt_seconds;
        if (object.drag > 0.0f)
        {
            const float damping = std::clamp(1.0f - object.drag * dt_seconds, 0.0f, 1.0f);
            object.vx *= damping;
            object.vy *= damping;
        }
        sl::set_physics_body_velocity(physics_bodies[index], {object.vx, object.vy});
    }
    sl::step_physics_world(physics_world, dt_seconds);
    for (std::size_t index = 0; index < objects.size(); ++index)
    {
        if (objects[index].is_static)
        {
            continue;
        }
        const sl::Vec2 position = sl::physics_body_position(physics_bodies[index]);
        const sl::Vec2 velocity = sl::physics_body_velocity(physics_bodies[index]);
        objects[index].x = position.x;
        objects[index].y = position.y;
        objects[index].vx = velocity.x;
        objects[index].vy = velocity.y;
    }
}

void adjust_balloon_volume(GameObject& object, float volume_delta)
{
    if (!object.is_balloon)
    {
        return;
    }
    object.gas_bag_volume = std::clamp(
        object.gas_bag_volume + volume_delta,
        minimum_balloon_volume,
        maximum_balloon_volume);
    object.radius = 16.0f * std::sqrt(object.gas_bag_volume);
    object.width = object.radius * 2.0f;
    object.height = object.radius * 2.0f;
}

/** @brief Keep dynamic objects inside the current simlib display and bounce them from its edges. */
void constrain_to_screen(std::vector<GameObject>& objects) {
    const float screenWidth = static_cast<float>(sl::screen_width());
    const float screenHeight = static_cast<float>(sl::screen_height());
    for (std::size_t index = 0; index < objects.size(); ++index) {
        GameObject& object = objects[index];
        if (object.is_static) {
            continue;
        }
        const float halfWidth = object.shape == ColliderShape::circle ? object.radius : object.width * 0.5f;
        const float halfHeight = object.shape == ColliderShape::circle ? object.radius : object.height * 0.5f;

        if (object.is_balloon) {
            if (object.x + halfWidth < 0.0f) {
                object.x = screenWidth + halfWidth;
            } else if (object.x - halfWidth > screenWidth) {
                object.x = -halfWidth;
            }
        } else if (object.x - halfWidth < 0.0f) {
            object.x = halfWidth;
            object.vx = -object.vx * object.restitution;
        } else if (object.x + halfWidth > screenWidth) {
            object.x = screenWidth - halfWidth;
            object.vx = -object.vx * object.restitution;
        }

        if (object.y - halfHeight < 0.0f) {
            object.y = halfHeight;
            object.vy = -object.vy * object.restitution;
        } else if (object.y + halfHeight > screenHeight) {
            object.y = screenHeight - halfHeight;
            object.vy = -object.vy * object.restitution;
        }

        if (physics_world && physics_bodies.size() == objects.size())
        {
            sl::set_physics_body_transform(physics_bodies[index], {object.x, object.y});
            sl::set_physics_body_velocity(physics_bodies[index], {object.vx, object.vy});
        }
    }
}

/** @brief Draw each object bitmap around its physics centre via simlib's scaled-sprite renderer. */
void render_objects(const std::vector<GameObject>& objects) {
    for (const GameObject& object : objects) {
        if (!object.bitmap) {
            continue;
        }
        const float halfWidth = object.shape == ColliderShape::circle ? object.radius : object.width * 0.5f;
        const float halfHeight = object.shape == ColliderShape::circle ? object.radius : object.height * 0.5f;
        sl::draw_sprite_stretched(
            object.bitmap,
            object.x - halfWidth,
            object.y - halfHeight,
            static_cast<int>(halfWidth * 2.0f),
            static_cast<int>(halfHeight * 2.0f)
        );
    }
}

} // namespace game

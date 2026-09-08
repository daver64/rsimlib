#include "entity.h"

#include "display.h"

#include <algorithm>
#include <cmath>

namespace game {

/** @brief Create a circular physics object centred at @p x, @p y using @p bitmap as its sprite. */
GameObject make_circle_object(simlib::Bitmap* bitmap, float x, float y, float radius, float mass) {
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
GameObject make_aabb_object(simlib::Bitmap* bitmap, float x, float y, float width, float height, float mass) {
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
    for (GameObject& object : objects) {
        if (object.is_static) {
            continue;
        }
        object.vy += gravity * object.gravity_scale * dt_seconds;
        if (object.drag > 0.0f) {
            const float damping = std::clamp(1.0f - object.drag * dt_seconds, 0.0f, 1.0f);
            object.vx *= damping;
            object.vy *= damping;
        }
        object.x += object.vx * dt_seconds;
        object.y += object.vy * dt_seconds;
    }
}

/**
 * @brief Resolve pairwise overlap using mass-weighted position correction and restitution impulses.
 *
 * Supports circle, AABB, and mixed circle/AABB pairs. Objects marked static participate in
 * collision detection but do not move or receive velocity changes.
 */
void resolve_collisions(std::vector<GameObject>& objects) {
    for (std::size_t i = 0; i < objects.size(); ++i) {
        for (std::size_t j = i + 1; j < objects.size(); ++j) {
            GameObject& a = objects[i];
            GameObject& b = objects[j];
            const float invA = inverse_mass(a);
            const float invB = inverse_mass(b);
            if (invA == 0.0f && invB == 0.0f) {
                continue;
            }

            float normalX = 0.0f;
            float normalY = 0.0f;
            float penetration = 0.0f;
            if (!compute_overlap(a, b, normalX, normalY, penetration)) {
                continue;
            }

            const float totalInverseMass = invA + invB;

            // positional correction, split by relative mass
            a.x -= normalX * penetration * (invA / totalInverseMass);
            a.y -= normalY * penetration * (invA / totalInverseMass);
            b.x += normalX * penetration * (invB / totalInverseMass);
            b.y += normalY * penetration * (invB / totalInverseMass);

            // velocity resolution along the collision normal
            const float relativeVX = b.vx - a.vx;
            const float relativeVY = b.vy - a.vy;
            const float velocityAlongNormal = relativeVX * normalX + relativeVY * normalY;
            if (velocityAlongNormal > 0.0f) {
                continue;
            }
            const float restitution = std::min(a.restitution, b.restitution);
            const float impulseMagnitude = -(1.0f + restitution) * velocityAlongNormal / totalInverseMass;
            const float impulseX = impulseMagnitude * normalX;
            const float impulseY = impulseMagnitude * normalY;
            a.vx -= impulseX * invA;
            a.vy -= impulseY * invA;
            b.vx += impulseX * invB;
            b.vy += impulseY * invB;
        }
    }
}

/** @brief Keep dynamic objects inside the current simlib display and bounce them from its edges. */
void constrain_to_screen(std::vector<GameObject>& objects) {
    const float screenWidth = static_cast<float>(simlib::screen_width());
    const float screenHeight = static_cast<float>(simlib::screen_height());
    for (GameObject& object : objects) {
        if (object.is_static) {
            continue;
        }
        const float halfWidth = object.shape == ColliderShape::circle ? object.radius : object.width * 0.5f;
        const float halfHeight = object.shape == ColliderShape::circle ? object.radius : object.height * 0.5f;

        if (object.x - halfWidth < 0.0f) {
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
        simlib::draw_sprite_stretched(
            object.bitmap,
            object.x - halfWidth,
            object.y - halfHeight,
            static_cast<int>(halfWidth * 2.0f),
            static_cast<int>(halfHeight * 2.0f)
        );
    }
}

} // namespace game

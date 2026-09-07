#pragma once

#include "draw.h"

#include <vector>

namespace game {

enum class ColliderShape { circle, aabb };

/** A simple game object bundling transform, physics, collider, and sprite data. */
struct GameObject {
    // transform (x, y is the center of the object)
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;

    // physics
    float mass = 1.0f;
    float gravity_scale = 1.0f;
    float drag = 0.0f;
    float restitution = 0.5f;
    bool is_static = false;

    // collider
    ColliderShape shape = ColliderShape::circle;
    float radius = 16.0f;
    float width = 32.0f;
    float height = 32.0f;

    // sprite (drawn stretched to the collider's bounding box)
    simlib::Bitmap* bitmap = nullptr;
};

/** Create a circle-collider object centred at (x, y). */
GameObject make_circle_object(simlib::Bitmap* bitmap, float x, float y, float radius, float mass = 1.0f);
/** Create an AABB-collider object centred at (x, y). */
GameObject make_aabb_object(simlib::Bitmap* bitmap, float x, float y, float width, float height, float mass = 1.0f);

/** Apply gravity/drag and integrate position for all objects. */
void physics_step(std::vector<GameObject>& objects, float dt_seconds, float gravity = 980.0f);
/** Detect and resolve overlaps between all object pairs (circle and/or AABB). */
void resolve_collisions(std::vector<GameObject>& objects);
/** Keep objects inside the screen bounds, bouncing off the edges. */
void constrain_to_screen(std::vector<GameObject>& objects);
/** Draw every object's sprite at its current position. */
void render_objects(const std::vector<GameObject>& objects);

} // namespace game

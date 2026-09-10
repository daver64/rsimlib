#include "physics.h"

#include <cmath>
#include <iostream>

int main()
{
    sl::PhysicsWorld *world = sl::create_physics_world({0.0f, 100.0f});
    sl::PhysicsBody *body = sl::create_physics_body(world, sl::BodyType::dynamic_body, {20.0f, 20.0f});
    const bool fixture_created = sl::add_circle_fixture(body, 8.0f, 1.0f);
    for (int step = 0; step < 10; ++step)
    {
        sl::step_physics_world(world, 0.1f);
    }
    const sl::Vec2 position = sl::physics_body_position(body);
    const bool passed = world && body && fixture_created && position.y > 20.0f &&
        std::isfinite(position.x) && std::isfinite(position.y);

    sl::destroy_physics_body(body);
    sl::destroy_physics_world(world);
    if (!passed)
    {
        std::cerr << "Physics wrapper produced an unexpected body state.\n";
        return 1;
    }
    return 0;
}

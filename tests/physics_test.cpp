#include "physics.h"

#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    sl::PhysicsWorld *world = sl::create_physics_world({0.0f, 100.0f});
    sl::PhysicsBody *obstacle = sl::create_physics_body(world, sl::BodyType::static_body, {20.0f, 100.0f});
    sl::PhysicsBody *body = sl::create_physics_body(world, sl::BodyType::dynamic_body, {20.0f, 100.0f});
    const bool fixture_created = sl::add_circle_fixture(body, 8.0f, 1.0f);
    const bool polygon_created = sl::add_polygon_fixture(obstacle, {{-12.0f, -4.0f}, {12.0f, -4.0f}, {0.0f, 10.0f}});
    for (int step = 0; step < 10; ++step)
    {
        sl::step_physics_world(world, 0.1f);
    }
    const sl::Vec2 position = sl::physics_body_position(body);
    const std::vector<sl::PhysicsContact> contacts = sl::poll_physics_contacts(world);
    const bool contact_recorded = !contacts.empty() && contacts.front().type == sl::ContactType::begin &&
        contacts.front().body_a != nullptr && contacts.front().body_b != nullptr;
    const bool passed = world && body && obstacle && fixture_created && polygon_created && contact_recorded &&
        position.y > 20.0f && std::isfinite(position.x) && std::isfinite(position.y);

    sl::destroy_physics_body(body);
    sl::destroy_physics_body(obstacle);
    sl::destroy_physics_world(world);
    if (!passed)
    {
        std::cerr << "Physics wrapper produced an unexpected body state.\n";
        return 1;
    }
    return 0;
}

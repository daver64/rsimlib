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

    // Ray casts, queries, sensors and filters in a gravity-free world.
    bool queries_passed = false;
    {
        sl::PhysicsWorld *space = sl::create_physics_world({0.0f, 0.0f});
        sl::PhysicsBody *near_wall = sl::create_physics_body(space, sl::BodyType::static_body, {200.0f, 0.0f});
        sl::PhysicsBody *far_wall = sl::create_physics_body(space, sl::BodyType::static_body, {400.0f, 0.0f});
        sl::add_box_fixture(near_wall, 20.0f, 100.0f);
        sl::add_box_fixture(far_wall, 20.0f, 100.0f);
        sl::PhysicsBody *zone = sl::create_physics_body(space, sl::BodyType::static_body, {100.0f, 0.0f});
        sl::set_physics_body_sensor(zone, true);
        sl::add_box_fixture(zone, 20.0f, 100.0f);
        sl::step_physics_world(space, 1.0f / 60.0f);

        const sl::PhysicsRayHit hit = sl::physics_raycast(space, {0.0f, 0.0f}, {600.0f, 0.0f});
        const bool closest = hit.hit && hit.body == near_wall && std::abs(hit.point.x - 190.0f) < 1.0f &&
                             hit.normal.x < -0.9f && std::abs(hit.distance - 190.0f) < 1.0f;
        const std::vector<sl::PhysicsRayHit> all = sl::physics_raycast_all(space, {0.0f, 0.0f}, {600.0f, 0.0f});
        const bool ordered = all.size() == 2 && all[0].body == near_wall && all[1].body == far_wall;
        const std::vector<sl::PhysicsRayHit> with_sensor = sl::physics_raycast_all(space, {0.0f, 0.0f}, {600.0f, 0.0f}, true);
        const bool sensors_optional = with_sensor.size() == 3 && with_sensor[0].body == zone;
        const bool ignored = sl::physics_raycast(space, {0.0f, 0.0f}, {600.0f, 0.0f}, false, near_wall).body == far_wall;
        const bool miss = !sl::physics_raycast(space, {0.0f, 200.0f}, {600.0f, 200.0f}).hit &&
                          !sl::physics_raycast(space, {5.0f, 5.0f}, {5.0f, 5.0f}).hit;
        const bool box_query = sl::physics_query_box(space, {150.0f, -10.0f}, {250.0f, 10.0f}).size() == 1 &&
                               sl::physics_query_box(space, {90.0f, -10.0f}, {110.0f, 10.0f}).empty() &&
                               sl::physics_query_box(space, {90.0f, -10.0f}, {110.0f, 10.0f}, true).size() == 1;
        const bool point_query = sl::physics_query_point(space, {400.0f, 0.0f}).size() == 1 &&
                                 sl::physics_query_point(space, {300.0f, 0.0f}).empty();

        // A dynamic body passing through the sensor reports a sensor contact but is not stopped.
        sl::PhysicsBody *mover = sl::create_physics_body(space, sl::BodyType::dynamic_body, {60.0f, 0.0f});
        sl::add_circle_fixture(mover, 8.0f);
        sl::set_physics_body_velocity(mover, {300.0f, 0.0f});
        for (int step = 0; step < 30; ++step)
            sl::step_physics_world(space, 1.0f / 60.0f);
        bool sensor_begin = false;
        for (const sl::PhysicsContact &contact : sl::poll_physics_contacts(space))
            sensor_begin = sensor_begin || (contact.sensor && contact.type == sl::ContactType::begin);
        const bool passed_through = sl::physics_body_position(mover).x > 130.0f;

        // Filtering: bodies with disjoint masks pass through each other.
        sl::PhysicsBody *ghost = sl::create_physics_body(space, sl::BodyType::dynamic_body, {330.0f, 0.0f});
        sl::set_physics_body_filter(ghost, 2, 4);
        sl::add_circle_fixture(ghost, 8.0f);
        sl::set_physics_body_velocity(ghost, {300.0f, 0.0f});
        for (int step = 0; step < 30; ++step)
            sl::step_physics_world(space, 1.0f / 60.0f);
        const bool filtered = sl::physics_body_position(ghost).x > 440.0f;

        queries_passed = closest && ordered && sensors_optional && ignored && miss && box_query && point_query &&
                         sensor_begin && passed_through && filtered;
        if (!queries_passed)
            std::cerr << "ray/query/sensor results: " << closest << ordered << sensors_optional << ignored << miss
                      << box_query << point_query << sensor_begin << passed_through << filtered << "\n";
        sl::destroy_physics_world(space);
    }
    if (!queries_passed)
        return 1;
    bool joints_passed = false;
    {
        sl::PhysicsWorld *space = sl::create_physics_world({0.0f, 0.0f});
        sl::PhysicsBody *anchor = sl::create_physics_body(space, sl::BodyType::static_body, {100.0f, 100.0f});
        sl::add_circle_fixture(anchor, 5.0f);
        sl::PhysicsBody *bob = sl::create_physics_body(space, sl::BodyType::dynamic_body, {200.0f, 100.0f});
        sl::add_circle_fixture(bob, 5.0f);
        sl::set_physics_body_velocity(bob, {0.0f, 600.0f});
        sl::PhysicsJoint *rod = sl::create_distance_joint(space, anchor, bob, {100.0f, 100.0f}, {200.0f, 100.0f});
        for (int step = 0; step < 60; ++step)
            sl::step_physics_world(space, 1.0f / 60.0f);
        const sl::Vec2 p = sl::physics_body_position(bob);
        const float length = std::hypot(p.x - 100.0f, p.y - 100.0f);
        const bool rigid = rod && std::fabs(length - 100.0f) < 3.0f;

        // Motorised revolute joint spins its body.
        sl::PhysicsBody *hub = sl::create_physics_body(space, sl::BodyType::static_body, {400.0f, 400.0f});
        sl::add_circle_fixture(hub, 5.0f);
        sl::PhysicsBody *wheel = sl::create_physics_body(space, sl::BodyType::dynamic_body, {400.0f, 400.0f});
        sl::add_circle_fixture(wheel, 20.0f);
        sl::PhysicsJoint *axle = sl::create_revolute_joint(space, hub, wheel, {400.0f, 400.0f});
        sl::set_joint_motor(axle, 3.0f, 100000.0f);
        for (int step = 0; step < 60; ++step)
            sl::step_physics_world(space, 1.0f / 60.0f);
        const bool spins = std::fabs(sl::physics_body_angle(wheel)) > 1.0f;

        const bool bad_args = !sl::create_revolute_joint(space, hub, hub, {0.0f, 0.0f}) &&
                              !sl::create_prismatic_joint(space, hub, wheel, {0.0f, 0.0f}, {0.0f, 0.0f}) &&
                              !sl::create_mouse_joint(space, hub, {0.0f, 0.0f});

        // Destroying a body deactivates its joints without leaving dangling handles.
        sl::destroy_physics_body(bob);
        const bool deactivated = !sl::physics_joint_active(rod) && sl::physics_joint_active(axle);
        sl::destroy_physics_joint(rod);
        sl::destroy_physics_joint(axle);

        // Debug drawing needs no display; without a screen it must be a safe no-op.
        sl::debug_draw_physics(space);
        joints_passed = rigid && spins && bad_args && deactivated;
        if (!joints_passed)
            std::cerr << "joint results: " << rigid << spins << bad_args << deactivated << "\n";
        sl::destroy_physics_world(space);
    }
    if (!joints_passed)
        return 1;
    if (!passed)
    {
        std::cerr << "Physics wrapper produced an unexpected body state.\n";
        return 1;
    }
    return 0;
}

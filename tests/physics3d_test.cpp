#include "sl.h"

#include <cmath>
#include <cstdio>

namespace
{
    int failures = 0;

    void check(bool ok, const char *what)
    {
        if (!ok)
        {
            std::fprintf(stderr, "FAIL: %s\n", what);
            ++failures;
        }
    }
}

int main()
{
    // A cube dropped flat lands on the floor and comes to rest without sinking.
    {
        sl::RigidBoxWorld3D world;
        sl::RigidBox3D box;
        box.half_extents = glm::vec3(0.5f);
        box.position = {0.0f, 3.0f, 0.0f};
        world.add(box);
        for (int i = 0; i < 600; ++i)
            world.step(1.0f / 60.0f);
        const sl::RigidBox3D &b = world.bodies()[0];
        check(b.position.y > 0.49f && b.position.y < 0.6f, "cube rests on the floor");
        check(glm::length(b.velocity) < 0.2f, "cube comes to rest");
    }
    // A cube landing on a corner picks up spin.
    {
        sl::RigidBoxWorld3D world;
        sl::RigidBox3D box;
        box.position = {0.0f, 2.0f, 0.0f};
        box.orientation = glm::angleAxis(0.6f, glm::normalize(glm::vec3(1.0f, 0.0f, 1.0f)));
        world.add(box);
        float spin = 0.0f;
        for (int i = 0; i < 120; ++i)
        {
            world.step(1.0f / 60.0f);
            spin = std::max(spin, glm::length(world.bodies()[0].angular_velocity));
        }
        check(spin > 0.5f, "off-centre landing makes the cube tumble");
    }
    // Bodies stay above the floor even when thrown hard.
    {
        sl::RigidBoxWorld3D world;
        sl::RigidBox3D box;
        box.position = {0.0f, 1.0f, 0.0f};
        box.velocity = {4.0f, -30.0f, 0.0f};
        box.half_extents = glm::vec3(0.1f);
        world.add(box);
        bool below = false;
        for (int i = 0; i < 300; ++i)
        {
            world.step(1.0f / 60.0f);
            for (const sl::RigidBox3D &b : world.bodies())
                if (b.position.y < 0.05f)
                    below = true;
        }
        check(!below, "a fast cube does not tunnel through the floor");
    }
    // Lifetime removes bodies, and the body cap discards the oldest.
    {
        sl::RigidBoxWorld3D world;
        sl::RigidBox3D box;
        box.lifetime = 0.5f;
        world.add(box);
        for (int i = 0; i < 60; ++i)
            world.step(1.0f / 60.0f);
        check(world.bodies().empty(), "expired bodies are removed");
        world.max_bodies = 3;
        for (int i = 0; i < 10; ++i)
            world.add(sl::RigidBox3D{});
        check(world.bodies().size() == 3, "body cap is enforced");
    }
    // A static box stops a falling cube.
    {
        sl::RigidBoxWorld3D world;
        world.has_floor = false;
        world.obstacles.push_back({{-2.0f, 0.0f, -2.0f}, {2.0f, 1.0f, 2.0f}});
        sl::RigidBox3D box;
        box.half_extents = glm::vec3(0.25f);
        box.position = {0.0f, 4.0f, 0.0f};
        world.add(box);
        for (int i = 0; i < 400; ++i)
            world.step(1.0f / 60.0f);
        check(world.bodies()[0].position.y > 1.0f, "a cube rests on a static box");
    }
    // An explosion pushes bodies outwards, and only those inside the radius.
    {
        sl::RigidBoxWorld3D world;
        world.gravity = glm::vec3(0.0f);
        sl::RigidBox3D near_box, far_box;
        near_box.position = {1.0f, 0.0f, 0.0f};
        far_box.position = {20.0f, 0.0f, 0.0f};
        world.add(near_box);
        world.add(far_box);
        world.explode({0.0f, 0.0f, 0.0f}, 5.0f, 10.0f);
        check(world.bodies()[0].velocity.x > 1.0f, "explosion pushes a nearby body away");
        check(glm::length(world.bodies()[1].velocity) == 0.0f, "explosion leaves distant bodies alone");
    }
    if (failures == 0)
        std::puts("physics3d_test: all checks passed");
    return failures == 0 ? 0 : 1;
}

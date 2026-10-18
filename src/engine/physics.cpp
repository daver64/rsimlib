#include "physics.h"

#include <box2d/box2d.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace sl
{
    namespace
    {
        bool finite(Vec2 value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        bool finite_fixture_properties(float density, float friction, float restitution)
        {
            return std::isfinite(density) && std::isfinite(friction) && std::isfinite(restitution);
        }

        b2Vec2 to_box2d(Vec2 value)
        {
            return {value.x / physics_pixels_per_meter, value.y / physics_pixels_per_meter};
        }

        Vec2 from_box2d(b2Vec2 value)
        {
            return {value.x * physics_pixels_per_meter, value.y * physics_pixels_per_meter};
        }

        b2BodyType to_box2d(BodyType type)
        {
            switch (type)
            {
            case BodyType::static_body:
                return b2_staticBody;
            case BodyType::kinematic_body:
                return b2_kinematicBody;
            case BodyType::dynamic_body:
                return b2_dynamicBody;
            }
            return b2_staticBody;
        }
    }

    struct PhysicsWorld;
    struct PhysicsBody;

    class ContactListener final : public b2ContactListener
    {
    public:
        explicit ContactListener(PhysicsWorld *owner) : owner(owner) {}

        void BeginContact(b2Contact *contact) override;
        void EndContact(b2Contact *contact) override;

        PhysicsWorld *owner;
    };

    struct PhysicsWorld
    {
        explicit PhysicsWorld(Vec2 gravity) : world(to_box2d(gravity)), listener(this)
        {
            world.SetContactListener(&listener);
        }

        b2World world;
        ContactListener listener;
        std::vector<PhysicsContact> contacts;
        std::vector<PhysicsBody *> bodies;
    };

    struct PhysicsBody
    {
        PhysicsWorld *owner = nullptr;
        b2Body *body = nullptr;
    };

    namespace
    {
        void record_contact(PhysicsWorld *world, ContactType type, b2Contact *contact)
        {
            if (!world || !contact)
            {
                return;
            }
            b2Body *bodyA = contact->GetFixtureA()->GetBody();
            b2Body *bodyB = contact->GetFixtureB()->GetBody();
            PhysicsContact event;
            event.type = type;
            event.body_a = reinterpret_cast<PhysicsBody *>(bodyA->GetUserData().pointer);
            event.body_b = reinterpret_cast<PhysicsBody *>(bodyB->GetUserData().pointer);
            if (contact->GetManifold()->pointCount > 0)
            {
                b2WorldManifold manifold;
                contact->GetWorldManifold(&manifold);
                event.point = from_box2d(manifold.points[0]);
                event.normal = {manifold.normal.x, manifold.normal.y};
            }
            world->contacts.push_back(event);
        }
    }

    void ContactListener::BeginContact(b2Contact *contact)
    {
        record_contact(owner, ContactType::begin, contact);
    }

    void ContactListener::EndContact(b2Contact *contact)
    {
        record_contact(owner, ContactType::end, contact);
    }

    PhysicsWorld *create_physics_world(Vec2 gravity)
    {
        return finite(gravity) ? new PhysicsWorld(gravity) : nullptr;
    }

    void destroy_physics_world(PhysicsWorld *world)
    {
        if (!world)
            return;
        world->world.SetContactListener(nullptr);
        for (PhysicsBody *body : world->bodies)
            delete body;
        delete world;
    }

    void step_physics_world(PhysicsWorld *world, float time_step, int velocity_iterations, int position_iterations)
    {
        if (!world || !std::isfinite(time_step) || time_step <= 0.0f)
        {
            return;
        }
        world->world.Step(time_step, std::max(1, velocity_iterations), std::max(1, position_iterations));
    }

    std::vector<PhysicsContact> poll_physics_contacts(PhysicsWorld *world)
    {
        if (!world)
        {
            return {};
        }
        std::vector<PhysicsContact> contacts = std::move(world->contacts);
        world->contacts.clear();
        return contacts;
    }

    PhysicsBody *create_physics_body(PhysicsWorld *world, BodyType type, Vec2 position)
    {
        if (!world || !finite(position))
        {
            return nullptr;
        }
        b2BodyDef definition;
        definition.type = to_box2d(type);
        definition.position = to_box2d(position);
        PhysicsBody *body = new PhysicsBody;
        body->owner = world;
        definition.userData.pointer = reinterpret_cast<uintptr_t>(body);
        body->body = world->world.CreateBody(&definition);
        if (!body->body)
        {
            delete body;
            return nullptr;
        }
        world->bodies.push_back(body);
        return body;
    }

    void destroy_physics_body(PhysicsBody *body)
    {
        if (!body)
        {
            return;
        }
        if (body->owner)
        {
            PhysicsWorld *owner = body->owner;
            if (body->body)
                owner->world.DestroyBody(body->body);
            owner->contacts.erase(std::remove_if(owner->contacts.begin(), owner->contacts.end(),
                                                 [body](const PhysicsContact &contact)
                                                 { return contact.body_a == body || contact.body_b == body; }),
                                  owner->contacts.end());
            owner->bodies.erase(std::remove(owner->bodies.begin(), owner->bodies.end(), body), owner->bodies.end());
        }
        delete body;
    }

    void set_physics_body_transform(PhysicsBody *body, Vec2 position, float angle)
    {
        if (body && body->body && finite(position) && std::isfinite(angle))
        {
            body->body->SetTransform(to_box2d(position), angle);
        }
    }

    Vec2 physics_body_position(const PhysicsBody *body)
    {
        return body && body->body ? from_box2d(body->body->GetPosition()) : Vec2{};
    }

    float physics_body_angle(const PhysicsBody *body)
    {
        return body && body->body ? body->body->GetAngle() : 0.0f;
    }

    Vec2 physics_body_velocity(const PhysicsBody *body)
    {
        return body && body->body ? from_box2d(body->body->GetLinearVelocity()) : Vec2{};
    }

    void set_physics_body_velocity(PhysicsBody *body, Vec2 velocity)
    {
        if (body && body->body && finite(velocity))
        {
            body->body->SetLinearVelocity(to_box2d(velocity));
        }
    }

    void apply_physics_force(PhysicsBody *body, Vec2 force)
    {
        if (body && body->body && finite(force))
        {
            body->body->ApplyForceToCenter(to_box2d(force), true);
        }
    }

    bool add_box_fixture(PhysicsBody *body, float width, float height, float density, float friction, float restitution)
    {
        if (!body || !body->body || !std::isfinite(width) || !std::isfinite(height) ||
            !finite_fixture_properties(density, friction, restitution) || width <= 0.0f || height <= 0.0f)
        {
            return false;
        }
        b2PolygonShape shape;
        shape.SetAsBox(width * 0.5f / physics_pixels_per_meter, height * 0.5f / physics_pixels_per_meter);
        b2FixtureDef fixture;
        fixture.shape = &shape;
        fixture.density = std::max(0.0f, density);
        fixture.friction = std::max(0.0f, friction);
        fixture.restitution = std::clamp(restitution, 0.0f, 1.0f);
        return body->body->CreateFixture(&fixture) != nullptr;
    }

    bool add_circle_fixture(PhysicsBody *body, float radius, float density, float friction, float restitution)
    {
        if (!body || !body->body || !std::isfinite(radius) ||
            !finite_fixture_properties(density, friction, restitution) || radius <= 0.0f)
        {
            return false;
        }
        b2CircleShape shape;
        shape.m_radius = radius / physics_pixels_per_meter;
        b2FixtureDef fixture;
        fixture.shape = &shape;
        fixture.density = std::max(0.0f, density);
        fixture.friction = std::max(0.0f, friction);
        fixture.restitution = std::clamp(restitution, 0.0f, 1.0f);
        return body->body->CreateFixture(&fixture) != nullptr;
    }

    bool add_polygon_fixture(PhysicsBody *body, const std::vector<Vec2> &vertices,
                             float density, float friction, float restitution)
    {
        if (!body || !body->body || vertices.size() < 3 || vertices.size() > b2_maxPolygonVertices ||
            !finite_fixture_properties(density, friction, restitution))
        {
            return false;
        }
        b2Vec2 points[b2_maxPolygonVertices];
        for (std::size_t index = 0; index < vertices.size(); ++index)
        {
            if (!finite(vertices[index]))
                return false;
            points[index] = to_box2d(vertices[index]);
        }
        bool has_area = false;
        for (std::size_t a = 0; a < vertices.size() && !has_area; ++a)
            for (std::size_t b = a + 1; b < vertices.size() && !has_area; ++b)
                for (std::size_t c = b + 1; c < vertices.size(); ++c)
                {
                    const float cross = (points[b].x - points[a].x) * (points[c].y - points[a].y) -
                                        (points[b].y - points[a].y) * (points[c].x - points[a].x);
                    if (std::abs(cross) > b2_linearSlop * b2_linearSlop)
                    {
                        has_area = true;
                        break;
                    }
                }
        if (!has_area)
            return false;
        b2PolygonShape shape;
        shape.Set(points, static_cast<int32>(vertices.size()));
        b2FixtureDef fixture;
        fixture.shape = &shape;
        fixture.density = std::max(0.0f, density);
        fixture.friction = std::max(0.0f, friction);
        fixture.restitution = std::clamp(restitution, 0.0f, 1.0f);
        return body->body->CreateFixture(&fixture) != nullptr;
    }
}

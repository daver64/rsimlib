#include "particles3d.h"

#include <algorithm>
#include <cmath>

namespace sl
{
    namespace
    {
        constexpr float pi = 3.14159265358979f;

        Colour lerp_colour(const Colour &a, const Colour &b, float t)
        {
            const auto mix = [t](std::uint8_t x, std::uint8_t y) { return static_cast<std::uint8_t>(std::clamp(x + (y - x) * t, 0.0f, 255.0f)); };
            return {mix(a.red, b.red), mix(a.green, b.green), mix(a.blue, b.blue), mix(a.alpha, b.alpha)};
        }

        glm::vec3 any_perpendicular(const glm::vec3 &axis)
        {
            const glm::vec3 other = std::fabs(axis.x) < 0.9f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
            return glm::normalize(glm::cross(axis, other));
        }
    } // namespace

    void ParticleSystem3D::spawn(const Particle3D &particle)
    {
        if (particles_.size() >= capacity_)
            particles_.erase(particles_.begin(), particles_.begin() + static_cast<std::ptrdiff_t>(particles_.size() - capacity_ + 1));
        particles_.push_back(particle);
    }

    void ParticleSystem3D::update(float dt)
    {
        for (Particle3D &p : particles_)
        {
            p.age += dt;
            p.velocity += p.acceleration * dt;
            p.velocity *= std::max(0.0f, 1.0f - p.drag * dt);
            p.position += p.velocity * dt;
        }
        particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle3D &p) { return p.age >= p.life; }), particles_.end());
    }

    void ParticleSystem3D::draw(const Mesh *unit_sphere, const glm::mat4 &frame) const
    {
        if (!unit_sphere)
            return;
        Material glow;
        glow.lit = false;
        glow.fog = false;
        glow.cull = CullMode::none;
        // Smoke first so fire that burns in front of it still adds light on top.
        for (const bool additive : {false, true})
        {
            glow.blend = additive ? MaterialBlend::additive : MaterialBlend::alpha;
            for (const Particle3D &p : particles_)
            {
                if (p.additive != additive)
                    continue;
                const float t = std::clamp(p.age / p.life, 0.0f, 1.0f);
                glow.tint = lerp_colour(p.colour_start, p.colour_end, t);
                const float size = p.size_start + (p.size_end - p.size_start) * t;
                draw_mesh(unit_sphere, frame * glm::translate(glm::mat4(1.0f), p.position) * glm::scale(glm::mat4(1.0f), glm::vec3(size)), glow);
            }
        }
    }

    Effect3D::Effect3D(Effect3DKind kind, const glm::vec3 &position, const glm::vec3 &up, float scale, std::uint32_t seed, const glm::vec3 &direction)
        : kind_(kind), position_(position), up_(glm::length(up) > 0.0f ? glm::normalize(up) : glm::vec3(0, 1, 0)),
          direction_(glm::length(direction) > 0.0f ? glm::normalize(direction) : up_), scale_(scale), seed_(seed * 2654435761u + 12345u)
    {
        switch (kind)
        {
        case Effect3DKind::muzzle_flash: duration_ = 0.3f; break;
        case Effect3DKind::smoke: duration_ = 6.0f; break;
        case Effect3DKind::explosion: duration_ = 5.0f; break;
        case Effect3DKind::plasma: duration_ = 1.0f; break;
        case Effect3DKind::mushroom: duration_ = 70.0f; break;
        }
    }

    float Effect3D::random01()
    {
        seed_ = seed_ * 1664525u + 1013904223u;
        return static_cast<float>(seed_ >> 8) / 16777216.0f;
    }

    float Effect3D::light() const
    {
        switch (kind_)
        {
        case Effect3DKind::muzzle_flash: return std::max(0.0f, 1.0f - age_ / 0.08f);
        case Effect3DKind::explosion: return std::exp(-age_ * 3.0f);
        case Effect3DKind::plasma: return std::max(0.0f, 1.0f - age_ / 0.4f);
        case Effect3DKind::mushroom: return std::exp(-age_ / 3.0f);
        default: return 0.0f;
        }
    }

    float Effect3D::light_radius() const
    {
        switch (kind_)
        {
        case Effect3DKind::muzzle_flash: return 8.0f;
        case Effect3DKind::explosion: return scale_ * 6.0f;
        case Effect3DKind::plasma: return 10.0f * scale_;
        case Effect3DKind::mushroom: return 130.0f * std::cbrt(scale_) * 20.0f;
        default: return 0.0f;
        }
    }

    void Effect3D::burst(ParticleSystem3D &system)
    {
        const glm::vec3 u = any_perpendicular(up_), w = glm::cross(up_, u);
        const auto sphere_direction = [&]
        {
            const float z = 2.0f * random01() - 1.0f, a = 2.0f * pi * random01(), r = std::sqrt(std::max(0.0f, 1.0f - z * z));
            return u * (r * std::cos(a)) + w * (r * std::sin(a)) + up_ * z;
        };
        const auto particle = [&](const glm::vec3 &at, const glm::vec3 &v, float life, float s0, float s1, Colour c0, Colour c1, bool additive)
        {
            Particle3D p;
            p.position = at;
            p.velocity = v;
            p.life = life;
            p.size_start = s0;
            p.size_end = s1;
            p.colour_start = c0;
            p.colour_end = c1;
            p.additive = additive;
            return p;
        };
        switch (kind_)
        {
        case Effect3DKind::muzzle_flash:
        {
            system.spawn(particle(position_, glm::vec3(0.0f), 0.07f, 0.12f * scale_, 0.05f * scale_, {255, 240, 170, 230}, {255, 150, 40, 0}, true));
            for (int i = 0; i < 5; ++i)
            {
                Particle3D p = particle(position_, (direction_ + sphere_direction() * 0.35f) * (12.0f + 12.0f * random01()), 0.12f, 0.025f, 0.01f,
                                        {255, 220, 140, 255}, {255, 120, 30, 0}, true);
                system.spawn(p);
            }
            break;
        }
        case Effect3DKind::smoke:
            for (int i = 0; i < 14; ++i)
            {
                Particle3D p = particle(position_ + sphere_direction() * (0.3f * scale_), up_ * (1.0f + 1.5f * random01()) + sphere_direction() * 0.5f,
                                        3.5f + 2.5f * random01(), 0.4f * scale_, 1.8f * scale_, {110, 108, 104, 150}, {70, 70, 70, 0}, false);
                p.drag = 0.5f;
                system.spawn(p);
            }
            break;
        case Effect3DKind::plasma:
            system.spawn(particle(position_, glm::vec3(0.0f), 0.3f, 0.4f * scale_, 1.8f * scale_, {160, 255, 220, 255}, {40, 200, 255, 0}, true));
            for (int i = 0; i < 24; ++i)
                system.spawn(particle(position_, sphere_direction() * (4.0f + 10.0f * random01()) * scale_, 0.35f + 0.4f * random01(), 0.12f * scale_,
                                      0.02f * scale_, {170, 255, 230, 255}, {40, 160, 255, 0}, true));
            break;
        case Effect3DKind::explosion:
        {
            const float r = scale_;
            system.spawn(particle(position_ + up_ * (0.3f * r), glm::vec3(0.0f), 0.3f, 1.1f * r, 2.2f * r, {255, 250, 220, 255}, {255, 180, 60, 0}, true));
            for (int i = 0; i < 34; ++i)
            {
                const glm::vec3 d = sphere_direction();
                Particle3D p = particle(position_ + d * (0.2f * r), d * (0.9f + random01()) * r * 1.6f + up_ * (r * 0.6f), 1.0f + 1.4f * random01(), 0.5f * r,
                                        0.9f * r, {255, 190, 70, 230}, {160, 40, 10, 0}, true);
                p.drag = 1.6f;
                system.spawn(p);
            }
            for (int i = 0; i < 26; ++i)
            {
                const glm::vec3 d = sphere_direction();
                Particle3D p = particle(position_ + d * (0.4f * r), d * (0.5f + random01()) * r * 0.8f + up_ * (r * 0.7f), 3.0f + 2.0f * random01(), 0.5f * r,
                                        1.7f * r, {80, 74, 68, 190}, {40, 40, 40, 0}, false);
                p.drag = 0.8f;
                p.acceleration = up_ * (0.2f * r);
                system.spawn(p);
            }
            for (int i = 0; i < 30; ++i)
            {
                const glm::vec3 d = glm::normalize(sphere_direction() * 0.6f + up_);
                Particle3D p = particle(position_, d * (1.5f + 2.5f * random01()) * r * 1.5f, 1.4f + 1.4f * random01(), 0.04f * r, 0.03f * r,
                                        {255, 200, 120, 255}, {255, 90, 20, 0}, true);
                p.acceleration = -up_ * 9.8f;
                p.drag = 0.1f;
                system.spawn(p);
            }
            break;
        }
        case Effect3DKind::mushroom:
        {
            // The initial fireball: a handful of enormous, bright, fast-fading blobs.
            const float r = 130.0f * std::cbrt(scale_);
            system.spawn(particle(position_ + up_ * (0.6f * r), glm::vec3(0.0f), 1.5f, 1.2f * r, 1.9f * r, {255, 255, 235, 255}, {255, 200, 90, 0}, true));
            for (int i = 0; i < 14; ++i)
            {
                const glm::vec3 d = sphere_direction();
                Particle3D p = particle(position_ + up_ * (0.8f * r) + d * (0.3f * r), d * (0.2f * r) + up_ * (0.8f * r), 4.0f + 3.0f * random01(), 0.7f * r,
                                        1.2f * r, {255, 170, 60, 220}, {150, 40, 10, 0}, true);
                p.drag = 0.3f;
                system.spawn(p);
            }
            break;
        }
        }
    }

    void Effect3D::update(ParticleSystem3D &system, float dt)
    {
        if (pending_)
        {
            burst(system);
            pending_ = false;
        }
        const float previous = age_;
        age_ += dt;
        if (kind_ != Effect3DKind::mushroom || previous > duration_)
            return;

        const float r = 130.0f * std::cbrt(scale_);
        const glm::vec3 u = any_perpendicular(up_), w = glm::cross(up_, u);
        const float t = age_;
        const float height = 22.0f * r * (1.0f - std::exp(-t / 22.0f));
        const float grow = 1.0f - std::exp(-t / 18.0f);
        const float cap_major = r * (0.9f + 2.6f * grow), cap_tube = r * (0.5f + 1.1f * grow);
        const float heat = std::max(0.0f, 1.0f - t / 14.0f);
        const auto particle = [&](const glm::vec3 &at, const glm::vec3 &v, float life, float s0, float s1, Colour c0, Colour c1, bool additive)
        {
            Particle3D p;
            p.position = at;
            p.velocity = v;
            p.life = life;
            p.size_start = s0;
            p.size_end = s1;
            p.colour_start = c0;
            p.colour_end = c1;
            p.additive = additive;
            return p;
        };
        const Colour hot_start = lerp_colour({120, 112, 104, 200}, {255, 150, 55, 200}, heat);

        // Rates per second, applied with a carried remainder so a slow frame still spawns the right number.
        carry_ += dt;
        const float step = 1.0f / 90.0f;
        while (carry_ >= step)
        {
            carry_ -= step;
            // Stem: a rising column of dark smoke, glowing at the base while it is hot.
            {
                const float y = height * random01();
                const float a = 2.0f * pi * random01();
                const float taper = 0.55f + 0.45f * (1.0f - y / std::max(height, 1.0f));
                const float radius = r * 0.32f * taper * std::sqrt(random01());
                const glm::vec3 at = position_ + up_ * y + (u * std::cos(a) + w * std::sin(a)) * radius;
                const bool glow = random01() < 0.4f * heat && y < height * 0.6f;
                system.spawn(particle(at, up_ * (0.08f * height / 22.0f * 5.0f), 7.0f + 4.0f * random01(), r * 0.5f * taper, r * 0.9f * taper,
                                      glow ? Colour{255, 140, 50, 150} : Colour{90, 82, 74, 170}, {50, 50, 52, 0}, glow));
            }
            // Cap: a toroidal vortex, churning outwards over the top and down the outside.
            for (int k = 0; k < 2; ++k)
            {
                const float a = 2.0f * pi * random01(), b = 2.0f * pi * random01();
                const glm::vec3 radial = u * std::cos(a) + w * std::sin(a);
                const glm::vec3 tube = radial * std::cos(b) + up_ * (0.7f * std::sin(b));
                const glm::vec3 at = position_ + up_ * (height + 0.5f * r) + radial * cap_major + tube * (cap_tube * std::sqrt(random01()));
                const bool glow = random01() < 0.55f * heat;
                system.spawn(particle(at, up_ * (height / 22.0f * 0.2f) + radial * (0.03f * r), 6.0f + 3.0f * random01(), r * 0.8f, r * 1.2f,
                                      glow ? Colour{255, 160, 70, 140} : hot_start, {70, 66, 62, 0}, glow));
            }
            // Foot: a low, spreading skirt of dust rolling out from the ground.
            if (t < 45.0f)
            {
                const float a = 2.0f * pi * random01();
                const float ring = std::min(r * 0.8f + 70.0f * t * std::exp(-t / 22.0f) * std::cbrt(scale_) * 1.0f, r * 7.0f);
                const glm::vec3 radial = u * std::cos(a) + w * std::sin(a);
                system.spawn(particle(position_ + radial * ring + up_ * (0.2f * r), radial * (0.05f * r), 6.0f, r * 0.55f, r * 1.0f, {128, 112, 94, 150},
                                      {80, 72, 64, 0}, false));
            }
        }
    }

} // namespace sl

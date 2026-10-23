#include "props3d.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace sl
{
    namespace
    {
        using V = glm::vec3;
        using Ring = std::vector<V>;
        constexpr float pi = 3.14159265358979f;

        const Colour glass_dark{36, 50, 68, 255};
        const Colour glass_lit{255, 222, 138, 255};
        const Colour steel{88, 92, 100, 255};
        const Colour dark_metal{44, 46, 52, 255};

        float hash01(std::uint32_t value)
        {
            value ^= value >> 16;
            value *= 0x7feb352dU;
            value ^= value >> 15;
            value *= 0x846ca68bU;
            value ^= value >> 16;
            return static_cast<float>(value & 0xffffffU) / 16777215.0f;
        }

        Colour shade(Colour colour, float factor)
        {
            const auto scale = [&](std::uint8_t channel)
            { return static_cast<std::uint8_t>(std::clamp(static_cast<float>(channel) * factor, 0.0f, 255.0f)); };
            return {scale(colour.red), scale(colour.green), scale(colour.blue), colour.alpha};
        }

        Colour vary(Colour colour, std::uint32_t seed, float amount)
        {
            return shade(colour, 1.0f + amount * (2.0f * hash01(seed * 977u + 13u) - 1.0f));
        }

        Vertex3D make_vertex(const V &position, const V &normal, Colour colour)
        {
            Vertex3D vertex;
            vertex.position[0] = position.x;
            vertex.position[1] = position.y;
            vertex.position[2] = position.z;
            vertex.normal[0] = normal.x;
            vertex.normal[1] = normal.y;
            vertex.normal[2] = normal.z;
            vertex.colour[0] = colour.red / 255.0f;
            vertex.colour[1] = colour.green / 255.0f;
            vertex.colour[2] = colour.blue / 255.0f;
            vertex.colour[3] = colour.alpha / 255.0f;
            return vertex;
        }

        V centroid(const Ring &ring)
        {
            V sum{0.0f};
            for (const V &point : ring)
                sum += point;
            return sum / static_cast<float>(ring.size());
        }

        /** Emits flat-shaded triangles. Every face is oriented to point away from the `inside` point it is given. */
        struct Builder
        {
            MeshData data;

            void tri(V a, V b, V c, Colour colour, const V &inside)
            {
                V normal = glm::cross(b - a, c - a);
                if (glm::length(normal) < 1e-9f)
                    return;
                normal = glm::normalize(normal);
                if (glm::dot(normal, (a + b + c) / 3.0f - inside) < 0.0f)
                {
                    std::swap(b, c);
                    normal = -normal;
                }
                const auto base = static_cast<std::uint32_t>(data.vertices.size());
                data.vertices.push_back(make_vertex(a, normal, colour));
                data.vertices.push_back(make_vertex(b, normal, colour));
                data.vertices.push_back(make_vertex(c, normal, colour));
                data.indices.insert(data.indices.end(), {base, base + 1, base + 2});
            }

            void quad(const V &a, const V &b, const V &c, const V &d, Colour colour, const V &inside)
            {
                tri(a, b, c, colour, inside);
                tri(a, c, d, colour, inside);
            }

            void loft(const Ring &a, const Ring &b, const std::function<Colour(int)> &colour, const V &inside)
            {
                const int n = static_cast<int>(a.size());
                for (int i = 0; i < n; ++i)
                {
                    const auto j = static_cast<std::size_t>((i + 1) % n);
                    quad(a[static_cast<std::size_t>(i)], a[j], b[j], b[static_cast<std::size_t>(i)], colour(i), inside);
                }
            }

            void loft(const Ring &a, const Ring &b, Colour colour, const V &inside)
            {
                loft(a, b, [&](int) { return colour; }, inside);
            }

            void cap(const Ring &ring, Colour colour, const V &inside)
            {
                const V c = centroid(ring);
                const int n = static_cast<int>(ring.size());
                for (int i = 0; i < n; ++i)
                    tri(c, ring[static_cast<std::size_t>(i)], ring[static_cast<std::size_t>((i + 1) % n)], colour, inside);
            }

            /** A closed solid between two corresponding rings (a convex shape). */
            void prism(const Ring &lo, const Ring &hi, Colour colour)
            {
                const V inside = 0.5f * (centroid(lo) + centroid(hi));
                loft(lo, hi, colour, inside);
                cap(lo, colour, inside);
                cap(hi, colour, inside);
            }

            void append(const MeshData &part, const V &offset, float yaw, float scale)
            {
                const float s = std::sin(yaw);
                const float c = std::cos(yaw);
                const auto base = static_cast<std::uint32_t>(data.vertices.size());
                for (Vertex3D vertex : part.vertices)
                {
                    const float px = vertex.position[0] * scale, py = vertex.position[1] * scale, pz = vertex.position[2] * scale;
                    vertex.position[0] = c * px + s * pz + offset.x;
                    vertex.position[1] = py + offset.y;
                    vertex.position[2] = -s * px + c * pz + offset.z;
                    const float nx = vertex.normal[0], nz = vertex.normal[2];
                    vertex.normal[0] = c * nx + s * nz;
                    vertex.normal[2] = -s * nx + c * nz;
                    data.vertices.push_back(vertex);
                }
                for (const std::uint32_t index : part.indices)
                    data.indices.push_back(index + base);
            }
        };

        Ring rect(float cx, float cz, float hx, float hz, float y)
        {
            return {{cx - hx, y, cz - hz}, {cx + hx, y, cz - hz}, {cx + hx, y, cz + hz}, {cx - hx, y, cz + hz}};
        }

        void perpendicular(const V &axis, V &u, V &v)
        {
            const V reference = std::abs(axis.y) < 0.9f ? V{0.0f, 1.0f, 0.0f} : V{1.0f, 0.0f, 0.0f};
            u = glm::normalize(glm::cross(reference, axis));
            v = glm::cross(axis, u);
        }

        Ring circle(const V &centre, const V &axis, float radius, int sides, float phase = 0.0f)
        {
            V u, v;
            perpendicular(axis, u, v);
            Ring ring;
            for (int i = 0; i < sides; ++i)
            {
                const float angle = phase + 2.0f * pi * static_cast<float>(i) / static_cast<float>(sides);
                ring.push_back(centre + (u * std::cos(angle) + v * std::sin(angle)) * radius);
            }
            return ring;
        }

        /** An elliptical ring in the XY plane at depth z. */
        Ring ring_xy(float z, float cy, float rx, float ry, int sides, float phase = 0.0f)
        {
            Ring ring;
            for (int i = 0; i < sides; ++i)
            {
                const float angle = phase + 2.0f * pi * static_cast<float>(i) / static_cast<float>(sides);
                ring.push_back({rx * std::cos(angle), cy + ry * std::sin(angle), z});
            }
            return ring;
        }

        void box(Builder &b, const V &lo, const V &hi, Colour colour)
        {
            const float cx = 0.5f * (lo.x + hi.x), cz = 0.5f * (lo.z + hi.z);
            b.prism(rect(cx, cz, 0.5f * (hi.x - lo.x), 0.5f * (hi.z - lo.z), lo.y), rect(cx, cz, 0.5f * (hi.x - lo.x), 0.5f * (hi.z - lo.z), hi.y),
                    colour);
        }

        /** A (possibly tapered) cylinder or cone from a to b; a radius of 0 gives an apex. */
        void column(Builder &b, const V &a, const V &c, float r0, float r1, int sides, Colour colour, bool cap_a = true, bool cap_b = true,
                    float phase = 0.0f)
        {
            const V axis = glm::normalize(c - a);
            const Ring ring_a = circle(a, axis, r0, sides, phase);
            const Ring ring_b = circle(c, axis, r1, sides, phase);
            const V inside = 0.5f * (a + c);
            b.loft(ring_a, ring_b, colour, inside);
            if (cap_a && r0 > 1e-5f)
                b.cap(ring_a, colour, inside);
            if (cap_b && r1 > 1e-5f)
                b.cap(ring_b, colour, inside);
        }

        /** Part of a lumpy ellipsoid between two latitudes (radians, -pi/2..pi/2). */
        void ellipsoid(Builder &b, const V &centre, const V &radii, int segments, int rings, float lat0, float lat1, float jitter,
                       std::uint32_t seed, Colour colour, float variation, bool cap_bottom = false)
        {
            std::vector<Ring> grid;
            for (int r = 0; r <= rings; ++r)
            {
                const float lat = lat0 + (lat1 - lat0) * static_cast<float>(r) / static_cast<float>(rings);
                const bool pole = std::abs(std::cos(lat)) < 1e-4f;
                Ring ring;
                for (int s = 0; s < segments; ++s)
                {
                    const float angle = 2.0f * pi * static_cast<float>(s) / static_cast<float>(segments);
                    const float lump = 1.0f + jitter * (2.0f * hash01(seed * 31u + static_cast<std::uint32_t>(r * 131 + (pole ? 0 : s))) - 1.0f);
                    ring.push_back(centre + V{std::cos(lat) * std::sin(angle) * radii.x, std::sin(lat) * radii.y,
                                              std::cos(lat) * std::cos(angle) * radii.z} * lump);
                }
                grid.push_back(ring);
            }
            for (int r = 0; r < rings; ++r)
            {
                for (int s = 0; s < segments; ++s)
                {
                    const auto n = static_cast<std::size_t>((s + 1) % segments);
                    const auto i = static_cast<std::size_t>(s);
                    const Colour facet = shade(colour, 1.0f + variation * (2.0f * hash01(seed * 17u + static_cast<std::uint32_t>(r * 53 + s)) - 1.0f));
                    b.quad(grid[static_cast<std::size_t>(r)][i], grid[static_cast<std::size_t>(r)][n], grid[static_cast<std::size_t>(r + 1)][n],
                           grid[static_cast<std::size_t>(r + 1)][i], facet, centre);
                }
            }
            if (cap_bottom)
                b.cap(grid.front(), shade(colour, 0.7f), centre + V{0.0f, radii.y, 0.0f});
        }

        /** A flat quad facing `normal`, sitting just proud of the surface it is placed on. */
        void panel(Builder &b, const V &centre, const V &normal, float w, float h, Colour colour, const V &up = {0.0f, 1.0f, 0.0f})
        {
            const V right = glm::normalize(glm::cross(up, normal)) * (0.5f * w);
            const V rise = glm::normalize(glm::cross(normal, glm::normalize(glm::cross(up, normal)))) * (0.5f * h);
            const V c = centre + normal * 0.02f;
            b.quad(c - right - rise, c + right - rise, c + right + rise, c - right + rise, colour, centre - normal);
        }

        /** A grid of window panels across one face; `lit` is the fraction that glow. */
        void facade(Builder &b, const V &centre, const V &normal, float length, float y0, int first_floor, int floors, float floor_h,
                    float pitch, float window_w, float window_h, float lit, std::uint32_t seed)
        {
            const V right = glm::cross(V{0.0f, 1.0f, 0.0f}, normal);
            const int columns = std::max(1, static_cast<int>(length / pitch));
            const float step = length / static_cast<float>(columns);
            std::uint32_t id = seed * 7919u + static_cast<std::uint32_t>(std::abs(normal.x * 3.0f + normal.z * 5.0f) + 1.0f) * 131u;
            for (int floor = first_floor; floor < floors; ++floor)
            {
                for (int column = 0; column < columns; ++column)
                {
                    const V position = centre + right * ((static_cast<float>(column) + 0.5f) * step - 0.5f * length) +
                                       V{0.0f, y0 + static_cast<float>(floor) * floor_h + floor_h * 0.55f, 0.0f};
                    const bool on = hash01(++id) < lit;
                    panel(b, V{position.x, position.y, position.z}, normal, window_w, window_h,
                          on ? shade(glass_lit, 0.85f + 0.15f * hash01(id + 99u)) : glass_dark);
                }
            }
        }

        void all_facades(Builder &b, float cx, float cz, float hx, float hz, float y0, int first_floor_front, int floors, float floor_h,
                         float pitch, float window_w, float window_h, float lit, std::uint32_t seed)
        {
            facade(b, {cx, 0.0f, cz + hz}, {0.0f, 0.0f, 1.0f}, 2.0f * hx - 1.0f, y0, first_floor_front, floors, floor_h, pitch, window_w, window_h, lit, seed);
            facade(b, {cx, 0.0f, cz - hz}, {0.0f, 0.0f, -1.0f}, 2.0f * hx - 1.0f, y0, 0, floors, floor_h, pitch, window_w, window_h, lit, seed + 1);
            facade(b, {cx + hx, 0.0f, cz}, {1.0f, 0.0f, 0.0f}, 2.0f * hz - 1.0f, y0, 0, floors, floor_h, pitch, window_w, window_h, lit, seed + 2);
            facade(b, {cx - hx, 0.0f, cz}, {-1.0f, 0.0f, 0.0f}, 2.0f * hz - 1.0f, y0, 0, floors, floor_h, pitch, window_w, window_h, lit, seed + 3);
        }
    } // namespace

    // ----------------------------------------------------------------------------------------------- Buildings

    MeshData make_house_data(float width, float depth, float height, Colour wall, Colour roof, std::uint32_t seed)
    {
        Builder b;
        wall = vary(wall, seed, 0.08f);
        roof = vary(roof, seed + 5u, 0.12f);
        const float hw = 0.5f * width, hd = 0.5f * depth, overhang = 0.3f;
        const float rise = std::min(width, depth) * 0.35f + 0.3f;
        box(b, {-hw, 0.0f, -hd}, {hw, height, hd}, wall);
        const V ridge_a{-hw - overhang, height + rise, 0.0f}, ridge_b{hw + overhang, height + rise, 0.0f};
        const V inside{0.0f, height - 0.5f, 0.0f};
        b.quad({-hw - overhang, height, hd + overhang}, {hw + overhang, height, hd + overhang}, ridge_b, ridge_a, roof, inside);
        b.quad({-hw - overhang, height, -hd - overhang}, {hw + overhang, height, -hd - overhang}, ridge_b, ridge_a, shade(roof, 0.9f), inside);
        for (const float side : {-1.0f, 1.0f})
            b.tri({side * hw, height, -hd}, {side * hw, height, hd}, {side * hw, height + rise, 0.0f}, wall, inside);
        // Chimney, rising through the back slope.
        box(b, {hw * 0.45f, height + rise * 0.2f, -hd * 0.4f}, {hw * 0.45f + 0.5f, height + rise + 0.7f, -hd * 0.4f + 0.5f}, shade(wall, 0.7f));
        panel(b, {0.0f, 1.0f, hd}, {0.0f, 0.0f, 1.0f}, 0.95f, 2.0f, {92, 58, 36, 255});
        const auto window_colour = [&](std::uint32_t k) { return hash01(seed * 41u + k) < 0.5f ? glass_lit : glass_dark; };
        std::uint32_t k = 0;
        for (const float side : {-1.0f, 1.0f})
        {
            panel(b, {side * hw * 0.55f, height * 0.55f, hd}, {0.0f, 0.0f, 1.0f}, 0.9f, 0.9f, window_colour(++k));
            panel(b, {side * hw, height * 0.55f, 0.0f}, {side, 0.0f, 0.0f}, 0.9f, 0.9f, window_colour(++k));
            panel(b, {side * hw * 0.5f, height * 0.55f, -hd}, {0.0f, 0.0f, -1.0f}, 0.9f, 0.9f, window_colour(++k));
        }
        return b.data;
    }

    MeshData make_office_data(float width, float depth, int floors, Colour wall, std::uint32_t seed)
    {
        Builder b;
        floors = std::max(floors, 1);
        wall = vary(wall, seed, 0.06f);
        constexpr float floor_h = 3.5f;
        const float hx = 0.5f * width, hz = 0.5f * depth, total = static_cast<float>(floors) * floor_h;
        box(b, {-hx, 0.0f, -hz}, {hx, total, hz}, wall);
        all_facades(b, 0.0f, 0.0f, hx, hz, 0.0f, 1, floors, floor_h, 2.6f, 1.7f, 1.9f, 0.4f, seed);
        panel(b, {0.0f, 1.3f, hz}, {0.0f, 0.0f, 1.0f}, std::min(3.4f, width * 0.4f), 2.5f, glass_dark);
        // Parapet and rooftop plant.
        const float edge = 0.25f;
        box(b, {-hx, total, hz - edge}, {hx, total + 0.5f, hz}, shade(wall, 0.85f));
        box(b, {-hx, total, -hz}, {hx, total + 0.5f, -hz + edge}, shade(wall, 0.85f));
        box(b, {-hx, total, -hz + edge}, {-hx + edge, total + 0.5f, hz - edge}, shade(wall, 0.85f));
        box(b, {hx - edge, total, -hz + edge}, {hx, total + 0.5f, hz - edge}, shade(wall, 0.85f));
        box(b, {-hx * 0.5f, total, -hz * 0.4f}, {-hx * 0.5f + 2.2f, total + 1.2f, -hz * 0.4f + 1.6f}, steel);
        box(b, {hx * 0.2f, total, hz * 0.1f}, {hx * 0.2f + 1.6f, total + 2.2f, hz * 0.1f + 1.6f}, shade(wall, 0.75f));
        return b.data;
    }

    MeshData make_skyscraper_data(float width, int floors, Colour glass, std::uint32_t seed)
    {
        Builder b;
        floors = std::max(floors, 3);
        glass = vary(glass, seed, 0.06f);
        constexpr float floor_h = 3.6f;
        const int f1 = std::max(1, static_cast<int>(static_cast<float>(floors) * 0.55f));
        const int f2 = std::max(1, static_cast<int>(static_cast<float>(floors) * 0.3f));
        const int f3 = std::max(1, floors - f1 - f2);
        const int tier_floors[3] = {f1, f2, f3};
        const float scale[3] = {1.0f, 0.78f, 0.55f};
        float y = 0.0f;
        float top_hx = 0.0f, top_hz = 0.0f;
        for (int tier = 0; tier < 3; ++tier)
        {
            const float hx = 0.5f * width * scale[tier], hz = 0.5f * width * 0.8f * scale[tier];
            const float height = static_cast<float>(tier_floors[tier]) * floor_h;
            box(b, {-hx, y, -hz}, {hx, y + height, hz}, glass);
            all_facades(b, 0.0f, 0.0f, hx, hz, y, tier == 0 ? 1 : 0, tier_floors[tier], floor_h, 2.0f, 1.4f, 2.1f, 0.35f, seed + static_cast<std::uint32_t>(tier) * 11u);
            if (tier + 1 < 3)
                box(b, {-hx - 0.2f, y + height, -hz - 0.2f}, {hx + 0.2f, y + height + 0.35f, hz + 0.2f}, shade(glass, 0.7f));
            y += height;
            top_hx = hx;
            top_hz = hz;
        }
        panel(b, {0.0f, 1.6f, 0.5f * width * 0.8f}, {0.0f, 0.0f, 1.0f}, std::min(4.0f, width * 0.35f), 3.0f, glass_dark);
        box(b, {-top_hx * 0.5f, y, -top_hz * 0.5f}, {top_hx * 0.5f, y + 2.0f, top_hz * 0.5f}, steel);
        column(b, {0.0f, y + 2.0f, 0.0f}, {0.0f, y + 2.0f + width * 0.7f, 0.0f}, 0.18f, 0.06f, 6, steel);
        box(b, {-0.12f, y + 2.0f + width * 0.7f, -0.12f}, {0.12f, y + 2.0f + width * 0.7f + 0.3f, 0.12f}, {255, 60, 50, 255});
        return b.data;
    }

    MeshData make_hangar_data(float width, float depth, float height, Colour wall, Colour roof)
    {
        Builder b;
        const float hw = 0.5f * width, hd = 0.5f * depth;
        const float wall_h = height * 0.55f, arch_h = height - wall_h;
        std::vector<V> profile{{-hw, 0.0f, 0.0f}, {hw, 0.0f, 0.0f}, {hw, wall_h, 0.0f}};
        constexpr int arc = 8;
        for (int i = 1; i < arc; ++i)
        {
            const float t = pi * static_cast<float>(i) / static_cast<float>(arc);
            profile.push_back({hw * std::cos(t), wall_h + arch_h * std::sin(t), 0.0f});
        }
        profile.push_back({-hw, wall_h, 0.0f});
        Ring front = profile, back = profile;
        for (V &p : front)
            p.z = hd;
        for (V &p : back)
            p.z = -hd;
        const V inside{0.0f, wall_h * 0.5f, 0.0f};
        const int count = static_cast<int>(profile.size());
        b.loft(front, back, [&](int i)
               { return i == 0 ? shade(wall, 0.6f) : (i == 1 || i == count - 1) ? wall : shade(roof, 0.9f + 0.1f * static_cast<float>(i % 2)); },
               inside);
        b.cap(front, wall, inside);
        b.cap(back, wall, inside);
        panel(b, {0.0f, wall_h * 0.9f, hd}, {0.0f, 0.0f, 1.0f}, width * 0.8f, wall_h * 1.8f, {58, 64, 72, 255});
        panel(b, {0.0f, wall_h * 1.9f + 0.1f, hd}, {0.0f, 0.0f, 1.0f}, width * 0.8f, 0.25f, shade(wall, 0.7f));
        return b.data;
    }

    // ----------------------------------------------------------------------------------------------- Structures

    MeshData make_wall_data(float length, float height, float thickness, Colour colour)
    {
        Builder b;
        const float hl = 0.5f * length, ht = 0.5f * thickness;
        box(b, {-hl, 0.0f, -ht}, {hl, height, ht}, colour);
        box(b, {-hl, height, -ht - 0.05f}, {hl, height + 0.15f, ht + 0.05f}, shade(colour, 1.15f));
        for (const float side : {-1.0f, 1.0f})
        {
            const float inner = side * (hl - 0.2f), outer = side * hl;
            box(b, {std::min(inner, outer), 0.0f, -ht - 0.05f}, {std::max(inner, outer), height + 0.2f, ht + 0.05f}, shade(colour, 0.9f));
        }
        for (int course = 1; course < 4; ++course)
        {
            const float y = height * static_cast<float>(course) / 4.0f;
            box(b, {-hl + 0.2f, y - 0.02f, -ht - 0.02f}, {hl - 0.2f, y + 0.02f, ht + 0.02f}, shade(colour, 0.8f));
        }
        return b.data;
    }

    MeshData make_fence_data(float length, float height, Colour colour)
    {
        Builder b;
        const float hl = 0.5f * length;
        const int bays = 4;
        for (int i = 0; i <= bays; ++i)
        {
            const float x = -hl + length * static_cast<float>(i) / static_cast<float>(bays);
            box(b, {x - 0.06f, 0.0f, -0.06f}, {x + 0.06f, height * 1.05f, 0.06f}, shade(colour, 0.8f));
        }
        for (const float level : {0.3f, 0.75f})
            box(b, {-hl, height * level, 0.06f}, {hl, height * level + 0.08f, 0.1f}, shade(colour, 0.9f));
        const int pickets = std::max(2, static_cast<int>(length / 0.25f));
        for (int i = 0; i < pickets; ++i)
        {
            const float x = -hl + (static_cast<float>(i) + 0.5f) * length / static_cast<float>(pickets);
            const float hw = 0.5f * length / static_cast<float>(pickets) * 0.6f;
            box(b, {x - hw, 0.0f, 0.1f}, {x + hw, height * 0.92f, 0.14f}, colour);
            b.tri({x - hw, height * 0.92f, 0.12f}, {x + hw, height * 0.92f, 0.12f}, {x, height, 0.12f}, colour, {x, height * 0.5f, -1.0f});
            b.tri({x - hw, height * 0.92f, 0.12f}, {x + hw, height * 0.92f, 0.12f}, {x, height, 0.12f}, colour, {x, height * 0.5f, 1.0f});
        }
        return b.data;
    }

    MeshData make_water_tower_data(float height, float tank_radius, Colour tank)
    {
        Builder b;
        const float tank_y0 = height * 0.6f, tank_y1 = height * 0.88f;
        const float foot = tank_radius * 0.95f, top = tank_radius * 0.7f;
        for (int leg = 0; leg < 4; ++leg)
        {
            const float sx = (leg & 1) ? 1.0f : -1.0f, sz = (leg & 2) ? 1.0f : -1.0f;
            column(b, {sx * foot, 0.03f, sz * foot}, {sx * top, tank_y0, sz * top}, 0.16f, 0.14f, 5, steel);
        }
        for (const float level : {0.22f, 0.42f})
        {
            const float f = foot + (top - foot) * level / 0.6f;
            for (int side = 0; side < 4; ++side)
            {
                const float s = (side & 1) ? 1.0f : -1.0f;
                if (side < 2)
                    column(b, {-f, height * level, s * f}, {f, height * level, s * f}, 0.07f, 0.07f, 4, dark_metal, false, false);
                else
                    column(b, {s * f, height * level, -f}, {s * f, height * level, f}, 0.07f, 0.07f, 4, dark_metal, false, false);
            }
        }
        column(b, {0.0f, tank_y0 - 0.5f, 0.0f}, {0.0f, tank_y0, 0.0f}, tank_radius * 0.55f, tank_radius, 10, shade(tank, 0.8f));
        column(b, {0.0f, tank_y0, 0.0f}, {0.0f, tank_y1, 0.0f}, tank_radius, tank_radius, 10, tank, false, false);
        column(b, {0.0f, tank_y1, 0.0f}, {0.0f, height, 0.0f}, tank_radius * 1.08f, 0.0f, 10, {96, 52, 44, 255});
        return b.data;
    }

    MeshData make_radar_data(float height, float dish_radius, Colour colour)
    {
        Builder b;
        height = std::max(height, dish_radius + 1.5f);
        box(b, {-1.4f, 0.0f, -1.4f}, {1.4f, 0.9f, 1.4f}, shade(colour, 0.7f));
        column(b, {0.0f, 0.9f, 0.0f}, {0.0f, height - 0.4f, 0.0f}, 0.34f, 0.26f, 8, steel);
        box(b, {-0.45f, height - 0.4f, -0.45f}, {0.45f, height + 0.2f, 0.45f}, dark_metal);
        const float tilt = 0.45f;
        const V axis = glm::normalize(V{0.0f, std::sin(tilt), std::cos(tilt)});
        const V hub{0.0f, height + 0.2f, 0.0f};
        const float depth = dish_radius * 0.3f;
        const V rim = hub + axis * depth;
        const Ring rim_ring = circle(rim, axis, dish_radius, 14);
        const Ring back_ring = circle(hub, axis, dish_radius * 0.12f, 14);
        const V inside = hub - axis * 2.0f;
        b.loft(back_ring, rim_ring, colour, hub + axis * 0.1f);
        const V bowl = hub + axis * (depth * 0.45f);
        for (std::size_t i = 0; i < rim_ring.size(); ++i)
            b.tri(bowl, rim_ring[i], rim_ring[(i + 1) % rim_ring.size()], shade(colour, 1.05f), inside);
        column(b, bowl, bowl + axis * (dish_radius * 0.55f), 0.07f, 0.07f, 5, dark_metal);
        box(b, bowl + axis * (dish_radius * 0.55f) - V{0.14f}, bowl + axis * (dish_radius * 0.55f) + V{0.14f}, {220, 60, 50, 255});
        return b.data;
    }

    MeshData make_domed_radar_data(float radius, Colour colour)
    {
        Builder b;
        const float plinth = radius * 0.45f;
        column(b, {0.0f, 0.0f, 0.0f}, {0.0f, plinth, 0.0f}, radius * 1.05f, radius * 1.05f, 12, shade(colour, 0.7f));
        ellipsoid(b, {0.0f, plinth, 0.0f}, {radius, radius, radius}, 12, 5, 0.0f, 0.5f * pi, 0.0f, 1u, colour, 0.05f, false);
        column(b, {0.0f, plinth + radius - 0.02f, 0.0f}, {0.0f, plinth + radius + 0.35f, 0.0f}, 0.1f, 0.05f, 5, steel);
        panel(b, {0.0f, 0.8f, radius * 1.0f}, {0.0f, 0.0f, 1.0f}, radius * 0.45f, 1.6f, {68, 72, 80, 255});
        return b.data;
    }

    MeshData make_control_tower_data(float height, std::uint32_t seed)
    {
        Builder b;
        height = std::max(height, 14.0f);
        const Colour concrete = vary({196, 194, 188, 255}, seed, 0.05f);
        const float base_h = 4.5f;
        box(b, {-7.0f, 0.0f, -4.5f}, {7.0f, base_h, 4.5f}, shade(concrete, 0.9f));
        all_facades(b, 0.0f, 0.0f, 7.0f, 4.5f, 0.0f, 1, 1, 4.5f, 2.8f, 1.8f, 1.5f, 0.3f, seed);
        panel(b, {0.0f, 1.2f, 4.5f}, {0.0f, 0.0f, 1.0f}, 2.6f, 2.4f, glass_dark);
        box(b, {-7.2f, base_h, -4.7f}, {7.2f, base_h + 0.3f, 4.7f}, shade(concrete, 0.7f));
        const float cab_y = height - 5.0f;
        column(b, {0.0f, base_h, 0.0f}, {0.0f, cab_y, 0.0f}, 2.3f, 1.8f, 8, concrete, false, false, 0.3927f);
        column(b, {0.0f, cab_y, 0.0f}, {0.0f, cab_y + 1.2f, 0.0f}, 1.8f, 3.2f, 8, shade(concrete, 0.8f), false, false, 0.3927f);
        const float glass_y = cab_y + 1.2f;
        column(b, {0.0f, glass_y, 0.0f}, {0.0f, glass_y + 2.4f, 0.0f}, 3.2f, 3.4f, 8, glass_dark, false, false, 0.3927f);
        column(b, {0.0f, glass_y + 2.4f, 0.0f}, {0.0f, glass_y + 2.8f, 0.0f}, 3.9f, 3.9f, 8, shade(concrete, 0.75f), true, true, 0.3927f);
        for (int i = 0; i < 8; ++i)
        {
            const float a0 = 2.0f * pi * static_cast<float>(i) / 8.0f + 0.3927f;
            column(b, {std::sin(a0) * 3.33f, glass_y, std::cos(a0) * 3.33f}, {std::sin(a0) * 3.43f, glass_y + 2.4f, std::cos(a0) * 3.43f}, 0.06f, 0.06f, 4,
                   concrete, false, false);
        }
        const float roof = glass_y + 2.8f;
        column(b, {0.0f, roof, 0.0f}, {0.0f, roof + 3.2f, 0.0f}, 0.1f, 0.04f, 5, steel);
        box(b, {-0.9f, roof, -0.4f}, {-0.1f, roof + 0.5f, 0.4f}, steel);
        box(b, {-0.09f, roof + 3.2f, -0.09f}, {0.09f, roof + 3.4f, 0.09f}, {255, 60, 50, 255});
        return b.data;
    }

    MeshData make_street_lamp_data(float height, Colour colour)
    {
        Builder b;
        const float arm = 0.9f;
        column(b, {0.0f, 0.0f, 0.0f}, {0.0f, 0.3f, 0.0f}, 0.16f, 0.12f, 6, shade(colour, 0.8f));
        column(b, {0.0f, 0.3f, 0.0f}, {0.0f, height, 0.0f}, 0.08f, 0.06f, 6, colour, false, true);
        box(b, {-0.04f, height - 0.04f, 0.0f}, {0.04f, height + 0.04f, arm}, colour);
        box(b, {-0.14f, height, arm - 0.3f}, {0.14f, height + 0.1f, arm + 0.3f}, shade(colour, 0.9f));
        b.quad({-0.12f, height - 0.0f, arm - 0.28f}, {0.12f, height, arm - 0.28f}, {0.12f, height, arm + 0.28f}, {-0.12f, height, arm + 0.28f},
               {255, 236, 170, 255}, {0.0f, height + 1.0f, arm});
        return b.data;
    }

    MeshData make_road_data(float length, float width)
    {
        Builder b;
        const float hw = 0.5f * width, hl = 0.5f * length;
        const float kerb = std::min(1.0f, width * 0.15f);
        const Colour asphalt{62, 63, 68, 255};
        box(b, {-hw + kerb, 0.0f, -hl}, {hw - kerb, 0.06f, hl}, asphalt);
        for (const float side : {-1.0f, 1.0f})
        {
            const float x0 = side * hw, x1 = side * (hw - kerb);
            box(b, {std::min(x0, x1), 0.0f, -hl}, {std::max(x0, x1), 0.15f, hl}, {160, 158, 152, 255});
            const float line = side * (hw - kerb - 0.25f);
            b.quad({line - 0.06f, 0.065f, -hl}, {line + 0.06f, 0.065f, -hl}, {line + 0.06f, 0.065f, hl}, {line - 0.06f, 0.065f, hl}, {220, 220, 215, 255},
                   {line, -1.0f, 0.0f});
        }
        const int dashes = std::max(1, static_cast<int>(length / 3.0f));
        for (int i = 0; i < dashes; ++i)
        {
            const float z = -hl + (static_cast<float>(i) + 0.5f) * length / static_cast<float>(dashes);
            b.quad({-0.08f, 0.065f, z - 0.7f}, {0.08f, 0.065f, z - 0.7f}, {0.08f, 0.065f, z + 0.7f}, {-0.08f, 0.065f, z + 0.7f}, {235, 220, 120, 255},
                   {0.0f, -1.0f, z});
        }
        return b.data;
    }

    // ----------------------------------------------------------------------------------------------- Small props

    MeshData make_barrel_data(float radius, float height, Colour colour)
    {
        Builder b;
        const V bottom{0.0f, 0.0f, 0.0f}, middle{0.0f, height * 0.5f, 0.0f}, top{0.0f, height, 0.0f};
        const Ring r0 = circle(bottom, {0.0f, 1.0f, 0.0f}, radius * 0.88f, 10);
        const Ring r1 = circle(middle, {0.0f, 1.0f, 0.0f}, radius, 10);
        const Ring r2 = circle(top, {0.0f, 1.0f, 0.0f}, radius * 0.88f, 10);
        b.loft(r0, r1, colour, middle);
        b.loft(r1, r2, colour, middle);
        b.cap(r0, shade(colour, 0.7f), middle);
        b.cap(r2, shade(colour, 1.15f), middle);
        for (const float level : {0.2f, 0.8f})
        {
            const float r = radius * (0.88f + 0.12f * (1.0f - std::abs(level - 0.5f) * 2.0f)) + 0.015f;
            column(b, {0.0f, height * level - 0.03f, 0.0f}, {0.0f, height * level + 0.03f, 0.0f}, r, r, 10, dark_metal, false, false);
        }
        return b.data;
    }

    MeshData make_crate_data(float size, Colour colour)
    {
        Builder b;
        const float h = 0.5f * size, t = 0.09f * size;
        box(b, {-h + 0.01f, 0.0f, -h + 0.01f}, {h - 0.01f, size, h - 0.01f}, colour);
        const Colour frame = shade(colour, 0.72f);
        for (const float sx : {-1.0f, 1.0f})
        {
            for (const float sz : {-1.0f, 1.0f})
            {
                const float cx = sx * (h - t * 0.5f + 0.02f), cz = sz * (h - t * 0.5f + 0.02f);
                box(b, {cx - t * 0.5f - 0.01f, 0.0f, cz - t * 0.5f - 0.01f}, {cx + t * 0.5f + 0.01f, size, cz + t * 0.5f + 0.01f}, frame);
            }
        }
        for (const float y : {0.0f, size - t})
        {
            for (const float s : {-1.0f, 1.0f})
            {
                const float near_edge = s > 0 ? h - t : -h, far_edge = s > 0 ? h : -h + t;
                box(b, {-h, y, near_edge}, {h, y + t, far_edge}, frame);
                box(b, {near_edge, y, -h}, {far_edge, y + t, h}, frame);
            }
        }
        return b.data;
    }

    MeshData make_container_data(float length, float width, float height, Colour colour, std::uint32_t seed)
    {
        Builder b;
        colour = vary(colour, seed, 0.18f);
        const float hl = 0.5f * length, hw = 0.5f * width;
        box(b, {-hw, 0.0f, -hl}, {hw, height, hl}, colour);
        const int ribs = std::max(2, static_cast<int>(length / 0.3f));
        const Colour rib = shade(colour, 0.86f);
        for (int i = 0; i < ribs; ++i)
        {
            const float z = -hl + 0.3f + (static_cast<float>(i) + 0.5f) * (length - 0.6f) / static_cast<float>(ribs);
            for (const float side : {-1.0f, 1.0f})
                box(b, {side > 0 ? hw : -hw - 0.05f, 0.12f, z - 0.06f}, {side > 0 ? hw + 0.05f : -hw, height - 0.12f, z + 0.06f}, rib);
        }
        const Colour frame = shade(colour, 0.7f);
        box(b, {-hw - 0.03f, 0.0f, -hl}, {hw + 0.03f, 0.12f, hl}, frame);
        box(b, {-hw - 0.03f, height - 0.12f, -hl}, {hw + 0.03f, height, hl}, frame);
        for (const float side : {-1.0f, 1.0f})
        {
            for (const float shift : {0.13f, 0.5f, 0.87f})
                box(b, {side * hw * shift - 0.02f, 0.12f, hl}, {side * hw * shift + 0.02f, height - 0.12f, hl + 0.04f}, steel);
        }
        panel(b, {0.0f, height * 0.5f, hl}, {0.0f, 0.0f, 1.0f}, 0.03f, height - 0.3f, dark_metal);
        return b.data;
    }

    MeshData make_tank_data(float radius, float height, Colour colour)
    {
        Builder b;
        column(b, {0.0f, 0.0f, 0.0f}, {0.0f, 0.3f, 0.0f}, radius * 1.04f, radius * 1.04f, 14, shade(colour, 0.6f));
        column(b, {0.0f, 0.3f, 0.0f}, {0.0f, height, 0.0f}, radius, radius, 14, colour, false, false);
        ellipsoid(b, {0.0f, height, 0.0f}, {radius, radius * 0.3f, radius}, 14, 3, 0.0f, 0.5f * pi, 0.0f, 3u, shade(colour, 1.05f), 0.0f);
        const float lx = 0.0f, lz = radius + 0.1f;
        for (const float side : {-1.0f, 1.0f})
            box(b, {lx + side * 0.25f - 0.03f, 0.3f, lz - 0.05f}, {lx + side * 0.25f + 0.03f, height, lz + 0.03f}, steel);
        for (int i = 1; i < static_cast<int>(height / 0.5f); ++i)
            box(b, {-0.25f, 0.3f + 0.5f * static_cast<float>(i) - 0.02f, lz - 0.04f}, {0.25f, 0.3f + 0.5f * static_cast<float>(i) + 0.02f, lz + 0.0f}, steel);
        column(b, {0.0f, 0.8f, -radius + 0.1f}, {0.0f, 0.8f, -radius - 1.2f}, 0.16f, 0.16f, 6, steel);
        column(b, {0.0f, 0.0f, -radius - 1.2f}, {0.0f, 0.8f, -radius - 1.2f}, 0.16f, 0.16f, 6, steel, false, false);
        return b.data;
    }

    // ----------------------------------------------------------------------------------------------- Nature

    MeshData make_tree_data(float height, float radius, Colour trunk, Colour foliage, std::uint32_t seed)
    {
        Builder b;
        trunk = vary(trunk, seed, 0.1f);
        foliage = vary(foliage, seed + 3u, 0.12f);
        column(b, {0.0f, 0.0f, 0.0f}, {0.0f, height * 0.6f, 0.0f}, radius * 0.17f, radius * 0.09f, 6, trunk, false, false);
        const float cy = height * 0.64f, ry = height * 0.32f;
        ellipsoid(b, {0.0f, cy, 0.0f}, {radius, ry, radius}, 8, 4, -0.5f * pi, 0.5f * pi, 0.18f, seed, foliage, 0.1f);
        for (int blob = 0; blob < 2; ++blob)
        {
            const float angle = 2.0f * pi * hash01(seed * 5u + static_cast<std::uint32_t>(blob)) + static_cast<float>(blob) * pi;
            ellipsoid(b, {std::sin(angle) * radius * 0.55f, cy - ry * 0.25f + static_cast<float>(blob) * ry * 0.5f, std::cos(angle) * radius * 0.55f},
                      {radius * 0.62f, ry * 0.7f, radius * 0.62f}, 7, 3, -0.5f * pi, 0.5f * pi, 0.2f, seed + 9u + static_cast<std::uint32_t>(blob),
                      shade(foliage, 0.92f), 0.1f);
        }
        return b.data;
    }

    MeshData make_bush_data(float radius, Colour colour, std::uint32_t seed)
    {
        Builder b;
        colour = vary(colour, seed, 0.12f);
        ellipsoid(b, {0.0f, 0.0f, 0.0f}, {radius, radius * 0.8f, radius}, 7, 3, 0.0f, 0.5f * pi, 0.2f, seed, colour, 0.12f, true);
        ellipsoid(b, {radius * 0.6f, 0.0f, radius * 0.2f}, {radius * 0.6f, radius * 0.6f, radius * 0.6f}, 6, 3, 0.0f, 0.5f * pi, 0.2f,
                  seed + 4u, shade(colour, 0.9f), 0.12f, true);
        return b.data;
    }

    MeshData make_rock_cluster_data(float radius, Colour colour, std::uint32_t seed)
    {
        Builder b;
        const int count = 3 + static_cast<int>(hash01(seed) * 2.99f);
        for (int i = 0; i < count; ++i)
        {
            const auto id = seed * 13u + static_cast<std::uint32_t>(i);
            const float angle = 2.0f * pi * (static_cast<float>(i) + hash01(id)) / static_cast<float>(count);
            const float size = (i == 0 ? 0.55f : 0.25f + 0.25f * hash01(id + 7u)) * radius;
            const float spread = i == 0 ? 0.0f : radius * (0.45f + 0.3f * hash01(id + 3u));
            b.append(make_rock_data(size, vary(colour, id, 0.1f), id), {std::sin(angle) * spread, 0.0f, std::cos(angle) * spread},
                     2.0f * pi * hash01(id + 11u), 1.0f);
        }
        return b.data;
    }

    // ----------------------------------------------------------------------------------------------- Vehicles

    MeshData make_car_data(float length, Colour body, std::uint32_t seed)
    {
        Builder b;
        body = vary(body, seed, 0.1f);
        const float k = length / 4.4f;
        const float hw = 0.9f * k, hl = 0.5f * length, wheel_r = 0.33f * k;
        const Colour tyre{26, 26, 30, 255};
        box(b, {-hw, 0.3f * k, -hl}, {hw, 0.85f * k, hl}, body);
        const Ring cab_lo = rect(0.0f, -0.12f * k, hw - 0.08f * k, 1.2f * k, 0.85f * k);
        const Ring cab_hi = rect(0.0f, -0.28f * k, hw - 0.22f * k, 0.78f * k, 1.4f * k);
        const V inside{0.0f, 1.0f * k, -0.2f * k};
        b.loft(cab_lo, cab_hi, glass_dark, inside);
        b.cap(cab_hi, shade(body, 1.05f), inside);
        for (const float sx : {-1.0f, 1.0f})
        {
            for (const float z : {-0.3f * length, 0.3f * length})
            {
                const float x = sx * (hw - 0.04f * k);
                column(b, {x - 0.14f * k, wheel_r, z}, {x + 0.14f * k, wheel_r, z}, wheel_r, wheel_r, 8, tyre);
                column(b, {x + sx * 0.12f * k, wheel_r, z}, {x + sx * 0.16f * k, wheel_r, z}, wheel_r * 0.55f, wheel_r * 0.55f, 8, {170, 172, 178, 255});
                box(b, {x - 0.17f * k, wheel_r * 1.2f, z - wheel_r * 1.35f}, {x + 0.17f * k, 0.5f * k, z + wheel_r * 1.35f}, shade(body, 0.55f));
            }
            panel(b, {sx * hw * 0.6f, 0.62f * k, hl}, {0.0f, 0.0f, 1.0f}, 0.38f * k, 0.16f * k, {255, 244, 200, 255});
            panel(b, {sx * hw * 0.6f, 0.65f * k, -hl}, {0.0f, 0.0f, -1.0f}, 0.34f * k, 0.14f * k, {220, 30, 30, 255});
        }
        box(b, {-hw, 0.2f * k, hl - 0.05f * k}, {hw, 0.34f * k, hl + 0.08f * k}, steel);
        box(b, {-hw, 0.2f * k, -hl - 0.08f * k}, {hw, 0.34f * k, -hl + 0.05f * k}, steel);
        return b.data;
    }

    MeshData make_spaceship_data(float length, Colour hull)
    {
        Builder b;
        const float R = length * 0.14f, leg_h = R * 0.55f, cy = leg_h + R * 0.9f + 0.1f;
        const float L = length;
        struct Station
        {
            float z, rx, ry;
        };
        const Station stations[] = {{-0.5f, 0.55f, 0.5f}, {-0.36f, 0.8f, 0.75f}, {-0.1f, 1.0f, 0.9f}, {0.2f, 0.85f, 0.72f}, {0.38f, 0.45f, 0.36f}, {0.5f, 0.03f, 0.03f}};
        constexpr int sides = 8;
        constexpr float phase = pi / 8.0f;
        Ring previous;
        for (const Station &station : stations)
        {
            Ring ring = ring_xy(station.z * L, cy, R * station.rx, R * station.ry, sides, phase);
            if (!previous.empty())
                b.loft(previous, ring, [&](int i)
                       { return std::sin(phase + 2.0f * pi * (static_cast<float>(i) + 0.5f) / static_cast<float>(sides)) < 0.0f ? shade(hull, 0.75f) : hull; },
                       {0.0f, cy, station.z * L});
            else
                b.cap(ring, dark_metal, {0.0f, cy, station.z * L + 1.0f});
            previous = ring;
        }
        // Canopy.
        ellipsoid(b, {0.0f, cy + R * 0.45f, L * 0.2f}, {R * 0.5f, R * 0.42f, L * 0.13f}, 8, 3, -0.15f * pi, 0.5f * pi, 0.0f, 2u, {96, 190, 230, 255}, 0.05f);
        // Swept wings, thin slabs.
        for (const float s : {-1.0f, 1.0f})
        {
            const float y = cy - R * 0.35f;
            const Ring lo{{s * R * 0.7f, y, 0.15f * L}, {s * L * 0.48f, y - 0.1f * R, -0.3f * L}, {s * L * 0.48f, y - 0.1f * R, -0.4f * L},
                          {s * R * 0.7f, y, -0.36f * L}};
            Ring hi = lo;
            for (V &p : hi)
                p.y += R * 0.14f;
            b.prism(lo, hi, shade(hull, 0.9f));
            box(b, {s * L * 0.48f - 0.08f, y - 0.1f * R, -0.4f * L}, {s * L * 0.48f + 0.08f, y + 0.05f * R, -0.3f * L}, {230, 80, 60, 255});
            // Engine pod with a glowing exhaust.
            const float ex = s * R * 1.5f;
            column(b, {ex, y - 0.15f * R, -0.08f * L}, {ex, y - 0.15f * R, -0.5f * L}, R * 0.28f, R * 0.22f, 8, shade(hull, 0.65f), true, false);
            const Ring glow = circle({ex, y - 0.15f * R, -0.5f * L}, {0.0f, 0.0f, 1.0f}, R * 0.2f, 8);
            b.cap(glow, {120, 210, 255, 255}, {ex, y - 0.15f * R, -0.2f * L});
        }
        // Tail fin.
        {
            const float x = 0.06f * R;
            const Ring lo{{-x, cy + R * 0.7f, -0.12f * L}, {-x, cy + R * 2.6f, -0.42f * L}, {-x, cy + R * 2.6f, -0.5f * L}, {-x, cy + R * 0.3f, -0.5f * L}};
            Ring hi = lo;
            for (V &p : hi)
                p.x = x;
            b.prism(lo, hi, {230, 80, 60, 255});
        }
        const Ring core = circle({0.0f, cy, -0.5f * L - 0.02f}, {0.0f, 0.0f, 1.0f}, R * 0.4f, 8, phase);
        b.cap(core, {150, 225, 255, 255}, {0.0f, cy, -0.3f * L});
        // Landing legs.
        const V feet[] = {{-R * 0.75f, 0.0f, -0.18f * L}, {R * 0.75f, 0.0f, -0.18f * L}, {0.0f, 0.0f, 0.26f * L}};
        for (const V &foot : feet)
        {
            column(b, {foot.x, 0.04f, foot.z}, {foot.x * 0.8f, cy - R * 0.5f, foot.z}, R * 0.08f, R * 0.06f, 5, steel, false, false);
            column(b, {foot.x, 0.0f, foot.z}, {foot.x, 0.06f, foot.z}, R * 0.28f, R * 0.26f, 8, dark_metal);
        }
        return b.data;
    }

    MeshData make_aeroplane_data(float wingspan, Colour body, Colour stripe)
    {
        Builder b;
        const float L = wingspan * 0.95f, R = wingspan * 0.07f;
        const float cy = R * 2.6f;
        struct Station
        {
            float z, r;
        };
        const Station stations[] = {{-0.5f, 0.08f}, {-0.4f, 0.4f}, {-0.2f, 1.0f}, {0.26f, 1.0f}, {0.4f, 0.76f}, {0.5f, 0.12f}};
        constexpr int sides = 8;
        constexpr float phase = pi / 8.0f;
        Ring previous;
        for (const Station &station : stations)
        {
            const Ring ring = ring_xy(station.z * L, station.z < -0.3f ? cy + R * 0.25f : cy, R * station.r, R * station.r, sides, phase);
            if (!previous.empty())
                b.loft(previous, ring, [&](int i)
                       {
                           const float a = phase + 2.0f * pi * (static_cast<float>(i) + 0.5f) / static_cast<float>(sides);
                           return std::sin(a) < -0.4f ? stripe : body;
                       },
                       {0.0f, cy, station.z * L});
            previous = ring;
        }
        const float flat = R * std::cos(pi / 8.0f);
        for (const float s : {-1.0f, 1.0f})
        {
            for (int i = 0; i < 13; ++i)
                panel(b, {s * flat, cy + R * 0.15f, (-0.18f + 0.032f * static_cast<float>(i)) * L}, {s, 0.0f, 0.0f}, L * 0.02f, R * 0.3f, glass_dark);
            panel(b, {s * R * 0.3f, cy + R * 0.28f, 0.42f * L}, {s * 0.6f, 0.2f, 0.7f}, R * 0.5f, R * 0.25f, glass_dark, {0.0f, 1.0f, 0.0f});
        }
        // Wings, tail planes and engines.
        for (const float s : {-1.0f, 1.0f})
        {
            const float y = cy - R * 0.5f;
            const Ring lo{{s * R * 0.9f, y, 0.06f * L}, {s * wingspan * 0.5f, y + R * 0.5f, -0.14f * L}, {s * wingspan * 0.5f, y + R * 0.5f, -0.2f * L},
                          {s * R * 0.9f, y, -0.14f * L}};
            Ring hi = lo;
            for (V &p : hi)
                p.y += R * 0.12f;
            b.prism(lo, hi, shade(body, 0.95f));
            const Ring tail_lo{{s * R * 0.4f, cy + R * 0.3f, -0.4f * L}, {s * wingspan * 0.2f, cy + R * 0.55f, -0.47f * L},
                               {s * wingspan * 0.2f, cy + R * 0.55f, -0.5f * L}, {s * R * 0.4f, cy + R * 0.3f, -0.5f * L}};
            Ring tail_hi = tail_lo;
            for (V &p : tail_hi)
                p.y += R * 0.08f;
            b.prism(tail_lo, tail_hi, shade(body, 0.95f));
            const float ex = s * wingspan * 0.19f, ey = y - R * 0.5f;
            column(b, {ex, ey, 0.06f * L}, {ex, ey, -0.1f * L}, R * 0.42f, R * 0.4f, 8, shade(body, 0.9f), false, true);
            const Ring intake = circle({ex, ey, 0.06f * L}, {0.0f, 0.0f, 1.0f}, R * 0.34f, 8);
            b.cap(intake, dark_metal, {ex, ey, -0.05f * L});
            column(b, {ex, ey + R * 0.4f, 0.0f}, {ex, y + R * 0.1f, 0.0f}, R * 0.06f, R * 0.06f, 4, steel, false, false);
        }
        {
            const Ring lo{{-R * 0.07f, cy + R * 0.8f, -0.3f * L}, {-R * 0.07f, cy + R * 3.2f, -0.47f * L}, {-R * 0.07f, cy + R * 3.2f, -0.5f * L},
                          {-R * 0.07f, cy + R * 0.5f, -0.5f * L}};
            Ring hi = lo;
            for (V &p : hi)
                p.x = R * 0.07f;
            b.prism(lo, hi, stripe);
        }
        // Landing gear: nose gear and a main gear under the wing roots.
        const float wheel_r = R * 0.32f;
        const V gear[] = {{0.0f, 0.0f, 0.38f * L}, {-R * 1.0f, 0.0f, -0.02f * L}, {R * 1.0f, 0.0f, -0.02f * L}};
        for (const V &g : gear)
        {
            column(b, {g.x, wheel_r, g.z}, {g.x * 0.8f, cy - R * 0.6f, g.z}, R * 0.07f, R * 0.07f, 4, steel, false, false);
            column(b, {g.x - wheel_r * 0.4f, wheel_r, g.z}, {g.x + wheel_r * 0.4f, wheel_r, g.z}, wheel_r, wheel_r, 8, {26, 26, 30, 255});
        }
        return b.data;
    }
} // namespace sl

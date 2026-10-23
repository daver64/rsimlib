#include "clutter.h"

#include "display.h"
#include "error.h"

#include <algorithm>
#include <cmath>

namespace sl
{
    namespace
    {
        constexpr float pi = 3.14159265358979f;

        /** A frame (a, b) perpendicular to `up` with a = +Z and b = +X when up is +Y, so yaw 0 is the identity. */
        void up_basis(const glm::vec3 &up, glm::vec3 &a, glm::vec3 &b)
        {
            glm::vec3 reference{0.0f, 0.0f, 1.0f};
            if (std::abs(glm::dot(reference, up)) > 0.99f)
                reference = glm::vec3{1.0f, 0.0f, 0.0f};
            a = glm::normalize(reference - up * glm::dot(reference, up));
            b = glm::cross(up, a);
        }

        Vertex3D make_vertex(const glm::vec3 &position, const glm::vec3 &normal, Colour colour, float u = 0.0f, float v = 0.0f)
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
            vertex.uv[0] = u;
            vertex.uv[1] = v;
            return vertex;
        }

        void add_flat_triangle(MeshData &data, const glm::vec3 &p0, const glm::vec3 &p1, const glm::vec3 &p2, Colour colour)
        {
            const glm::vec3 cross = glm::cross(p1 - p0, p2 - p0);
            const glm::vec3 normal = glm::length(cross) > 1e-9f ? glm::normalize(cross) : glm::vec3{0.0f, 1.0f, 0.0f};
            const auto base = static_cast<std::uint32_t>(data.vertices.size());
            data.vertices.push_back(make_vertex(p0, normal, colour));
            data.vertices.push_back(make_vertex(p1, normal, colour));
            data.vertices.push_back(make_vertex(p2, normal, colour));
            data.indices.insert(data.indices.end(), {base, base + 1, base + 2});
        }

        /** A cone with a smooth side and no base, apex up. */
        void add_cone(MeshData &data, float y0, float height, float radius, int sides, Colour colour)
        {
            const float slope = radius / std::max(height, 1e-4f);
            for (int side = 0; side < sides; ++side)
            {
                const float a0 = 2.0f * pi * static_cast<float>(side) / static_cast<float>(sides);
                const float a1 = 2.0f * pi * static_cast<float>(side + 1) / static_cast<float>(sides);
                const float am = 0.5f * (a0 + a1);
                const auto normal = [&](float angle)
                { return glm::normalize(glm::vec3{std::sin(angle), slope, std::cos(angle)}); };
                const auto base = static_cast<std::uint32_t>(data.vertices.size());
                data.vertices.push_back(make_vertex({std::sin(a0) * radius, y0, std::cos(a0) * radius}, normal(a0), colour));
                data.vertices.push_back(make_vertex({std::sin(a1) * radius, y0, std::cos(a1) * radius}, normal(a1), colour));
                data.vertices.push_back(make_vertex({0.0f, y0 + height, 0.0f}, normal(am), colour));
                // Counter-clockwise seen from outside: apex, then the two base points in the order that faces out.
                data.indices.insert(data.indices.end(), {base + 1, base, base + 2});
            }
        }

        void add_cylinder(MeshData &data, float y0, float y1, float radius, int sides, Colour colour)
        {
            for (int side = 0; side < sides; ++side)
            {
                const float a0 = 2.0f * pi * static_cast<float>(side) / static_cast<float>(sides);
                const float a1 = 2.0f * pi * static_cast<float>(side + 1) / static_cast<float>(sides);
                const glm::vec3 n0{std::sin(a0), 0.0f, std::cos(a0)};
                const glm::vec3 n1{std::sin(a1), 0.0f, std::cos(a1)};
                const auto base = static_cast<std::uint32_t>(data.vertices.size());
                data.vertices.push_back(make_vertex(n0 * radius + glm::vec3{0.0f, y0, 0.0f}, n0, colour));
                data.vertices.push_back(make_vertex(n1 * radius + glm::vec3{0.0f, y0, 0.0f}, n1, colour));
                data.vertices.push_back(make_vertex(n1 * radius + glm::vec3{0.0f, y1, 0.0f}, n1, colour));
                data.vertices.push_back(make_vertex(n0 * radius + glm::vec3{0.0f, y1, 0.0f}, n0, colour));
                data.indices.insert(data.indices.end(), {base + 1, base, base + 3, base + 1, base + 3, base + 2});
            }
        }

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
    } // namespace

    void append_mesh_data(MeshData &into, const MeshData &part)
    {
        const auto base = static_cast<std::uint32_t>(into.vertices.size());
        into.vertices.insert(into.vertices.end(), part.vertices.begin(), part.vertices.end());
        for (const std::uint32_t index : part.indices)
            into.indices.push_back(index + base);
    }

    MeshData make_conifer_data(float height, float radius, Colour trunk, Colour foliage, int sides)
    {
        sides = std::max(sides, 3);
        MeshData data;
        add_cylinder(data, 0.0f, height * 0.3f, radius * 0.18f, std::min(sides, 6), trunk);
        add_cone(data, height * 0.18f, height * 0.5f, radius, sides, shade(foliage, 0.85f));
        add_cone(data, height * 0.45f, height * 0.55f, radius * 0.7f, sides, foliage);
        return data;
    }

    MeshData make_rock_data(float radius, Colour colour, std::uint32_t seed)
    {
        constexpr int segments = 7;
        constexpr int rings = 4;
        // A displaced, flattened half-sphere of flat-shaded facets.
        std::vector<glm::vec3> points;
        for (int ring = 0; ring <= rings; ++ring)
        {
            const float elevation = 0.5f * pi * static_cast<float>(ring) / static_cast<float>(rings);
            for (int segment = 0; segment < segments; ++segment)
            {
                const float angle = 2.0f * pi * static_cast<float>(segment) / static_cast<float>(segments);
                const float jitter = 0.78f + 0.44f * hash01(seed * 131u + static_cast<std::uint32_t>(ring * 17 + segment));
                const float r = radius * jitter;
                points.push_back({std::cos(elevation) * std::sin(angle) * r, std::sin(elevation) * r * 0.7f,
                                  std::cos(elevation) * std::cos(angle) * r});
            }
        }
        MeshData data;
        for (int ring = 0; ring < rings; ++ring)
        {
            for (int segment = 0; segment < segments; ++segment)
            {
                const int next = (segment + 1) % segments;
                const glm::vec3 &p00 = points[static_cast<std::size_t>(ring * segments + segment)];
                const glm::vec3 &p01 = points[static_cast<std::size_t>(ring * segments + next)];
                const glm::vec3 &p10 = points[static_cast<std::size_t>((ring + 1) * segments + segment)];
                const glm::vec3 &p11 = points[static_cast<std::size_t>((ring + 1) * segments + next)];
                const Colour facet = shade(colour, 0.85f + 0.3f * hash01(seed + static_cast<std::uint32_t>(ring * 31 + segment)));
                add_flat_triangle(data, p00, p01, p11, facet);
                add_flat_triangle(data, p00, p11, p10, facet);
            }
        }
        return data;
    }

    Mesh *bake_instances(const MeshData &source, const std::vector<Instance3D> &instances)
    {
        if (source.vertices.empty() || source.indices.empty() || instances.empty())
            return nullptr;
        MeshData merged;
        merged.vertices.reserve(source.vertices.size() * instances.size());
        merged.indices.reserve(source.indices.size() * instances.size());
        for (const Instance3D &instance : instances)
        {
            glm::vec3 up = instance.up;
            up = glm::length(up) > 1e-6f ? glm::normalize(up) : glm::vec3{0.0f, 1.0f, 0.0f};
            glm::vec3 a;
            glm::vec3 b;
            up_basis(up, a, b);
            const float cos_yaw = std::cos(instance.yaw);
            const float sin_yaw = std::sin(instance.yaw);
            // Local x, y, z of the object map onto b, up and a (identity for +Y up), rotated by the yaw about up.
            const glm::vec3 local_x = b * cos_yaw - a * sin_yaw;
            const glm::vec3 local_z = a * cos_yaw + b * sin_yaw;
            const auto transform = [&](const float *v)
            { return local_x * v[0] + up * v[1] + local_z * v[2]; };
            const auto base = static_cast<std::uint32_t>(merged.vertices.size());
            for (const Vertex3D &input : source.vertices)
            {
                Vertex3D vertex = input;
                const glm::vec3 position = instance.position + transform(input.position) * instance.scale;
                const glm::vec3 normal = transform(input.normal);
                const glm::vec3 tangent = transform(input.tangent);
                std::copy(&position.x, &position.x + 3, vertex.position);
                std::copy(&normal.x, &normal.x + 3, vertex.normal);
                if (input.tangent[3] != 0.0f)
                    std::copy(&tangent.x, &tangent.x + 3, vertex.tangent);
                vertex.colour[0] *= instance.tint.red / 255.0f;
                vertex.colour[1] *= instance.tint.green / 255.0f;
                vertex.colour[2] *= instance.tint.blue / 255.0f;
                vertex.colour[3] *= instance.tint.alpha / 255.0f;
                merged.vertices.push_back(vertex);
            }
            for (const std::uint32_t index : source.indices)
                merged.indices.push_back(index + base);
        }
        return create_mesh(merged.vertices, merged.indices);
    }

    Impostor *bake_impostor(const MeshData &source, const Material &material, int views, int tile_size)
    {
        views = std::max(views, 1);
        if (source.vertices.empty() || tile_size <= 0)
        {
            detail::set_error("bake_impostor needs geometry and a positive tile size");
            return nullptr;
        }
        glm::vec3 low{1e30f};
        glm::vec3 high{-1e30f};
        float horizontal = 0.0f;
        for (const Vertex3D &vertex : source.vertices)
        {
            const glm::vec3 position{vertex.position[0], vertex.position[1], vertex.position[2]};
            low = glm::min(low, position);
            high = glm::max(high, position);
            horizontal = std::max(horizontal, std::hypot(position.x, position.z));
        }
        const float height = std::max(high.y - low.y, 1e-3f);
        const float side = std::max(height, 2.0f * horizontal) * 1.06f;
        const float centre_y = 0.5f * (low.y + high.y);

        Mesh *mesh = create_mesh(source.vertices, source.indices);
        if (!mesh)
            return nullptr;
        auto *impostor = new Impostor;
        impostor->size = side;
        impostor->centre_height = centre_y;

        // The views are rendered near-orthographically: far away with a narrow field of view.
        const float distance = side * 12.0f;
        Camera camera;
        camera.field_of_view = glm::degrees(2.0f * std::atan(0.5f * side / distance));
        camera.near_plane = distance - side;
        camera.far_plane = distance + side;
        camera.target = {0.0f, centre_y, 0.0f};

        const Light3D previous_light = get_light_3d();
        const std::vector<LocalLight3D> previous_locals = get_local_lights_3d();
        float previous_bias = 0.0015f;
        const ShadowMap3D *previous_shadow = get_shadow_map_3d(&previous_bias);
        const LocalShadowMap3D *previous_local_shadow = get_local_shadow_map_3d();
        set_local_lights_3d({});
        set_shadow_map_3d(nullptr);
        set_local_shadow_map_3d(nullptr);
        Light3D bake_light;
        bake_light.direction = glm::normalize(glm::vec3{-0.45f, -0.8f, -0.4f});
        bake_light.ambient = 0.45f;
        set_light_3d(bake_light);
        Material bake_material = material;
        bake_material.fog = false;
        bake_material.alpha_cutoff = 0.0f;
        bool ok = true;
        for (int view = 0; view < views && ok; ++view)
        {
            Surface3D *surface = create_surface_3d(tile_size, tile_size, 4);
            if (!surface)
            {
                ok = false;
                break;
            }
            impostor->views.push_back(surface);
            const float azimuth = 2.0f * pi * static_cast<float>(view) / static_cast<float>(views);
            camera.position = camera.target + glm::vec3{std::sin(azimuth), 0.0f, std::cos(azimuth)} * distance;
            ok = begin_surface_3d(surface, camera);
            if (ok)
            {
                clear_surface_3d({0, 0, 0, 0});
                ok = draw_mesh(mesh, glm::mat4(1.0f), bake_material);
                end_surface_3d();
            }
        }
        set_light_3d(previous_light);
        set_local_lights_3d(previous_locals);
        set_shadow_map_3d(previous_shadow, previous_bias);
        set_local_shadow_map_3d(previous_local_shadow);
        destroy_mesh(mesh);(mesh);
        if (!ok)
        {
            destroy_impostor(impostor);
            return nullptr;
        }
        return impostor;
    }

    void destroy_impostor(Impostor *impostor)
    {
        if (!impostor)
            return;
        for (Surface3D *surface : impostor->views)
            destroy_surface_3d(surface);
        delete impostor;
    }

    int impostor_view(const Impostor &impostor, const glm::vec3 &to_viewer, const glm::vec3 &up)
    {
        const int views = static_cast<int>(impostor.views.size());
        if (views <= 1)
            return 0;
        glm::vec3 a;
        glm::vec3 b;
        up_basis(glm::length(up) > 1e-6f ? glm::normalize(up) : glm::vec3{0.0f, 1.0f, 0.0f}, a, b);
        float azimuth = std::atan2(glm::dot(to_viewer, b), glm::dot(to_viewer, a));
        if (azimuth < 0.0f)
            azimuth += 2.0f * pi;
        return static_cast<int>(std::lround(azimuth / (2.0f * pi) * static_cast<float>(views))) % views;
    }

    Mesh *bake_billboards(const Impostor &impostor, const std::vector<Instance3D> &instances, int view)
    {
        const int views = static_cast<int>(impostor.views.size());
        if (views == 0 || instances.empty() || view < 0 || view >= views)
            return nullptr;
        const float azimuth = 2.0f * pi * static_cast<float>(view) / static_cast<float>(views);
        // Render-target textures are stored bottom-up on OpenGL and top-down elsewhere.
        const bool flipped = graphics_backend() == GraphicsBackend::opengl;
        const float v_top = flipped ? 1.0f : 0.0f;
        const float v_bottom = 1.0f - v_top;
        MeshData data;
        data.vertices.reserve(instances.size() * 4);
        data.indices.reserve(instances.size() * 6);
        for (const Instance3D &instance : instances)
        {
            const glm::vec3 up = glm::length(instance.up) > 1e-6f ? glm::normalize(instance.up) : glm::vec3{0.0f, 1.0f, 0.0f};
            glm::vec3 a;
            glm::vec3 b;
            up_basis(up, a, b);
            const glm::vec3 facing = a * std::cos(azimuth) + b * std::sin(azimuth);
            const glm::vec3 right = glm::normalize(glm::cross(up, facing));
            const float half = 0.5f * impostor.size * instance.scale;
            const glm::vec3 centre = instance.position + up * (impostor.centre_height * instance.scale);
            const Colour tint = instance.tint;
            const auto base = static_cast<std::uint32_t>(data.vertices.size());
            data.vertices.push_back(make_vertex(centre - right * half - up * half, facing, tint, 0.0f, v_bottom));
            data.vertices.push_back(make_vertex(centre + right * half - up * half, facing, tint, 1.0f, v_bottom));
            data.vertices.push_back(make_vertex(centre + right * half + up * half, facing, tint, 1.0f, v_top));
            data.vertices.push_back(make_vertex(centre - right * half + up * half, facing, tint, 0.0f, v_top));
            data.indices.insert(data.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }
        return create_mesh(data.vertices, data.indices);
    }

    Material impostor_material(const Impostor &impostor, int view)
    {
        Material material;
        material.lit = false;
        material.cull = CullMode::none;
        material.alpha_cutoff = impostor.alpha_cutoff;
        const int views = static_cast<int>(impostor.views.size());
        if (views > 0)
            material.texture = surface_3d_bitmap(impostor.views[static_cast<std::size_t>(std::clamp(view, 0, views - 1))]);
        return material;
    }

} // namespace sl

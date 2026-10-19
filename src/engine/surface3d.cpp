#include "surface3d.h"

#include "error.h"
#include "renderer.h"

#include <glm/gtc/constants.hpp>

#include <cmath>
#include <memory>

namespace sl
{
    namespace
    {
        struct ActiveSurface
        {
            Surface3D *surface = nullptr;
            glm::mat4 view_projection{1.0f};
        };

        ActiveSurface active;
        Light3D current_light;

        Vertex3D make_vertex(const glm::vec3 &position, const glm::vec3 &normal, Colour colour,
                             float u = 0.0f, float v = 0.0f)
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

        /** Accumulate per-triangle UV tangents into per-vertex tangents (Gram-Schmidt against the normal). */
        void generate_tangents(std::vector<Vertex3D> &vertices, const std::vector<std::uint32_t> &indices)
        {
            std::vector<glm::vec3> tangents(vertices.size(), glm::vec3(0.0f));
            std::vector<glm::vec3> bitangents(vertices.size(), glm::vec3(0.0f));
            const auto position = [&](std::uint32_t i)
            { return glm::vec3(vertices[i].position[0], vertices[i].position[1], vertices[i].position[2]); };
            for (std::size_t t = 0; t + 2 < indices.size(); t += 3)
            {
                const std::uint32_t a = indices[t], b = indices[t + 1], c = indices[t + 2];
                const glm::vec3 e1 = position(b) - position(a);
                const glm::vec3 e2 = position(c) - position(a);
                const float du1 = vertices[b].uv[0] - vertices[a].uv[0], dv1 = vertices[b].uv[1] - vertices[a].uv[1];
                const float du2 = vertices[c].uv[0] - vertices[a].uv[0], dv2 = vertices[c].uv[1] - vertices[a].uv[1];
                const float determinant = du1 * dv2 - du2 * dv1;
                if (std::fabs(determinant) < 1e-12f)
                    continue;
                const float inverse = 1.0f / determinant;
                const glm::vec3 tangent = (e1 * dv2 - e2 * dv1) * inverse;
                const glm::vec3 bitangent = (e2 * du1 - e1 * du2) * inverse;
                for (const std::uint32_t i : {a, b, c})
                {
                    tangents[i] += tangent;
                    bitangents[i] += bitangent;
                }
            }
            for (std::size_t i = 0; i < vertices.size(); ++i)
            {
                const glm::vec3 normal(vertices[i].normal[0], vertices[i].normal[1], vertices[i].normal[2]);
                glm::vec3 tangent = tangents[i] - normal * glm::dot(normal, tangents[i]);
                if (glm::dot(tangent, tangent) < 1e-12f)
                    continue;
                tangent = glm::normalize(tangent);
                // UV v runs down the image while normal-map green points up, so the handedness is flipped.
                const float handedness = glm::dot(glm::cross(normal, tangent), bitangents[i]) < 0.0f ? 1.0f : -1.0f;
                vertices[i].tangent[0] = tangent.x;
                vertices[i].tangent[1] = tangent.y;
                vertices[i].tangent[2] = tangent.z;
                vertices[i].tangent[3] = handedness;
            }
        }

        bool has_tangents(const std::vector<Vertex3D> &vertices)
        {
            for (const Vertex3D &vertex : vertices)
                if (vertex.tangent[0] != 0.0f || vertex.tangent[1] != 0.0f || vertex.tangent[2] != 0.0f ||
                    vertex.tangent[3] != 0.0f)
                    return true;
            return false;
        }

        /** 1x1 (128,128,255) texture used when a material has no normal map; rebuilt if the renderer changes. */
        Bitmap *flat_normal_bitmap()
        {
            static std::unique_ptr<Bitmap> bitmap;
            static detail::Renderer *owner = nullptr;
            detail::Renderer *renderer = detail::active_renderer();
            if (bitmap && owner != renderer)
            {
                // The old renderer (and its textures) is gone; only free the CPU-side object.
                bitmap->gpu_texture = 0;
                bitmap.reset();
            }
            if (!bitmap)
            {
                bitmap.reset(create_bitmap(1, 1));
                if (bitmap)
                {
                    putpixel(bitmap.get(), 0, 0, Colour{128, 128, 255, 255});
                    owner = renderer;
                }
            }
            return bitmap.get();
        }
    } // namespace

    Surface3D *create_surface_3d(int width, int height, int samples)
    {
        if (width <= 0 || height <= 0)
        {
            detail::set_error("Surface dimensions must be positive");
            return nullptr;
        }
        detail::Renderer *renderer = detail::active_renderer();
        std::uint32_t texture = 0;
        std::uint32_t framebuffer = 0;
        samples = samples < 1 ? 1 : samples;
        if (!renderer || !renderer->create_render_target_3d(width, height, samples, texture, framebuffer))
        {
            detail::set_error("3D surfaces are not supported by the active graphics backend");
            return nullptr;
        }
        Bitmap *bitmap = new Bitmap;
        bitmap->width = width;
        bitmap->height = height;
        bitmap->gpu_texture = texture;
        bitmap->fbo = framebuffer;
        Surface3D *surface = new Surface3D;
        surface->bitmap = bitmap;
        surface->samples = samples;
        return surface;
    }

    int surface_3d_samples(const Surface3D *surface)
    {
        return surface ? surface->samples : 1;
    }

    void destroy_surface_3d(Surface3D *surface)
    {
        if (!surface)
            return;
        if (active.surface == surface)
            end_surface_3d();
        destroy_bitmap(surface->bitmap);
        delete surface;
    }

    Bitmap *surface_3d_bitmap(Surface3D *surface)
    {
        return surface ? surface->bitmap : nullptr;
    }

    bool begin_surface_3d(Surface3D *surface, const Camera &camera)
    {
        if (!surface || !surface->bitmap || active.surface)
        {
            detail::set_error("Invalid 3D surface state");
            return false;
        }
        if (!begin_render_target(surface->bitmap))
            return false;
        const float aspect = static_cast<float>(surface->bitmap->width) / static_cast<float>(surface->bitmap->height);
        active.surface = surface;
        active.view_projection = camera.projection_matrix(aspect) * camera.view_matrix();
        return true;
    }

    void end_surface_3d()
    {
        if (!active.surface)
            return;
        active.surface = nullptr;
        end_render_target();
    }

    void clear_surface_3d(Colour colour)
    {
        detail::Renderer *renderer = detail::active_renderer();
        if (!active.surface || !renderer)
            return;
        renderer->clear_3d(colour.red / 255.0f, colour.green / 255.0f, colour.blue / 255.0f,
                           colour.alpha / 255.0f);
    }

    void set_light_3d(const Light3D &light)
    {
        current_light = light;
    }

    Mesh *create_mesh(const std::vector<Vertex3D> &vertices, const std::vector<std::uint32_t> &indices)
    {
        detail::Renderer *renderer = detail::active_renderer();
        if (vertices.empty() || indices.empty() || indices.size() % 3 != 0)
        {
            detail::set_error("Mesh needs vertices and a whole number of triangles");
            return nullptr;
        }
        for (const std::uint32_t index : indices)
        {
            if (index >= vertices.size())
            {
                detail::set_error("Mesh index is out of range");
                return nullptr;
            }
        }
        std::vector<Vertex3D> with_tangents;
        const std::vector<Vertex3D> *source = &vertices;
        if (!has_tangents(vertices))
        {
            with_tangents = vertices;
            generate_tangents(with_tangents, indices);
            source = &with_tangents;
        }
        std::uint32_t handle = 0;
        if (!renderer || !renderer->create_mesh(source->data(), source->size(), indices.data(),
                                                indices.size(), handle))
        {
            detail::set_error("Unable to create mesh on the active graphics backend");
            return nullptr;
        }
        Mesh *mesh = new Mesh;
        mesh->handle = handle;
        mesh->vertex_count = static_cast<int>(vertices.size());
        mesh->index_count = static_cast<int>(indices.size());
        return mesh;
    }

    void destroy_mesh(Mesh *mesh)
    {
        if (!mesh)
            return;
        if (detail::Renderer *renderer = detail::active_renderer())
            renderer->destroy_mesh(mesh->handle);
        delete mesh;
    }

    Mesh *create_cube_mesh(float size, Colour colour)
    {
        const float h = size * 0.5f;
        struct Face
        {
            glm::vec3 normal, u, v;
        };
        // u x v == normal, so each face is counter-clockwise seen from outside.
        const Face faces[6] = {
            {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},
            {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}},
            {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}},
            {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
            {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}},
            {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}}};
        std::vector<Vertex3D> vertices;
        std::vector<std::uint32_t> indices;
        for (const Face &face : faces)
        {
            const std::uint32_t base = static_cast<std::uint32_t>(vertices.size());
            const glm::vec3 centre = face.normal * h;
            vertices.push_back(make_vertex(centre - (face.u + face.v) * h, face.normal, colour, 0.0f, 1.0f));
            vertices.push_back(make_vertex(centre + (face.u - face.v) * h, face.normal, colour, 1.0f, 1.0f));
            vertices.push_back(make_vertex(centre + (face.u + face.v) * h, face.normal, colour, 1.0f, 0.0f));
            vertices.push_back(make_vertex(centre + (face.v - face.u) * h, face.normal, colour, 0.0f, 0.0f));
            for (const std::uint32_t offset : {0u, 1u, 2u, 0u, 2u, 3u})
                indices.push_back(base + offset);
        }
        return create_mesh(vertices, indices);
    }

    Mesh *create_plane_mesh(float width, float depth, Colour colour)
    {
        const float hw = width * 0.5f;
        const float hd = depth * 0.5f;
        const glm::vec3 up{0.0f, 1.0f, 0.0f};
        const std::vector<Vertex3D> vertices = {
            make_vertex({-hw, 0.0f, hd}, up, colour, 0.0f, 1.0f), make_vertex({hw, 0.0f, hd}, up, colour, 1.0f, 1.0f),
            make_vertex({hw, 0.0f, -hd}, up, colour, 1.0f, 0.0f), make_vertex({-hw, 0.0f, -hd}, up, colour, 0.0f, 0.0f)};
        return create_mesh(vertices, {0, 1, 2, 0, 2, 3});
    }

    Mesh *create_sphere_mesh(float radius, int segments, int rings, Colour colour)
    {
        if (segments < 3 || rings < 2)
        {
            detail::set_error("Sphere needs at least 3 segments and 2 rings");
            return nullptr;
        }
        std::vector<Vertex3D> vertices;
        std::vector<std::uint32_t> indices;
        for (int ring = 0; ring <= rings; ++ring)
        {
            const float phi = glm::pi<float>() * static_cast<float>(ring) / static_cast<float>(rings);
            for (int segment = 0; segment <= segments; ++segment)
            {
                const float theta = 2.0f * glm::pi<float>() * static_cast<float>(segment) / static_cast<float>(segments);
                const glm::vec3 normal{std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta)};
                vertices.push_back(make_vertex(normal * radius, normal, colour,
                                                    static_cast<float>(segment) / static_cast<float>(segments),
                                                    static_cast<float>(ring) / static_cast<float>(rings)));
            }
        }
        const std::uint32_t stride = static_cast<std::uint32_t>(segments + 1);
        for (int ring = 0; ring < rings; ++ring)
        {
            for (int segment = 0; segment < segments; ++segment)
            {
                const std::uint32_t a = ring * stride + segment;
                const std::uint32_t b = a + stride;
                for (const std::uint32_t index : {a, a + 1, b, a + 1, b + 1, b})
                    indices.push_back(index);
            }
        }
        return create_mesh(vertices, indices);
    }

    bool draw_mesh(const Mesh *mesh, const glm::mat4 &model, const Material &material)
    {
        detail::Renderer *renderer = detail::active_renderer();
        if (!mesh || !renderer || !active.surface)
        {
            detail::set_error("draw_mesh requires a mesh and an active 3D surface");
            return false;
        }
        detail::MeshDrawParams params{};
        const glm::mat4 mvp = active.view_projection * model;
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                params.mvp[column * 4 + row] = mvp[column][row];
        const glm::mat3 normal_matrix = glm::transpose(glm::inverse(glm::mat3(model)));
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 3; ++column)
                params.normal_rows[row][column] = normal_matrix[column][row];
        if (material.texture)
        {
            if (!upload_bitmap(material.texture))
            {
                detail::set_error("Unable to upload mesh texture");
                return false;
            }
            params.texture = material.texture->gpu_texture;
        }
        Bitmap *normal_map = material.lit ? material.normal_map : nullptr;
        if (!normal_map)
            normal_map = flat_normal_bitmap();
        if (!normal_map || !upload_bitmap(normal_map))
        {
            detail::set_error("Unable to upload mesh normal map");
            return false;
        }
        params.normal_texture = normal_map->gpu_texture;
        params.cull = material.cull;
        params.normal_rows[0][3] = material.tint.red / 255.0f;
        params.normal_rows[1][3] = material.tint.green / 255.0f;
        params.normal_rows[2][3] = material.tint.blue / 255.0f;
        if (material.terrain.enabled)
        {
            // Terrain is only translated, so the normal matrix is not needed and its slots carry the terrain
            // parameters. A negative tint red marks the mode for the shaders.
            const TerrainShading &t = material.terrain;
            const bool atlas = material.texture && t.atlas_levels > 0;
            params.normal_rows[0][0] = atlas ? static_cast<float>(t.atlas_levels + (t.normal_atlas ? 16 : 0) + 32 * t.layer_size) : 0.0f;
            params.normal_rows[0][1] = atlas && material.texture->width > 0 ? 1.0f / material.texture->width : 0.0f;
            params.normal_rows[0][2] = atlas && material.texture->height > 0 ? 1.0f / material.texture->height : 0.0f;
            params.normal_rows[0][3] = -1.0f;
            params.normal_rows[1][0] = t.eye[0];
            params.normal_rows[1][1] = t.eye[1];
            params.normal_rows[1][2] = t.eye[2];
            params.normal_rows[1][3] = t.morph_start;
            params.normal_rows[2][0] = t.morph_end;
            params.normal_rows[2][1] = t.fade_start;
            params.normal_rows[2][2] = t.fade_end;
            params.normal_rows[2][3] = 0.0f;
        }
        if (material.lit)
        {
            const glm::vec3 to_light = -glm::normalize(current_light.direction);
            params.light[0] = to_light.x;
            params.light[1] = to_light.y;
            params.light[2] = to_light.z;
            params.light[3] = current_light.ambient;
        }
        else
        {
            params.light[3] = 1.0f;
        }
        if (!renderer->draw_mesh(mesh->handle, params))
        {
            detail::set_error("Unable to draw mesh on the active graphics backend");
            return false;
        }
        return true;
    }

} // namespace sl

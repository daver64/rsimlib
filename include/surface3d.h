#pragma once

#include "draw.h"
#include "scene3d.h"

#include <cstdint>
#include <vector>

namespace sl
{

    /** A mesh vertex: position, normal, RGBA colour (0-1 floats) and texture coordinates (v = 0 is the top of the image). */
    struct Vertex3D
    {
        float position[3] = {0.0f, 0.0f, 0.0f};
        float normal[3] = {0.0f, 1.0f, 0.0f};
        float colour[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        float uv[2] = {0.0f, 0.0f};
        /**
         * Tangent-space +U direction (xyz) and bitangent handedness (w, +1 or -1), used by normal maps.
         * Leave all zero to have create_mesh() generate tangents from the positions and UVs; a non-zero w keeps
         * the supplied tangent as is (terrain meshes store their geomorph offset here).
         */
        float tangent[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    };

    /** Indexed triangle mesh stored on the GPU. Create with create_mesh() or a primitive builder. */
    struct Mesh
    {
        std::uint32_t handle = 0;
        int vertex_count = 0;
        int index_count = 0;
    };

    /** Which triangles draw_mesh() discards. Counter-clockwise triangles are front faces. */
    enum class CullMode
    {
        back,  /**< Discard back faces (default). */
        front, /**< Discard front faces; useful for inside-out hulls and skyboxes. */
        none   /**< Draw both faces. */
    };

    /**
     * Terrain shading, used by Terrain::draw(). Meshes are expected to hold layer weights in the vertex colour
     * (r, g, b, a = layers 0-3) and a vertex position offset in the tangent xyz. The shader moves each vertex toward
     * its coarser-level position between morph_start and morph_end metres from the eye, so level changes do not pop.
     * When Material::texture is set it is a 2x2 atlas of the four layers (row major), repeated per UV unit.
     * Material::texture and normal_map hold the layer pyramids built by Terrain; the tangent slot is not a tangent.
     */
    struct TerrainShading
    {
        bool enabled = false;
        /** Eye position in the mesh's own coordinates. */
        float eye[3] = {0.0f, 0.0f, 0.0f};
        float morph_start = 0.0f;
        float morph_end = 0.0f;
        /** Beyond fade_start the layers blend to a larger-scale copy of themselves, fully by fade_end (metres). */
        float fade_start = 0.0f;
        float fade_end = 0.0f;
        /** Mip levels in the layer pyramid texture (see Terrain::set_layer_atlas); 0 means no atlas, use vertex colours. */
        int atlas_levels = 0;
        /** Pixels along one side of a level-0 layer tile. */
        int layer_size = 0;
        /** The material's normal_map holds the matching normal pyramid. */
        bool normal_atlas = false;
    };

    /** Surface appearance for draw_mesh(). The tint and texture multiply the mesh vertex colours. */
    struct Material
    {
        Colour tint{255, 255, 255, 255};
        /** When false the mesh is drawn with its colours unchanged, ignoring the light. */
        bool lit = true;
        /** Optional texture sampled with the mesh UVs and multiplied into the colour. */
        Bitmap *texture = nullptr;
        /**
         * Optional tangent-space normal map (RGB = XYZ, +Y up as in glTF) sampled with the mesh UVs.
         * Needs mesh UVs; tangents are generated automatically. Not owned. Only applies to lit materials.
         */
        Bitmap *normal_map = nullptr;
        CullMode cull = CullMode::back;
        /** Apply the environment's fog and haze (see set_environment_3d()). Turn off for sky domes and emissive effects. */
        bool fog = true;
        /**
         * Discard pixels whose texture alpha (times the vertex alpha) is below this, 0-1; 0 draws everything.
         * Gives hard-edged cut-outs for foliage cards and impostors without sorting. Not applied to terrain.
         */
        float alpha_cutoff = 0.0f;
        TerrainShading terrain;
    };

    /** A single directional light plus ambient term, used by lit materials. */
    struct Light3D
    {
        /** Direction the light travels (from the light toward the scene). */
        glm::vec3 direction{-0.4f, -1.0f, -0.6f};
        /** Minimum brightness of lit surfaces, 0-1. */
        float ambient = 0.25f;
        /** Colour of the directional light; atmosphere_sun_colour() gives a sunset-aware value. */
        glm::vec3 colour{1.0f, 1.0f, 1.0f};
    };

    /**
     * An offscreen 3D drawing target with a depth buffer. Draw into it between
     * begin_surface_3d() and end_surface_3d(), then composite surface_3d_bitmap()
     * into 2D drawing with draw_sprite().
     * Supported on the OpenGL and Vulkan backends; creation fails with an error on others.
     */
    struct Surface3D
    {
        Bitmap *bitmap = nullptr;
        int samples = 1;
    };

    /**
     * Create a surface, or nullptr (see last_error()) if unsupported or allocation fails.
     * samples requests multisample anti-aliasing (e.g. 4); it is clamped to what the GPU supports.
     */
    Surface3D *create_surface_3d(int width, int height, int samples = 1);
    /** The sample count actually in use (1 when MSAA is off or unsupported). */
    int surface_3d_samples(const Surface3D *surface);
    void destroy_surface_3d(Surface3D *surface);
    /** The surface's colour image, for use with draw_sprite() and similar 2D calls. */
    Bitmap *surface_3d_bitmap(Surface3D *surface);

    /** Direct subsequent 3D drawing to the surface using the camera. Contents are undefined until clear_surface_3d(). */
    bool begin_surface_3d(Surface3D *surface, const Camera &camera);
    void end_surface_3d();
    /** Clear the active surface's colour and depth. */
    void clear_surface_3d(Colour colour);
    /** Set the light used by lit materials on subsequent draws. */
    void set_light_3d(const Light3D &light);
    /** The light most recently passed to set_light_3d(). */
    Light3D get_light_3d();

    /** Create a mesh from indexed triangles; returns nullptr on failure. */
    Mesh *create_mesh(const std::vector<Vertex3D> &vertices, const std::vector<std::uint32_t> &indices);
    void destroy_mesh(Mesh *mesh);
    /** Primitive builders. All meshes are centred on the origin with outward-facing normals. */
    Mesh *create_cube_mesh(float size = 1.0f, Colour colour = {255, 255, 255, 255});
    Mesh *create_plane_mesh(float width, float depth, Colour colour = {255, 255, 255, 255});
    Mesh *create_sphere_mesh(float radius, int segments = 24, int rings = 16,
                             Colour colour = {255, 255, 255, 255});
    /** Draw a mesh into the active surface. */
    bool draw_mesh(const Mesh *mesh, const glm::mat4 &model, const Material &material = {});

} // namespace sl

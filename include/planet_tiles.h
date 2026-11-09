#pragma once

#include "async_load.h"
#include "draw.h"
#include "surface3d.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace sl
{

    /** Settings for PlanetTiles::create(). */
    struct PlanetTilesOptions
    {
        /**
         * The colour map: a Celestia virtual texture (.ctx file) or the folder holding its `levelN` folders. Each level
         * folder holds `tx_<column>_<row>.<ext>` tiles (dds, jpg or png). Level 0 is 2 x 1 tiles (more with BaseSplit),
         * every level doubles both directions, column 0 starts at longitude -180 and row 0 is the north.
         */
        std::string colour;
        /**
         * Alternative colour tile provider for archives. The level layout is the same as a Celestia pyramid.
         * Both callbacks must be thread-safe; load_colour runs on a worker and returns a RAM-only bitmap owned by
         * PlanetTiles, or nullptr with last_error() set. The minimum level must cover the entire sphere.
         */
        std::function<bool(int level, int column, int row)> has_colour;
        /**
         * Optional sparse-archive traversal: true when a tile has child nodes, including when it has no colour data.
         * Called on the main thread. The renderer retains the nearest resident ancestor while traversing empty nodes.
         */
        std::function<bool(int level, int column, int row)> has_colour_children;
        std::function<Bitmap *(int level, int column, int row)> load_colour;
        int colour_min_level = 0;
        int colour_max_level = 0;
        int colour_base_split = 0;
        /**
         * Optional gloss map in the same layout (white is shiny, as in Celestia's SpecularTexture), drawn as a
         * sun-glint overlay. May stop at a shallower level than the colour map.
         */
        std::string specular;
        /**
         * Optional tangent-space normal map in the same layout. A patch only uses it when the normal tile exists at the
         * same level as the colour tile it draws, so it fades out where the colour map is deeper.
         */
        std::string normal;
        /** Quads along each side of a tile's patch of sphere. */
        int segments = 12;
        /**
         * Optional radial elevation in units of the planet radius, sampled in the tile's body-fixed unit-sphere frame.
         * The callback runs on worker threads when a patch mesh is built; it must be thread-safe and return finite values
         * within +-max_elevation. The coarsest meshes are built before create() returns.
         * Elevation at a position must not depend on tile level, so adjacent patches agree at shared vertices.
         * Not supported with limb_fade.
         */
        std::function<double(const glm::dvec3 &direction)> elevation;
        /**
         * Optional LOD-aware elevation for patch positions. The second argument is the approximate mesh-cell width
         * in units of planet radius. Unless adaptive_elevation_normals is enabled, normals use elevation at fixed
         * sampling detail so shared vertices have consistent lighting. Requires a positive normal_sample_spacing and
         * match_elevation_edges to reconcile heights across different mesh levels.
         */
        std::function<double(const glm::dvec3 &direction, double cell_size)> elevation_lod;
        /** Choose each elevated cell's diagonal from neighbouring heights, as in Orbiter's surface mesh. */
        bool adaptive_elevation_diagonals = false;
        /** Derive relief normals from the neighbouring mesh-level elevations, as in Orbiter. Requires adaptive diagonals. */
        bool adaptive_elevation_normals = false;
        /** Match elevated patch boundaries to the drawn coarser neighbours; requires fading and feathering disabled. */
        bool match_elevation_edges = false;
        /** Maximum absolute elevation in units of the planet radius; used for conservative visibility bounds. */
        double max_elevation = 0.0;
        /**
         * Tangent-space offset on the unit sphere for relief normals (approximately radians for small values). A fixed
         * positive offset makes normals agree at vertices shared by different levels; zero uses the mesh grid spacing.
         */
        double normal_sample_spacing = 0.0;
        /** Refine until one texel covers at most this many screen pixels (1 is full detail, 2 halves the resolution). */
        float texel_pixels = 1.0f;
        /**
         * When positive, also refine elevated geometry until a mesh cell covers at most this many screen pixels,
         * independently of texture availability. Missing imagery uses the nearest resident ancestor. When a skirt is
         * enabled, geometry may also refine while a requested texture is loading or deferred by a resource limit;
         * without skirts, the parent stays visible until requested child imagery is ready. Skirt-free mixed-LOD
         * transitions may still expose rasterization gaps. Zero disables geometry-based refinement.
         */
        float geometry_pixels = 0.0f;
        /** Deepest geometry level, or -1 to use the deepest colour level. The max_level cap also applies. */
        int geometry_max_level = -1;
        /** Deepest level to use; -1 for the deepest one present. */
        int max_level = -1;
        /** Eviction target for tile textures; active tiles and the coarsest level are retained. */
        int max_cached_tiles = 512;
        /** Hard limit on resident textures plus outstanding texture loads, across all layers; zero is unlimited.
         *  This limits the number of textures, not their dimensions or memory in bytes. */
        int max_resident_tiles = 0;
        /** Maximum remembered failures plus outstanding loads; zero leaves failures unlimited.
         *  New loads wait when failures in view fill the limit; old failures can be retried after leaving view. */
        int max_failed_tiles = 0;
        /** Maximum cached patch meshes before unused entries are evicted. Active ancestors and coarsest meshes are retained. */
        int max_cached_meshes = 1200;
        /** Hard limit on relief mesh entries, including queued jobs; zero leaves the cache without a hard cap. */
        int max_relief_meshes = 0;
        /** Limit the number of visible relief patches selected; zero is unlimited. */
        int max_relief_patches = 0;
        /** Maximum outstanding relief mesh jobs, including those awaiting upload; zero is unlimited. */
        int max_relief_mesh_jobs = 0;
        /** Most tile loads running at once; more are queued by the next update(). */
        int max_loads_in_flight = 6;
        /** Pixel width of a tile when the descriptor does not say (it is read from the first tile). 0 to detect. */
        int tile_size = 0;
        /** Texture filtering for the tiles. */
        float anisotropy = 4.0f;
        /** Brightness of the specular overlay, 0-1. */
        float specular_strength = 1.0f;
        /** Roughness of the shiniest places, so a white texel is not a mirror. 0.04-1. */
        float specular_min_roughness = 0.15f;
        /** How far, in tile widths, the skirt around each patch dips to hide cracks between levels of detail.
         *  Must be finite and non-negative; zero disables skirts. */
        float skirt = 0.05f;
        /**
         * Where a regional high-resolution patch ends, the sharp tile fades out over this fraction of its width onto the
         * coarser tile beneath, instead of stopping at a hard edge. 0 turns it off.
         */
        float feather = 0.3f;
        /** Seconds a tile takes to fade in over its parent when it finishes loading, so detail does not pop. 0 turns it off. */
        float fade_seconds = 0.4f;
        /**
         * For a translucent layer such as clouds: patches fade out where the view grazes the sphere, so a thin shell seen
         * edge-on near the horizon does not smear into streaks. The value is the cosine of the angle between the line of
         * sight and the surface normal at which the layer is fully opaque (it is clear at 0.3 of that). 0 turns it off.
         * The fade is rewritten into the patch vertices each draw, so it costs some upload bandwidth near the limb.
         */
        float limb_fade = 0.0f;
    };

    /** What PlanetTiles is doing, for overlays and tests. */
    struct PlanetTilesStats
    {
        int patches = 0;
        /** Deepest colour tile currently drawn. */
        int deepest_level = 0;
        /** Deepest visible geometry patch, which may exceed the colour level. */
        int deepest_geometry_level = 0;
        /** Visible patches sampling an ancestor colour tile and the largest number of levels skipped. */
        int patches_using_parent = 0;
        int max_colour_levels_up = 0;
        /** Visible patches whose conservative texel-size estimate exceeds the configured screen-space target. */
        int undersampled_patches = 0;
        int tiles_loaded = 0;
        int tiles_loading = 0;
        /** Failed texture requests currently remembered to avoid retrying them every frame. */
        int tiles_failed = 0;
        /** Whether a texture request was deferred by the resident/load limit in the last update. */
        bool texture_limited = false;
        /** Whether a texture request was deferred because the failure bookkeeping limit was full. */
        bool failure_limited = false;
        int meshes = 0;
        /** Outstanding relief mesh jobs, including those awaiting upload. */
        int mesh_jobs = 0;
        /** Number of visible fine edges matched to a coarser neighbour in the latest update. */
        int matched_edges = 0;
        /** True if a configured relief resource limit prevented further refinement. */
        bool relief_limited = false;
    };

    /**
     * A planet drawn from a pyramid of texture tiles that are streamed in as the camera approaches, so a 64k x 32k map
     * needs only the tiles that are in view.
     *
     * Each frame call update() with the camera, then draw(). The sphere is split into patches by a quadtree: a tile is
     * refined into its four children while its texels are bigger than the screen pixels, and its children are only
     * refined once the tile itself is resident. Tiles load on worker threads (see load_bitmap_async()); until a tile
     * arrives its patch draws the part of the next shallower tile that covers it. Elevated parents are retained until
     * their visible children are ready; invisible children are requested when they enter view. The pyramid may be sparse:
     * where a level has no tile (for example a regional high-resolution patch), the patch keeps using the coarser tile.
     *
     * The unit sphere has its poles on +-Y and longitude -180 at +X. East (+Z at longitude 0, which faces -X) is to the
     * right when seen from outside, which is the reverse of create_sphere_mesh()'s texture direction, so a mesh that
     * replaces a sphere needs its u coordinate mirrored. All of it belongs to the main thread.
     */
    class PlanetTiles
    {
    public:
        /** Open the tile sets and load the coarsest level. Returns nullptr on failure (see last_error()). */
        static std::unique_ptr<PlanetTiles> create(const PlanetTilesOptions &options);
        ~PlanetTiles();
        PlanetTiles(const PlanetTiles &) = delete;
        PlanetTiles &operator=(const PlanetTiles &) = delete;

        /**
         * Choose the patches and queue tile loads. `eye` is the camera in the unit sphere's frame (the planet radius is
         * 1). `fov_y` is the vertical field of view in radians and `viewport_height` the height of the view in pixels.
         * `view_projection`, when given, maps that frame to clip space (projection * view * model) and lets patches
         * outside the view be skipped.
         */
        void update(const glm::dvec3 &eye, double fov_y, int viewport_height, const glm::mat4 *view_projection = nullptr);

        /**
         * Like update(), with a double-precision view-projection matrix. This avoids losing frustum precision when
         * a planet-sized model is close to the camera. The matrix maps unit-sphere coordinates to clip space.
         */
        void update_relative(const glm::dvec3 &eye, double fov_y, int viewport_height, const glm::dmat4 *view_projection = nullptr);

        /**
         * Draw the patches chosen by update() with the given model matrix (which should scale the unit sphere to the
         * planet radius). `material` supplies the lighting, fog and tint; its texture and normal map are replaced.
         */
        void draw(const glm::mat4 &model, const Material &material = {});

        /**
         * Draw with a double-precision model matrix. Relief patch vertices are relative to their patch centre, and the
         * centre is transformed before conversion to float, preserving precision when the model includes a floating origin.
         */
        void draw_relative(const glm::dmat4 &model, const Material &material = {});

        /**
         * Sample the currently selected elevated mesh along a body-fixed radial direction. Returns the height in
         * planet-radius units, or false with last_error() set if that direction is not covered by a ready mesh.
         * Requires match_elevation_edges and disabled tile fading/feathering. Call after update_relative().
         */
        bool drawn_elevation(const glm::dvec3 &direction, double &height) const;

        /** Deepest level the colour map has. */
        int deepest_level() const;
        /** Width in pixels of one tile. */
        int tile_size() const;
        /** True when a specular map was opened. */
        bool has_specular() const;
        /** Changes the brightness of the specular overlay, 0-1, from the next draw (so it can fade with altitude). */
        void set_specular_strength(float strength);
        PlanetTilesStats stats() const;

        /**
         * Numbering helpers, public for tools and tests: how many tile columns and rows a level has, and the
         * UV rectangle (u0, v0, u1, v1) of tile (column, row) of `level` within its ancestor `levels_up` levels shallower.
         */
        static int columns(int level, int base_split = 0) { return 2 << (level + base_split); }
        static int rows(int level, int base_split = 0) { return 1 << (level + base_split); }
        static void ancestor_uv(int column, int row, int levels_up, float rect[4]);

    private:
        PlanetTiles();
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace sl

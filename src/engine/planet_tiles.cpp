#include "planet_tiles.h"

#include "error.h"
#include "planet_tile_edges.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sl
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr int colour_layer = 0;
        constexpr int specular_layer = 1;
        constexpr int normal_layer = 2;
        constexpr int layer_count = 3;

        std::uint64_t tile_key(int level, int column, int row)
        {
            return (static_cast<std::uint64_t>(level) << 56) | (static_cast<std::uint64_t>(column) << 28) | static_cast<std::uint64_t>(row);
        }

        std::string lower(std::string text)
        {
            for (char &c : text)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return text;
        }

        /** The tiles one map has on disk: which `levelN/tx_<column>_<row>` files exist. */
        struct TileSet
        {
            std::string root;
            std::string extension;
            int base_split = 0;
            int min_level = 0;
            int max_level = -1;
            std::vector<std::unordered_set<std::uint64_t>> present;

            bool valid() const { return max_level >= min_level; }
            bool has(int level, int column, int row) const
            {
                return level >= min_level && level <= max_level && present[static_cast<std::size_t>(level)].count(tile_key(level, column, row)) > 0;
            }
            std::string path(int level, int column, int row) const
            {
                char name[64];
                std::snprintf(name, sizeof(name), "tx_%d_%d.", column, row);
                return (fs::path(root) / ("level" + std::to_string(level)) / (std::string(name) + extension)).string();
            }
        };

        bool parse_ctx(const std::string &path, std::string &directory, int &split, std::string &type)
        {
            std::ifstream in(path);
            if (!in)
                return false;
            std::stringstream buffer;
            buffer << in.rdbuf();
            const std::string text = buffer.str();
            auto word_after = [&](const std::string &key) -> std::string
            {
                const std::size_t at = text.find(key);
                if (at == std::string::npos)
                    return {};
                std::size_t i = at + key.size();
                while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])))
                    ++i;
                if (i < text.size() && text[i] == '"')
                {
                    const std::size_t end = text.find('"', i + 1);
                    return end == std::string::npos ? std::string() : text.substr(i + 1, end - i - 1);
                }
                std::size_t end = i;
                while (end < text.size() && !std::isspace(static_cast<unsigned char>(text[end])) && text[end] != '}')
                    ++end;
                return text.substr(i, end - i);
            };
            directory = word_after("ImageDirectory");
            const std::string base = word_after("BaseSplit");
            split = base.empty() ? 0 : std::max(0, std::atoi(base.c_str()));
            type = lower(word_after("TileType"));
            return true;
        }

        bool open_tile_set(const std::string &source, TileSet &set, std::string &why)
        {
            std::string folder = source, type;
            if (lower(fs::path(source).extension().string()) == ".ctx")
            {
                std::string directory;
                if (!parse_ctx(source, directory, set.base_split, type))
                {
                    why = "cannot read " + source;
                    return false;
                }
                const fs::path beside = fs::path(source).parent_path();
                folder = (beside / directory).string();
                // Some packs name a folder that was renamed; the level folders then sit beside the descriptor.
                if (directory.empty() || !fs::is_directory(folder))
                    folder = beside.string();
            }
            std::error_code ec;
            if (!fs::is_directory(folder, ec))
            {
                why = "no tile folder at " + folder;
                return false;
            }
            set.root = folder;
            for (const fs::directory_entry &level_dir : fs::directory_iterator(folder, ec))
            {
                const std::string name = level_dir.path().filename().string();
                if (!level_dir.is_directory() || name.rfind("level", 0) != 0 || name.size() < 6)
                    continue;
                const int level = std::atoi(name.c_str() + 5);
                if (level < 0 || level > 40)
                    continue;
                if (static_cast<int>(set.present.size()) <= level)
                    set.present.resize(static_cast<std::size_t>(level) + 1);
                for (const fs::directory_entry &file : fs::directory_iterator(level_dir.path(), ec))
                {
                    const std::string file_name = file.path().filename().string();
                    int column = 0, row = 0;
                    if (std::sscanf(file_name.c_str(), "tx_%d_%d.", &column, &row) != 2 || column < 0 || row < 0)
                        continue;
                    const std::string extension = lower(file.path().extension().string());
                    if (extension.size() < 2)
                        continue;
                    if (set.extension.empty())
                        set.extension = file.path().extension().string().substr(1);
                    else if (lower(set.extension) != extension.substr(1))
                        continue;
                    set.present[static_cast<std::size_t>(level)].insert(tile_key(level, column, row));
                    set.max_level = std::max(set.max_level, level);
                }
            }
            // The coarsest level that has tiles.
            set.min_level = 0;
            while (set.min_level <= set.max_level && set.present[static_cast<std::size_t>(set.min_level)].empty())
                ++set.min_level;
            if (!type.empty() && set.extension.empty())
                set.extension = type;
            if (!set.valid())
            {
                why = "no tiles found under " + folder;
                return false;
            }
            return true;
        }

        struct Frustum
        {
            glm::dvec4 planes[4];
            bool enabled = false;
        };

        Frustum make_frustum(const glm::dmat4 &m)
        {
            auto row = [&](int i) { return glm::dvec4(m[0][i], m[1][i], m[2][i], m[3][i]); };
            Frustum f;
            f.planes[0] = row(3) + row(0);
            f.planes[1] = row(3) - row(0);
            f.planes[2] = row(3) + row(1);
            f.planes[3] = row(3) - row(1);
            f.enabled = true;
            return f;
        }

        // The position on the unit sphere for tile-space coordinates, using create_sphere_mesh()'s axes.
        glm::dvec3 sphere_point(double u, double v)
        {
            return detail::planet_tile_sphere_point(u, v);
        }
    } // namespace

    void PlanetTiles::ancestor_uv(int column, int row, int levels_up, float rect[4])
    {
        const int span = 1 << std::max(levels_up, 0);
        const float size = 1.0f / static_cast<float>(span);
        const float u0 = static_cast<float>(column & (span - 1)) * size, v0 = static_cast<float>(row & (span - 1)) * size;
        rect[0] = u0;
        rect[1] = v0;
        rect[2] = u0 + size;
        rect[3] = v0 + size;
    }

    namespace
    {
        double now_seconds()
        {
            return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
    } // namespace

    struct PlanetTiles::Impl
    {
        struct Tile
        {
            Bitmap *bitmap = nullptr;
            AsyncBitmap load;
            bool loading = false;
            bool failed = false;
            std::string description;
            std::string error;
            std::uint64_t used = 0;
            double arrived = -1.0e9; // seconds on now_seconds()'s clock when the bitmap became resident
        };

        struct MeshEntry
        {
            Mesh *mesh = nullptr;
            AsyncMesh load;
            bool failed = false;
            std::string error;
            std::uint64_t used = 0;
            std::vector<Vertex3D> vertices; // retained for limb fade or matched relief edges
            std::shared_ptr<std::vector<Vertex3D>> original_vertices;
            std::shared_ptr<std::vector<std::uint8_t>> diagonals;
            std::vector<float> base_alpha;
            bool faded = false;
            bool stitched = false;
            bool indices_only = false;
            detail::PlanetTileEdgeUpload edge_upload;
        };

        struct MeshKey
        {
            std::uint64_t tile;
            int up, layer, edges;

            bool operator==(const MeshKey &other) const
            {
                return tile == other.tile && up == other.up && layer == other.layer && edges == other.edges;
            }
        };

        struct MeshKeyHash
        {
            std::size_t operator()(const MeshKey &key) const
            {
                std::size_t value = std::hash<std::uint64_t>{}(key.tile);
                for (int part : {key.up, key.layer, key.edges})
                    value ^= std::hash<int>{}(part) + 0x9e3779b9u + (value << 6) + (value >> 2);
                return value;
            }
        };

        struct Leaf
        {
            int level, column, row;
            int up;      // levels up to the colour tile that is drawn
            int spec_up; // levels up to the specular tile, or -1
            Bitmap *colour = nullptr, *normal = nullptr, *specular = nullptr;
            Bitmap *under = nullptr; // the parent tile drawn beneath a leaf that is fading in or feathered at its edges
            float fade = 1.0f;       // opacity while the tile fades in over its parent
            int edges = 0;           // bits 0-3: sides of the patch (i = 0, i = n, j = 0, j = n) with no neighbour at this level
        };

        PlanetTilesOptions options;
        TileSet sets[layer_count];
        std::unordered_map<std::uint64_t, Tile> tiles[layer_count];
        std::unordered_map<MeshKey, MeshEntry, MeshKeyHash> meshes;
        std::vector<Leaf> leaves;
        std::uint64_t frame = 1;
        int tile_px = 512;
        int max_level = 0;
        int geometry_max_level = 0;
        int in_flight = 0;
        int matched_edges = 0;
        bool edge_match_failed = false;
        int relief_patch_budget_used = 0;
        bool relief_limited = false;
        bool relief_limit_reported = false;
        bool texture_limited = false;
        bool texture_limit_reported = false;
        bool failure_limited = false;
        bool failure_limit_reported = false;

        // Per-update camera state.
        glm::dvec3 eye{0.0};
        double eye_distance = 0.0;
        double pixel_per_unit = 1.0; // pixels per world unit at distance 1
        Frustum frustum;

        void report_relief_limit()
        {
            relief_limited = true;
            if (!relief_limit_reported)
            {
                std::fprintf(stderr, "[simlib] PlanetTiles: relief refinement limited by the configured patch or mesh budget\n");
                relief_limit_reported = true;
            }
        }

        ~Impl()
        {
            for (auto &entry : meshes)
                destroy_mesh(entry.second.mesh);
            for (auto &map : tiles)
                for (auto &entry : map)
                    if (entry.second.bitmap)
                        destroy_bitmap(entry.second.bitmap);
        }

        AsyncBitmapOptions load_options(int layer) const
        {
            AsyncBitmapOptions o;
            o.smooth = true;
            o.mipmaps = true;
            o.anisotropy = options.anisotropy;
            if (layer == specular_layer)
            {
                o.invert = true;
                o.invert_floor = static_cast<int>(std::clamp(options.specular_min_roughness, 0.04f, 1.0f) * 255.0f);
            }
            return o;
        }

        bool usable(int layer, int level, int column, int row)
        {
            const bool available = layer == colour_layer && options.has_colour
                ? level >= sets[layer].min_level && level <= sets[layer].max_level && options.has_colour(level, column, row)
                : sets[layer].has(level, column, row);
            if (!available)
                return false;
            const auto found = tiles[layer].find(tile_key(level, column, row));
            if (found != tiles[layer].end() && found->second.failed)
            {
                found->second.used = frame;
                return false;
            }
            return true;
        }

        bool colour_children(int level, int column, int row) const
        {
            return options.has_colour_children && level < max_level && options.has_colour_children(level, column, row);
        }

        Tile *resident(int layer, int level, int column, int row)
        {
            const auto found = tiles[layer].find(tile_key(level, column, row));
            return found != tiles[layer].end() && found->second.bitmap ? &found->second : nullptr;
        }

        void request(int layer, int level, int column, int row, bool wait)
        {
            if (!usable(layer, level, column, row))
                return;
            const std::uint64_t key = tile_key(level, column, row);
            const auto existing = tiles[layer].find(key);
            if (existing != tiles[layer].end() && (existing->second.bitmap || existing->second.loading || existing->second.failed))
                return;
            if (!wait && in_flight >= options.max_loads_in_flight)
                return;
            if (options.max_resident_tiles > 0)
            {
                int occupied = in_flight;
                for (const auto &map : tiles)
                    for (const auto &entry : map)
                        occupied += entry.second.bitmap ? 1 : 0;
                if (occupied >= options.max_resident_tiles)
                {
                    texture_limited = true;
                    if (!texture_limit_reported)
                    {
                        std::fprintf(stderr, "[simlib] PlanetTiles: texture requests limited by the configured resident tile budget\n");
                        texture_limit_reported = true;
                    }
                    return;
                }
            }
            if (options.max_failed_tiles > 0)
            {
                int occupied = in_flight;
                int oldest_layer = -1;
                std::uint64_t oldest_key = 0, oldest_frame = frame;
                for (int l = 0; l < layer_count; ++l)
                    for (const auto &entry : tiles[l])
                        if (entry.second.failed)
                        {
                            ++occupied;
                            if (frame > 3 && entry.second.used < frame - 3 && entry.second.used < oldest_frame)
                            {
                                oldest_layer = l;
                                oldest_key = entry.first;
                                oldest_frame = entry.second.used;
                            }
                        }
                if (occupied >= options.max_failed_tiles)
                {
                    if (oldest_layer >= 0)
                        tiles[oldest_layer].erase(oldest_key);
                    else
                    {
                        failure_limited = true;
                        if (!failure_limit_reported)
                        {
                            std::fprintf(stderr, "[simlib] PlanetTiles: texture requests limited by the configured failure budget\n");
                            failure_limit_reported = true;
                        }
                        return;
                    }
                }
            }
            Tile &tile = tiles[layer][key];
            tile.description = layer == colour_layer && options.load_colour
                ? "colour tile " + std::to_string(level) + "/" + std::to_string(column) + "/" + std::to_string(row)
                : sets[layer].path(level, column, row);
            if (layer == colour_layer && options.load_colour)
            {
                const auto loader = options.load_colour;
                tile.load = load_bitmap_async([loader, level, column, row] { return loader(level, column, row); }, load_options(layer));
            }
            else
                tile.load = load_bitmap_async(sets[layer].path(level, column, row), load_options(layer));
            tile.loading = true;
            ++in_flight;
            if (wait)
                poll(tile, true);
        }

        void poll(Tile &tile, bool wait = false)
        {
            if (!tile.loading)
                return;
            if (wait)
                tile.load.wait();
            if (!tile.load.done())
                return;
            tile.loading = false;
            --in_flight;
            tile.bitmap = tile.load.ready() ? tile.load.get() : nullptr;
            tile.failed = tile.bitmap == nullptr;
            if (tile.failed)
            {
                tile.used = frame;
                tile.error = tile.load.error();
                if (tile.error.empty())
                    tile.error = "no bitmap returned";
                std::fprintf(stderr, "[simlib] PlanetTiles: cannot load %s: %s\n", tile.description.c_str(), tile.error.c_str());
            }
            tile.arrived = wait ? -1.0e9 : now_seconds();
            tile.load = AsyncBitmap();
        }

        void poll_all()
        {
            for (auto &map : tiles)
                for (auto &entry : map)
                    poll(entry.second);
        }

        void trim_failed_tiles()
        {
            if (options.max_failed_tiles == 0)
                return;
            struct Failure
            {
                std::uint64_t used, key;
                int layer;
            };
            std::vector<Failure> failures;
            for (int layer = 0; layer < layer_count; ++layer)
                for (const auto &entry : tiles[layer])
                    if (entry.second.failed)
                        failures.push_back({entry.second.used, entry.first, layer});
            const std::size_t limit = static_cast<std::size_t>(options.max_failed_tiles);
            if (failures.size() <= limit)
                return;
            std::sort(failures.begin(), failures.end(), [](const Failure &a, const Failure &b)
            {
                return a.used < b.used;
            });
            for (std::size_t i = 0; i < failures.size() - limit; ++i)
                tiles[failures[i].layer].erase(failures[i].key);
        }

        // Bounding sphere (centre direction and radius) of a tile's patch of unit sphere.
        void bounds(int level, int column, int row, glm::dvec3 &centre, double &radius) const
        {
            const double cols = columns(level, sets[colour_layer].base_split), rws = rows(level, sets[colour_layer].base_split);
            glm::dvec3 points[9];
            for (int j = 0; j < 3; ++j)
                for (int i = 0; i < 3; ++i)
                {
                    points[j * 3 + i] = sphere_point((column + 0.5 * i) / cols, (row + 0.5 * j) / rws);
                }
            centre = points[4];
            radius = 0.0;
            for (const glm::dvec3 &p : points)
                radius = std::max(radius, glm::length(p - centre));
            radius += options.max_elevation;
        }

        bool visible(const glm::dvec3 &centre, double radius) const
        {
            const double inner_radius = 1.0 - options.max_elevation;
            const double outer_radius = 1.0 + options.max_elevation;
            if (eye_distance > inner_radius)
            {
                // Only the inner sphere is guaranteed to occlude terrain; peaks can rise above its horizon.
                const double angle = std::acos(std::clamp(glm::dot(centre, eye / eye_distance), -1.0, 1.0));
                const double horizon = std::acos(inner_radius / eye_distance) + std::acos(inner_radius / outer_radius);
                if (angle > horizon + 2.0 * std::asin(std::min(radius * 0.5, 1.0)))
                    return false;
            }
            if (frustum.enabled)
                for (const glm::dvec4 &plane : frustum.planes)
                {
                    const double distance = plane.x * centre.x + plane.y * centre.y + plane.z * centre.z + plane.w;
                    if (distance < -radius * glm::length(glm::dvec3(plane.x, plane.y, plane.z)))
                        return false;
                }
            return true;
        }

        bool wants_detail(int level, const glm::dvec3 &centre, double radius) const
        {
            const double distance = std::max(glm::length(eye - centre) - radius, 1e-9);
            const double texel = (glm::pi<double>() / rows(level, sets[colour_layer].base_split)) / tile_px;
            return texel * pixel_per_unit / distance > options.texel_pixels;
        }

        bool wants_geometry(int level, const glm::dvec3 &centre, double radius) const
        {
            if (!options.elevation || options.geometry_pixels <= 0.0f || level >= geometry_max_level)
                return false;
            const double distance = std::max(glm::length(eye - centre) - radius, 1e-9);
            const double cell = glm::pi<double>() / (rows(level, sets[colour_layer].base_split) * std::max(options.segments, 2));
            return cell * pixel_per_unit / distance > options.geometry_pixels;
        }

        void emit(int level, int column, int row, int up)
        {
            Leaf leaf{level, column, row, up, -1};
            const int cl = level - up, cc = column >> up, cr = row >> up;
            Tile *colour = resident(colour_layer, cl, cc, cr);
            if (!colour)
                return;
            colour->used = frame;
            leaf.colour = colour->bitmap;
            if (Tile *normal = resident(normal_layer, cl, cc, cr))
            {
                normal->used = frame;
                leaf.normal = normal->bitmap;
            }
            if (sets[specular_layer].valid())
                for (int l = level; l >= sets[specular_layer].min_level; --l)
                {
                    const int shift = level - l;
                    if (Tile *spec = resident(specular_layer, l, column >> shift, row >> shift))
                    {
                        spec->used = frame;
                        leaf.specular = spec->bitmap;
                        leaf.spec_up = shift;
                        break;
                    }
                }
            if (up == 0 && level > sets[colour_layer].min_level)
            {
                Tile *parent = resident(colour_layer, level - 1, column >> 1, row >> 1);
                if (parent)
                {
                    const double age = now_seconds() - colour->arrived;
                    if (options.fade_seconds > 0.0f && age < options.fade_seconds)
                        leaf.fade = static_cast<float>(std::max(age, 0.0) / options.fade_seconds);
                    if (options.feather > 0.0f)
                    {
                        const int cols = columns(level, sets[colour_layer].base_split), rws = rows(level, sets[colour_layer].base_split);
                        auto missing = [&](int c, int r)
                        {
                            if (r < 0 || r >= rws)
                                return false;
                            return !usable(colour_layer, level, (c + cols) % cols, r);
                        };
                        leaf.edges = (missing(column - 1, row) ? 1 : 0) | (missing(column + 1, row) ? 2 : 0) | (missing(column, row - 1) ? 4 : 0) |
                                     (missing(column, row + 1) ? 8 : 0);
                    }
                    if (leaf.fade < 1.0f || leaf.edges)
                    {
                        parent->used = frame;
                        leaf.under = parent->bitmap;
                    }
                }
            }
            leaves.push_back(leaf);
        }

        void visit(int level, int column, int row, bool exists, int up = 0, bool colour_needed = true)
        {
            glm::dvec3 centre;
            double radius;
            bounds(level, column, row, centre, radius);
            if (!visible(centre, radius))
                return;
            const bool geometry_refine = wants_geometry(level, centre, radius);
            // Skirts hide mixed-LOD rasterization gaps while geometry advances without its requested imagery.
            const bool geometry_fallback = geometry_refine && options.skirt > 0.0f;
            Tile *tile = exists ? resident(colour_layer, level, column, row) : nullptr;
            if (!tile)
            {
                if (exists && colour_needed)
                {
                    request(colour_layer, level, column, row, false);
                    for (int layer = 1; layer < layer_count; ++layer)
                        request(layer, level, column, row, false);
                    if (!geometry_fallback)
                    {
                        emit(level, column, row, up);
                        return;
                    }
                }
                if (!geometry_refine && (!colour_needed || !colour_children(level, column, row)))
                {
                    emit(level, column, row, up);
                    return;
                }
            }
            else
            {
                tile->used = frame;
                up = 0;
            }
            if (options.elevation)
            {
                const MeshKey key{tile_key(level, column, row), up, 0, 0};
                const auto mesh = meshes.find(key);
                if (mesh != meshes.end())
                    mesh->second.used = frame;
            }
            for (int layer = 1; layer < layer_count; ++layer)
                request(layer, level, column, row, false);
            bool refine = geometry_refine;
            const bool colour_refine = level < max_level && wants_detail(level - up, centre, radius);
            if (!refine && colour_refine)
            {
                refine = false;
                for (int k = 0; k < 4 && !refine; ++k)
                {
                    const int cc = 2 * column + (k & 1), cr = 2 * row + (k >> 1);
                    refine = usable(colour_layer, level + 1, cc, cr) || colour_children(level + 1, cc, cr);
                }
            }
            if (!refine)
            {
                emit(level, column, row, up);
                return;
            }
            std::array<std::pair<double, int>, 4> children{};
            for (int k = 0; k < 4; ++k)
            {
                children[static_cast<std::size_t>(k)].second = k;
                if (options.elevation && options.max_relief_patches > 0)
                {
                    glm::dvec3 child_centre;
                    double child_radius;
                    bounds(level + 1, 2 * column + (k & 1), 2 * row + (k >> 1), child_centre, child_radius);
                    children[static_cast<std::size_t>(k)].first = glm::length(eye - child_centre);
                }
            }
            if (options.elevation && options.max_relief_patches > 0)
                std::sort(children.begin(), children.end());
            if (refine && options.elevation && options.limb_fade <= 0.0f)
            {
                std::array<bool, 4> child_visible{};
                int visible_children = 0;
                for (int k = 0; k < 4; ++k)
                {
                    glm::dvec3 child_centre;
                    double child_radius;
                    bounds(level + 1, 2 * column + (k & 1), 2 * row + (k >> 1), child_centre, child_radius);
                    child_visible[static_cast<std::size_t>(k)] = visible(child_centre, child_radius);
                    visible_children += child_visible[static_cast<std::size_t>(k)] ? 1 : 0;
                }
                if (options.max_relief_patches > 0 &&
                    relief_patch_budget_used + visible_children - 1 > options.max_relief_patches)
                {
                    report_relief_limit();
                    emit(level, column, row, up);
                    return;
                }
                bool ready = true;
                for (const auto &child_order : children)
                {
                    const int k = child_order.second;
                    const int cc = 2 * column + (k & 1), cr = 2 * row + (k >> 1);
                    const int child_level = level + 1;
                    if (!child_visible[static_cast<std::size_t>(k)])
                        continue;
                    if (colour_refine && usable(colour_layer, child_level, cc, cr))
                    {
                        request(colour_layer, child_level, cc, cr, false);
                        if (!resident(colour_layer, child_level, cc, cr) && !geometry_fallback)
                        {
                            ready = false;
                            continue;
                        }
                    }
                    const int child_up = resident(colour_layer, child_level, cc, cr) ? 0 : up + 1;
                    const std::size_t before = leaves.size();
                    emit(child_level, cc, cr, child_up);
                    if (leaves.size() == before)
                    {
                        ready = false;
                        continue;
                    }
                    const Leaf child = leaves.back();
                    leaves.pop_back();
                    if (child.under)
                    {
                        const bool under_ready = patch_mesh(child_level, cc, cr, 1, 2) != nullptr;
                        const bool top_ready = child.edges ? patch_mesh(child_level, cc, cr, 0, 3, child.edges) != nullptr
                                                           : patch_mesh(child_level, cc, cr, 0, 0) != nullptr;
                        ready = ready && under_ready && top_ready;
                    }
                    else
                        ready = (patch_mesh(child_level, cc, cr, child.up, 0) != nullptr) && ready;
                }
                refine = ready;
                if (refine)
                    relief_patch_budget_used += visible_children - 1;
            }
            if (!refine)
            {
                emit(level, column, row, up);
                return;
            }
            for (const auto &child : children)
            {
                const int k = child.second;
                const int cc = 2 * column + (k & 1), cr = 2 * row + (k >> 1);
                visit(level + 1, cc, cr, usable(colour_layer, level + 1, cc, cr), up + 1, colour_refine);
            }
        }

        static bool build_patch(const PlanetTilesOptions &options, int base_split, int tile_px,
                                int level, int column, int row, int up, int layer, int edges, AsyncMeshData &data)
        {
            const int n = std::max(options.segments, 2);
            const double cols = columns(level, base_split), rws = rows(level, base_split);
            const glm::dvec3 centre = options.elevation ? sphere_point((column + 0.5) / cols, (row + 0.5) / rws) : glm::dvec3(0.0);
            float rect[4];
            ancestor_uv(column, row, up, rect);
            const float inset = 0.5f / static_cast<float>(std::max(tile_px, 1));
            if (rect[0] == 0.0f)
                rect[0] = inset;
            if (rect[1] == 0.0f)
                rect[1] = inset;
            if (rect[2] == 1.0f)
                rect[2] = 1.0f - inset;
            if (rect[3] == 1.0f)
                rect[3] = 1.0f - inset;

            const double skirt_depth = options.skirt * glm::pi<double>() / rws;
            const int stride = n + 1;
            std::vector<Vertex3D> &vertices = data.vertices;
            std::vector<std::uint32_t> &indices = data.indices;
            bool elevation_valid = true;
            const auto height_at = [&](const glm::dvec3 &direction, bool position)
            {
                if (!options.elevation)
                    return 0.0;
                const double height = position && options.elevation_lod
                    ? options.elevation_lod(direction, glm::pi<double>() / (rws * n))
                    : options.elevation(direction);
                if (!std::isfinite(height) || std::fabs(height) > options.max_elevation)
                {
                    elevation_valid = false;
                    return 0.0;
                }
                return height;
            };
            const auto elevated = [&](const glm::dvec3 &direction, bool position)
            {
                return direction * (1.0 + height_at(direction, position));
            };
            const int height_stride = n + 3;
            std::vector<double> heights;
            if (options.adaptive_elevation_diagonals)
            {
                heights.resize(static_cast<std::size_t>(height_stride) * height_stride);
                for (int j = -1; j <= n + 1; ++j)
                    for (int i = -1; i <= n + 1; ++i)
                    {
                        const glm::dvec3 direction = sphere_point((column + static_cast<double>(i) / n) / cols,
                                                                   (row + static_cast<double>(j) / n) / rws);
                        heights[static_cast<std::size_t>(j + 1) * height_stride + i + 1] = height_at(direction, true);
                    }
            }
            auto add_vertex = [&](int i, int j, double scale)
            {
                const double fu = static_cast<double>(i) / n, fv = static_cast<double>(j) / n;
                const glm::dvec3 p = sphere_point((column + fu) / cols, (row + fv) / rws);
                const glm::dvec3 position = heights.empty()
                    ? elevated(p, true)
                    : p * (1.0 + heights[static_cast<std::size_t>(j + 1) * height_stride + i + 1]);
                glm::dvec3 normal = p;
                if (options.adaptive_elevation_normals)
                {
                    const auto h = [&](int x, int y)
                    {
                        return heights[static_cast<std::size_t>(y + 1) * height_stride + x + 1];
                    };
                    normal = detail::planet_tile_elevation_normal((column + fu) / cols, (row + fv) / rws,
                                                                   1.0 / (cols * n), 1.0 / (rws * n),
                                                                   h(i - 1, j), h(i + 1, j),
                                                                   h(i, j - 1), h(i, j + 1));
                }
                else if (options.elevation)
                {
                    const glm::dvec3 axis = std::fabs(p.y) < 0.9 ? glm::dvec3(0, 1, 0) : glm::dvec3(1, 0, 0);
                    const glm::dvec3 tangent = glm::normalize(glm::cross(axis, p));
                    const glm::dvec3 bitangent = glm::cross(p, tangent);
                    const double eps = options.normal_sample_spacing > 0.0
                        ? options.normal_sample_spacing
                        : std::max(glm::pi<double>() / (rws * n), 1.0e-7);
                    const glm::dvec3 dx = elevated(glm::normalize(p + eps * tangent), false) - elevated(glm::normalize(p - eps * tangent), false);
                    const glm::dvec3 dy = elevated(glm::normalize(p + eps * bitangent), false) - elevated(glm::normalize(p - eps * bitangent), false);
                    const glm::dvec3 cross = glm::cross(dx, dy);
                    if (glm::length(cross) > 1.0e-15)
                        normal = glm::normalize(cross);
                }
                Vertex3D v;
                for (int a = 0; a < 3; ++a)
                {
                    v.position[a] = static_cast<float>(position[a] * scale - centre[a]);
                    v.normal[a] = static_cast<float>(normal[a]);
                }
                v.uv[0] = rect[0] + (rect[2] - rect[0]) * static_cast<float>(fu);
                v.uv[1] = rect[1] + (rect[3] - rect[1]) * static_cast<float>(fv);
                // Tile-local coordinates stay independent of the colour ancestor and its edge inset.
                v.extra[0] = static_cast<float>(fu);
                v.extra[1] = static_cast<float>(fv);
                if (edges && options.feather > 0.0f)
                {
                    // Opaque inside, fading to clear along each side that has no neighbour at this level.
                    double opacity = 1.0;
                    const double w = options.feather;
                    if (edges & 1)
                        opacity = std::min(opacity, fu / w);
                    if (edges & 2)
                        opacity = std::min(opacity, (1.0 - fu) / w);
                    if (edges & 4)
                        opacity = std::min(opacity, fv / w);
                    if (edges & 8)
                        opacity = std::min(opacity, (1.0 - fv) / w);
                    opacity = std::clamp(opacity, 0.0, 1.0);
                    v.colour[3] = static_cast<float>(opacity * opacity * (3.0 - 2.0 * opacity));
                }
                vertices.push_back(v);
            };
            // The underlay sits just beneath the patch drawn over it, well within the skirt, so the two never fight for depth.
            const double surface = layer == 2 ? 1.0 - 0.2 * skirt_depth : 1.0;
            for (int j = 0; j <= n; ++j)
                for (int i = 0; i <= n; ++i)
                    add_vertex(i, j, surface);
            for (int j = 0; j < n; ++j)
                for (int i = 0; i < n; ++i)
                {
                    const std::uint32_t a = static_cast<std::uint32_t>(j * stride + i);
                    bool flipped = false;
                    if (!heights.empty())
                    {
                        const auto h = [&](int x, int y)
                        {
                            return heights[static_cast<std::size_t>(y + 1) * height_stride + x + 1];
                        };
                        flipped = detail::planet_tile_flipped_diagonal(h(i, j), h(i, j + 1), h(i + 1, j), h(i + 1, j + 1),
                                                                      h(i - 1, j - 1), h(i - 1, j + 2),
                                                                      h(i + 2, j - 1), h(i + 2, j + 2));
                    }
                    for (const std::uint32_t index : detail::planet_tile_cell_indices(a, stride, flipped))
                        indices.push_back(index);
                }
            // Skirts: a ring of vertices pulled towards the centre, joined to the border both ways round so they show from either side.
            const double depth = 1.0 - skirt_depth;
            const std::uint32_t skirt_base = static_cast<std::uint32_t>(vertices.size());
            std::vector<std::uint32_t> border;
            std::vector<std::pair<int, int>> ring;
            for (int i = 0; i < n; ++i)
                ring.emplace_back(i, 0);
            for (int j = 0; j < n; ++j)
                ring.emplace_back(n, j);
            for (int i = n; i > 0; --i)
                ring.emplace_back(i, n);
            for (int j = n; j > 0; --j)
                ring.emplace_back(0, j);
            for (const auto &[i, j] : ring)
            {
                border.push_back(static_cast<std::uint32_t>(j * stride + i));
                add_vertex(i, j, depth);
            }
            if (!elevation_valid)
            {
                detail::set_error("PlanetTiles: elevation callback returned a non-finite or out-of-range height");
                return false;
            }
            const std::uint32_t count = static_cast<std::uint32_t>(ring.size());
            for (std::uint32_t k = 0; k < count; ++k)
            {
                const std::uint32_t k2 = (k + 1) % count;
                const std::uint32_t a = border[k], b = border[k2], c = skirt_base + k, d = skirt_base + k2;
                for (const std::uint32_t index : {a, b, c, b, d, c, a, c, b, b, c, d})
                    indices.push_back(index);
            }
            return true;
        }

        bool mesh_jobs_full() const
        {
            if (options.max_relief_mesh_jobs == 0)
                return false;
            int jobs = 0;
            for (const auto &item : meshes)
                jobs += item.second.load.valid() ? 1 : 0;
            return jobs >= options.max_relief_mesh_jobs;
        }

        Mesh *patch_mesh(int level, int column, int row, int up, int layer, int edges = 0)
        {
            // Layers: 0 colour, 1 specular glint, 2 the lowered underlay, 3 the edge-feathered colour patch.
            const MeshKey key{tile_key(level, column, row), up, layer, edges};
            auto found = meshes.find(key);
            if (found == meshes.end() && options.elevation && options.limb_fade <= 0.0f && mesh_jobs_full())
            {
                relief_limited = true;
                return nullptr;
            }
            if (found == meshes.end() && options.elevation && options.max_relief_meshes > 0 &&
                meshes.size() >= static_cast<std::size_t>(options.max_relief_meshes))
            {
                auto oldest = meshes.end();
                for (auto it = meshes.begin(); it != meshes.end(); ++it)
                {
                    const bool root = static_cast<int>(it->first.tile >> 56) == sets[colour_layer].min_level &&
                                      it->first.up == 0 && it->first.layer == 0 && it->first.edges == 0;
                    if (!root && !it->second.load.valid() && frame > 3 && it->second.used < frame - 3 &&
                        (oldest == meshes.end() || it->second.used < oldest->second.used))
                        oldest = it;
                }
                if (oldest == meshes.end())
                {
                    report_relief_limit();
                    return nullptr;
                }
                destroy_mesh(oldest->second.mesh);
                meshes.erase(oldest);
            }
            if (found == meshes.end())
                found = meshes.emplace(key, MeshEntry{}).first;
            MeshEntry &entry = found->second;
            entry.used = frame;
            if (entry.mesh)
            {
                apply_limb_fade(entry);
                return entry.mesh;
            }
            if (entry.failed)
                return nullptr;
            const int base_split = sets[colour_layer].base_split;
            if (options.elevation && options.limb_fade <= 0.0f)
            {
                if (!entry.load.valid())
                {
                    if (mesh_jobs_full())
                    {
                        relief_limited = true;
                        return nullptr;
                    }
                    const PlanetTilesOptions settings = options;
                    const int pixels = tile_px;
                    if (options.match_elevation_edges)
                        entry.original_vertices = std::make_shared<std::vector<Vertex3D>>();
                    const auto original_vertices = entry.original_vertices;
                    if (options.match_elevation_edges && options.adaptive_elevation_diagonals)
                        entry.diagonals = std::make_shared<std::vector<std::uint8_t>>();
                    const auto diagonals = entry.diagonals;
                    const bool retain = mesh_cpu_data_retained();
                    // Stitched normal-mapped meshes need their indices to regenerate tangents on update.
                    const bool retain_indices = options.match_elevation_edges && sets[normal_layer].valid() &&
                                                layer == 0 && edges == 0;
                    entry.indices_only = retain_indices && !retain;
                    if (retain_indices)
                        set_mesh_cpu_data_retained(true);
                    entry.load = build_mesh_async([settings, base_split, pixels, level, column, row, up, layer, edges, original_vertices, diagonals](AsyncMeshData &data)
                    {
                        if (!build_patch(settings, base_split, pixels, level, column, row, up, layer, edges, data))
                            return false;
                        if (original_vertices)
                            *original_vertices = data.vertices;
                        if (diagonals)
                        {
                            const int n = std::max(settings.segments, 2);
                            diagonals->resize(static_cast<std::size_t>(n) * n);
                            for (int j = 0; j < n; ++j)
                                for (int i = 0; i < n; ++i)
                                {
                                    const std::size_t cell = static_cast<std::size_t>(j * n + i);
                                    const std::uint32_t a = static_cast<std::uint32_t>(j * (n + 1) + i);
                                    (*diagonals)[cell] = data.indices[cell * 6 + 1] == a + n + 2;
                                }
                        }
                        return true;
                    });
                    set_mesh_cpu_data_retained(retain);
                }
                if (!entry.load.done())
                    return nullptr;
                if (entry.load.failed())
                {
                    entry.error = entry.load.error();
                    std::fprintf(stderr, "[simlib] PlanetTiles: cannot build mesh %d/%d/%d: %s\n",
                                 level, column, row, entry.error.c_str());
                    entry.failed = true;
                    entry.load = AsyncMesh();
                    entry.original_vertices.reset();
                    entry.diagonals.reset();
                    return nullptr;
                }
                entry.mesh = entry.load.get();
                if (entry.mesh && entry.indices_only)
                    std::vector<Vertex3D>().swap(entry.mesh->vertices);
                if (entry.mesh && entry.original_vertices)
                    entry.vertices = *entry.original_vertices;
                return entry.mesh;
            }
            AsyncMeshData data;
            if (!build_patch(options, base_split, tile_px, level, column, row, up, layer, edges, data))
            {
                entry.error = last_error();
                std::fprintf(stderr, "[simlib] %s\n", entry.error.c_str());
                entry.failed = true;
                return nullptr;
            }
            const bool retain = mesh_cpu_data_retained();
            const bool retain_indices = options.limb_fade > 0.0f && sets[normal_layer].valid();
            if (retain_indices)
                set_mesh_cpu_data_retained(true);
            entry.mesh = create_mesh(data.vertices, data.indices);
            set_mesh_cpu_data_retained(retain);
            if (entry.mesh && retain_indices && !retain)
                std::vector<Vertex3D>().swap(entry.mesh->vertices);
            if (options.limb_fade > 0.0f)
            {
                entry.base_alpha.reserve(data.vertices.size());
                for (const Vertex3D &v : data.vertices)
                    entry.base_alpha.push_back(v.colour[3]);
                entry.vertices = std::move(data.vertices);
                apply_limb_fade(entry);
            }
            return entry.mesh;
        }

        // Scales each vertex's opacity by how squarely the eye looks at the sphere there; the eye is in the unit sphere's frame.
        void apply_limb_fade(MeshEntry &entry)
        {
            if (options.limb_fade <= 0.0f || entry.vertices.empty() || eye_distance <= 1.0)
            {
                if (entry.faded && !entry.vertices.empty())
                {
                    for (std::size_t i = 0; i < entry.vertices.size(); ++i)
                        entry.vertices[i].colour[3] = entry.base_alpha[i];
                    update_mesh_vertices(entry.mesh, entry.vertices);
                    entry.faded = false;
                }
                return;
            }
            const double full = options.limb_fade, clear = 0.3 * options.limb_fade;
            bool any = false;
            std::vector<float> alpha(entry.vertices.size());
            for (std::size_t i = 0; i < entry.vertices.size(); ++i)
            {
                const Vertex3D &v = entry.vertices[i];
                const glm::dvec3 p(v.position[0], v.position[1], v.position[2]);
                const double len = glm::length(p);
                const double facing = len > 0.0 ? -glm::dot(p / len, glm::normalize(p - eye)) : 1.0;
                const double t = std::clamp((facing - clear) / (full - clear), 0.0, 1.0);
                alpha[i] = entry.base_alpha[i] * static_cast<float>(t * t * (3.0 - 2.0 * t));
                any = any || t < 1.0;
            }
            if (!any && !entry.faded)
                return;
            for (std::size_t i = 0; i < alpha.size(); ++i)
                entry.vertices[i].colour[3] = alpha[i];
            update_mesh_vertices(entry.mesh, entry.vertices);
            entry.faded = any;
        }

        void match_edges()
        {
            matched_edges = 0;
            edge_match_failed = false;
            if (!options.match_elevation_edges)
                return;
            std::unordered_map<std::uint64_t, const Leaf *> visible_leaves;
            std::vector<const Leaf *> order;
            for (const Leaf &leaf : leaves)
            {
                visible_leaves.emplace(tile_key(leaf.level, leaf.column, leaf.row), &leaf);
                order.push_back(&leaf);
            }
            std::sort(order.begin(), order.end(), [](const Leaf *a, const Leaf *b) { return a->level < b->level; });
            const int base_split = sets[colour_layer].base_split;
            const int n = std::max(options.segments, 2);
            const auto centre_of = [&](const Leaf &leaf)
            {
                return sphere_point((leaf.column + 0.5) / columns(leaf.level, base_split),
                                    (leaf.row + 0.5) / rows(leaf.level, base_split));
            };
            for (const Leaf *leaf : order)
            {
                Mesh *mesh = patch_mesh(leaf->level, leaf->column, leaf->row, leaf->up, 0);
                if (!mesh)
                    continue;
                MeshEntry &entry = meshes.at(MeshKey{tile_key(leaf->level, leaf->column, leaf->row), leaf->up, 0, 0});
                if (!entry.original_vertices)
                    continue;
                std::array<detail::PlanetTileEdge, 4> edges{};
                std::array<detail::PlanetTileCorner, 4> corners{};
                bool has_edge = false;
                const int cols = columns(leaf->level, base_split), rws = rows(leaf->level, base_split);
                for (int side = 0; side < 4; ++side)
                {
                    int column, row;
                    if (!detail::adjacent_planet_tile(leaf->column, leaf->row, cols, rws,
                                                      side == 0 ? -1 : side == 1 ? 1 : 0,
                                                      side == 2 ? -1 : side == 3 ? 1 : 0, column, row))
                        continue;
                    for (int level = leaf->level - 1; level >= sets[colour_layer].min_level; --level)
                    {
                        const int delta = leaf->level - level;
                        const auto found = visible_leaves.find(tile_key(level, column >> delta, row >> delta));
                        if (found == visible_leaves.end())
                            continue;
                        const Leaf &neighbour = *found->second;
                        if (patch_mesh(neighbour.level, neighbour.column, neighbour.row, neighbour.up, 0))
                        {
                            MeshEntry &other = meshes.at(MeshKey{tile_key(level, neighbour.column, neighbour.row), neighbour.up, 0, 0});
                            edges[side] = {&other.vertices, centre_of(neighbour), delta};
                            has_edge = true;
                            ++matched_edges;
                        }
                        break;
                    }
                }
                for (int corner = 0; corner < 4; ++corner)
                {
                    const bool right = (corner & 1) != 0, bottom = (corner & 2) != 0;
                    int column, row;
                    if (!detail::adjacent_planet_tile(leaf->column, leaf->row, cols, rws,
                                                      right ? 1 : -1, bottom ? 1 : -1, column, row))
                        continue;
                    for (int level = leaf->level - 1; level >= sets[colour_layer].min_level; --level)
                    {
                        const int delta = leaf->level - level;
                        const auto found = visible_leaves.find(tile_key(level, column >> delta, row >> delta));
                        if (found == visible_leaves.end())
                            continue;
                        const Leaf &neighbour = *found->second;
                        if (patch_mesh(neighbour.level, neighbour.column, neighbour.row, neighbour.up, 0))
                        {
                            MeshEntry &other = meshes.at(MeshKey{tile_key(level, neighbour.column, neighbour.row), neighbour.up, 0, 0});
                            const int span = 1 << delta;
                            corners[corner] = {{&other.vertices, centre_of(neighbour), delta},
                                               static_cast<double>((column % span) + (right ? 0 : 1)) * n / span,
                                               static_cast<double>((row % span) + (bottom ? 0 : 1)) * n / span,
                                               other.diagonals.get()};
                            has_edge = true;
                        }
                        break;
                    }
                }
                if (!has_edge && !entry.stitched && !entry.edge_upload.failed)
                    continue;
                std::vector<Vertex3D> candidate = *entry.original_vertices;
                detail::match_planet_tile_edges(candidate, centre_of(*leaf), n, leaf->column, leaf->row, edges, corners);
                bool changed = candidate.size() != entry.vertices.size();
                for (std::size_t i = 0; !changed && i < candidate.size(); ++i)
                {
                    for (int axis = 0; axis < 3; ++axis)
                        if (candidate[i].position[axis] != entry.vertices[i].position[axis] ||
                            candidate[i].normal[axis] != entry.vertices[i].normal[axis])
                        {
                            changed = true;
                            break;
                        }
                    for (int axis = 0; !changed && axis < 2; ++axis)
                        changed = candidate[i].uv[axis] != entry.vertices[i].uv[axis] ||
                                  candidate[i].extra[axis] != entry.vertices[i].extra[axis];
                }
                const bool upload_needed = changed || entry.edge_upload.failed;
                if (!entry.edge_upload.update(changed, [&] { return update_mesh_vertices(mesh, candidate); }))
                {
                    edge_match_failed = true;
                    std::fprintf(stderr, "[simlib] PlanetTiles: cannot match mesh edges: %s\n", last_error().c_str());
                }
                else
                {
                    if (upload_needed)
                        entry.vertices = std::move(candidate);
                    entry.stitched = has_edge;
                }
            }
        }

        void evict()
        {
            // Meshes and textures unused for a few frames may be released, oldest first, once over their budgets.
            const std::uint64_t safe = frame > 3 ? frame - 3 : 0;
            const std::size_t mesh_budget = static_cast<std::size_t>(std::max(options.max_cached_meshes, 1));
            if (meshes.size() > mesh_budget)
            {
                std::vector<std::pair<std::uint64_t, MeshKey>> old;
                for (const auto &entry : meshes)
                    if (entry.second.used < safe && !(options.max_relief_mesh_jobs > 0 && entry.second.load.valid()) &&
                        !(static_cast<int>(entry.first.tile >> 56) == sets[colour_layer].min_level &&
                          entry.first.up == 0 && entry.first.layer == 0 && entry.first.edges == 0))
                        old.emplace_back(entry.second.used, entry.first);
                std::sort(old.begin(), old.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
                for (const auto &item : old)
                {
                    if (meshes.size() <= mesh_budget * 3 / 4)
                        break;
                    destroy_mesh(meshes[item.second].mesh);
                    meshes.erase(item.second);
                }
            }
            std::size_t loaded = 0;
            for (const auto &map : tiles)
                for (const auto &entry : map)
                    loaded += entry.second.bitmap ? 1 : 0;
            if (loaded <= static_cast<std::size_t>(std::max(options.max_cached_tiles, 8)))
                return;
            struct Candidate
            {
                std::uint64_t used;
                int layer;
                std::uint64_t key;
            };
            std::vector<Candidate> old;
            for (int layer = 0; layer < layer_count; ++layer)
                for (const auto &entry : tiles[layer])
                {
                    const int level = static_cast<int>(entry.first >> 56);
                    if (entry.second.bitmap && entry.second.used < safe && !(layer == colour_layer && level == sets[colour_layer].min_level))
                        old.push_back({entry.second.used, layer, entry.first});
                }
            std::sort(old.begin(), old.end(), [](const Candidate &a, const Candidate &b) { return a.used < b.used; });
            for (const Candidate &c : old)
            {
                if (loaded <= static_cast<std::size_t>(options.max_cached_tiles))
                    break;
                Tile &tile = tiles[c.layer][c.key];
                destroy_bitmap(tile.bitmap);
                tiles[c.layer].erase(c.key);
                --loaded;
            }
        }
    };

    PlanetTiles::PlanetTiles() : impl_(new Impl) {}
    PlanetTiles::~PlanetTiles() = default;

    std::unique_ptr<PlanetTiles> PlanetTiles::create(const PlanetTilesOptions &options)
    {
        std::unique_ptr<PlanetTiles> planet(new PlanetTiles);
        Impl &m = *planet->impl_;
        m.options = options;
        if (!std::isfinite(options.skirt) || options.skirt < 0.0f)
        {
            detail::set_error("PlanetTiles: skirt depth must be finite and non-negative");
            return nullptr;
        }
        if (!std::isfinite(options.max_elevation) || options.max_elevation < 0.0 ||
            options.max_elevation >= 1.0 || (options.elevation && (options.max_elevation == 0.0 || options.limb_fade > 0.0f)))
        {
            detail::set_error("PlanetTiles: invalid elevation bound");
            return nullptr;
        }
        if (!std::isfinite(options.normal_sample_spacing) || options.normal_sample_spacing < 0.0 ||
            options.normal_sample_spacing > 1.0)
        {
            detail::set_error("PlanetTiles: invalid normal sample spacing");
            return nullptr;
        }
        if (!std::isfinite(options.geometry_pixels) || options.geometry_pixels < 0.0f ||
            (options.geometry_pixels > 0.0f && !options.elevation))
        {
            detail::set_error("PlanetTiles: invalid geometry pixel threshold");
            return nullptr;
        }
        if (options.match_elevation_edges &&
            (!options.elevation || options.feather != 0.0f || options.fade_seconds != 0.0f || !options.specular.empty()))
        {
            detail::set_error("PlanetTiles: matching elevation edges requires relief without feathering, fading or specular tiles");
            return nullptr;
        }
        if (options.elevation_lod && (!options.elevation || !options.match_elevation_edges || options.normal_sample_spacing <= 0.0))
        {
            detail::set_error("PlanetTiles: LOD elevation requires fixed normal spacing and matched edges");
            return nullptr;
        }
        if (options.adaptive_elevation_diagonals && !options.elevation)
        {
            detail::set_error("PlanetTiles: adaptive elevation diagonals require elevation");
            return nullptr;
        }
        if (options.adaptive_elevation_normals && !options.adaptive_elevation_diagonals)
        {
            detail::set_error("PlanetTiles: adaptive elevation normals require adaptive elevation diagonals");
            return nullptr;
        }
        if (options.max_relief_meshes < 0 || options.max_relief_patches < 0 || options.max_relief_mesh_jobs < 0)
        {
            detail::set_error("PlanetTiles: relief resource limits cannot be negative");
            return nullptr;
        }
        if (options.max_failed_tiles < 0)
        {
            detail::set_error("PlanetTiles: failed tile limit cannot be negative");
            return nullptr;
        }
        if (options.max_resident_tiles < 0 ||
            (options.max_resident_tiles > 0 && options.max_resident_tiles <= std::max(options.max_cached_tiles, 8)))
        {
            detail::set_error("PlanetTiles: resident tile limit must exceed the eviction target and eight tiles");
            return nullptr;
        }
        std::string why;
        if (options.has_colour || options.load_colour)
        {
            if (!options.has_colour || !options.load_colour || !options.colour.empty() ||
                options.colour_min_level < 0 || options.colour_max_level < options.colour_min_level ||
                options.colour_max_level > 25 || options.colour_base_split < 0 || options.colour_base_split > 25 ||
                options.colour_max_level + options.colour_base_split > 25 ||
                options.colour_min_level + options.colour_base_split > 8)
            {
                detail::set_error("PlanetTiles: invalid colour tile provider");
                return nullptr;
            }
            TileSet &set = m.sets[colour_layer];
            set.base_split = options.colour_base_split;
            set.min_level = options.colour_min_level;
            set.max_level = options.colour_max_level;
        }
        else if (options.colour.empty() || !open_tile_set(options.colour, m.sets[colour_layer], why))
        {
            detail::set_error("PlanetTiles: " + (why.empty() ? std::string("no colour map given") : why));
            return nullptr;
        }
        const std::string optional[] = {"", options.specular, options.normal};
        for (int layer = 1; layer < layer_count; ++layer)
            if (!optional[layer].empty() && !open_tile_set(optional[layer], m.sets[layer], why))
            {
                detail::set_error("PlanetTiles: " + why);
                return nullptr;
            }
        TileSet &colour = m.sets[colour_layer];
        m.max_level = options.max_level < 0 ? colour.max_level : std::min(options.max_level, colour.max_level);
        m.max_level = std::max(m.max_level, colour.min_level);
        if (options.geometry_max_level < -1 || options.geometry_max_level > 25 - colour.base_split)
        {
            detail::set_error("PlanetTiles: invalid geometry maximum level");
            return nullptr;
        }
        m.geometry_max_level = options.geometry_max_level < 0 ? m.max_level : options.geometry_max_level;
        if (options.max_level >= 0)
            m.geometry_max_level = std::min(m.geometry_max_level, options.max_level);
        m.geometry_max_level = std::max(m.geometry_max_level, colour.min_level);

        // The coarsest level is loaded now and kept, so every patch always has something to draw.
        const int cols = columns(colour.min_level, colour.base_split), rws = rows(colour.min_level, colour.base_split);
        if (options.max_resident_tiles > 0 && options.max_resident_tiles < cols * rws)
        {
            detail::set_error("PlanetTiles: resident tile limit cannot accommodate the coarsest tiles");
            return nullptr;
        }
        if (options.elevation && ((options.max_relief_meshes > 0 && options.max_relief_meshes < cols * rws) ||
                                  (options.max_relief_patches > 0 && options.max_relief_patches < cols * rws)))
        {
            detail::set_error("PlanetTiles: relief limits cannot accommodate the coarsest tiles");
            return nullptr;
        }
        for (int r = 0; r < rws; ++r)
            for (int c = 0; c < cols; ++c)
            {
                if (!m.usable(colour_layer, colour.min_level, c, r))
                {
                    detail::set_error("PlanetTiles: the coarsest level is missing tile " + std::to_string(c) + "," + std::to_string(r));
                    return nullptr;
                }
                m.request(colour_layer, colour.min_level, c, r, true);
                Impl::Tile *tile = m.resident(colour_layer, colour.min_level, c, r);
                if (!tile)
                {
                    const auto failed = m.tiles[colour_layer].find(tile_key(colour.min_level, c, r));
                    const std::string message = failed == m.tiles[colour_layer].end()
                        ? "cannot request coarsest tile"
                        : "cannot load " + failed->second.description + ": " + failed->second.error;
                    detail::set_error("PlanetTiles: " + message);
                    return nullptr;
                }
                if (c == 0 && r == 0)
                    m.tile_px = options.tile_size > 0 ? options.tile_size : std::max(tile->bitmap->width, 1);
            }
        if (options.elevation && options.limb_fade <= 0.0f)
            for (int r = 0; r < rws; ++r)
                for (int c = 0; c < cols; ++c)
                {
                    m.patch_mesh(colour.min_level, c, r, 0, 0);
                    Impl::MeshEntry &entry = m.meshes.at({tile_key(colour.min_level, c, r), 0, 0, 0});
                    if (!entry.mesh && entry.load.valid())
                        entry.load.wait();
                    if (!m.patch_mesh(colour.min_level, c, r, 0, 0))
                    {
                        detail::set_error("PlanetTiles: cannot build coarsest relief mesh: " + entry.error);
                        return nullptr;
                    }
                }
        return planet;
    }

    void PlanetTiles::update(const glm::dvec3 &eye, double fov_y, int viewport_height, const glm::mat4 *view_projection)
    {
        if (view_projection)
        {
            const glm::dmat4 matrix(*view_projection);
            update_relative(eye, fov_y, viewport_height, &matrix);
        }
        else
            update_relative(eye, fov_y, viewport_height);
    }

    void PlanetTiles::update_relative(const glm::dvec3 &eye, double fov_y, int viewport_height, const glm::dmat4 *view_projection)
    {
        Impl &m = *impl_;
        ++m.frame;
        m.poll_all();
        m.trim_failed_tiles();
        if (m.options.max_relief_mesh_jobs > 0 && m.frame > 3)
            for (auto it = m.meshes.begin(); it != m.meshes.end();)
            {
                if (it->second.load.valid() && it->second.load.done() && it->second.used < m.frame - 3)
                {
                    if (it->second.load.failed())
                        std::fprintf(stderr, "[simlib] PlanetTiles: abandoned mesh build failed: %s\n", it->second.load.error().c_str());
                    it = m.meshes.erase(it);
                }
                else
                    ++it;
            }
        m.leaves.clear();
        m.eye = eye;
        m.eye_distance = glm::length(eye);
        m.pixel_per_unit = static_cast<double>(std::max(viewport_height, 1)) / (2.0 * std::tan(std::clamp(fov_y, 0.01, 3.0) * 0.5));
        m.frustum = view_projection ? make_frustum(*view_projection) : Frustum{};
        const TileSet &colour = m.sets[colour_layer];
        const int cols = columns(colour.min_level, colour.base_split), rws = rows(colour.min_level, colour.base_split);
        m.relief_patch_budget_used = 0;
        for (int r = 0; r < rws; ++r)
            for (int c = 0; c < cols; ++c)
            {
                glm::dvec3 centre;
                double radius;
                m.bounds(colour.min_level, c, r, centre, radius);
                m.relief_patch_budget_used += m.visible(centre, radius) ? 1 : 0;
            }
        m.relief_limited = false;
        m.texture_limited = false;
        m.failure_limited = false;
        if (m.options.elevation && m.options.max_relief_patches > 0)
        {
            std::vector<std::tuple<double, int, int>> roots;
            roots.reserve(static_cast<std::size_t>(cols) * rws);
            for (int r = 0; r < rws; ++r)
                for (int c = 0; c < cols; ++c)
                {
                    glm::dvec3 centre;
                    double radius;
                    m.bounds(colour.min_level, c, r, centre, radius);
                    roots.emplace_back(glm::length(m.eye - centre), c, r);
                }
            std::sort(roots.begin(), roots.end());
            for (const auto &[distance, column, row] : roots)
            {
                (void)distance;
                m.visit(colour.min_level, column, row, true);
            }
        }
        else
            for (int r = 0; r < rws; ++r)
                for (int c = 0; c < cols; ++c)
                    m.visit(colour.min_level, c, r, true);
        m.match_edges();
        m.evict();
    }

    void PlanetTiles::draw(const glm::mat4 &model, const Material &material)
    {
        draw_relative(glm::dmat4(model), material);
    }

    bool PlanetTiles::drawn_elevation(const glm::dvec3 &direction, double &height) const
    {
        const Impl &m = *impl_;
        const double length = glm::length(direction);
        if (!m.options.elevation || !m.options.match_elevation_edges || m.options.fade_seconds > 0.0f ||
            m.options.feather > 0.0f || !std::isfinite(length) || length < 1e-12)
        {
            detail::set_error("PlanetTiles: drawn elevation requires a valid direction and edge-matched relief without fading");
            return false;
        }
        if (m.edge_match_failed)
        {
            detail::set_error("PlanetTiles: drawn elevation unavailable after a mesh edge update failed");
            return false;
        }
        const glm::dvec3 ray = direction / length;
        double u = std::atan2(-ray.z, ray.x) / (2.0 * glm::pi<double>());
        if (u < 0.0)
            u += 1.0;
        const double v = std::acos(std::clamp(ray.y, -1.0, 1.0)) / glm::pi<double>();
        const int n = std::max(m.options.segments, 2);
        const int base_split = m.sets[colour_layer].base_split;
        for (const Impl::Leaf &leaf : m.leaves)
        {
            const int cols = columns(leaf.level, base_split), rws = rows(leaf.level, base_split);
            double x = u * cols - leaf.column;
            if (x < -0.5 * cols)
                x += cols;
            else if (x > 0.5 * cols)
                x -= cols;
            const double y = v * rws - leaf.row;
            if (x < -1.0 / n || x > 1.0 + 1.0 / n || y < -1.0 / n || y > 1.0 + 1.0 / n)
                continue;
            const auto found = m.meshes.find(Impl::MeshKey{tile_key(leaf.level, leaf.column, leaf.row), leaf.up, 0, 0});
            if (found == m.meshes.end() || !found->second.mesh ||
                found->second.vertices.size() < static_cast<std::size_t>((n + 1) * (n + 1)))
                continue;
            const auto &entry = found->second;
            const glm::dvec3 centre = sphere_point((leaf.column + 0.5) / cols, (leaf.row + 0.5) / rws);
            const int cell_x = static_cast<int>(std::floor(x * n)), cell_y = static_cast<int>(std::floor(y * n));
            for (int j = std::max(0, cell_y - 1); j <= std::min(n - 1, cell_y + 1); ++j)
                for (int i = std::max(0, cell_x - 1); i <= std::min(n - 1, cell_x + 1); ++i)
                {
                    const std::size_t cell = static_cast<std::size_t>(j * n + i);
                    const bool flipped = entry.diagonals && (*entry.diagonals)[cell] != 0;
                    const auto indices = detail::planet_tile_cell_indices(static_cast<std::uint32_t>(j * (n + 1) + i),
                                                                          static_cast<std::uint32_t>(n + 1), flipped);
                    for (int triangle = 0; triangle < 2; ++triangle)
                    {
                        glm::dvec3 p[3];
                        for (int k = 0; k < 3; ++k)
                        {
                            const Vertex3D &vertex = entry.vertices[indices[triangle * 3 + k]];
                            p[k] = centre + glm::dvec3(vertex.position[0], vertex.position[1], vertex.position[2]);
                        }
                        const glm::dvec3 e1 = p[1] - p[0], e2 = p[2] - p[0];
                        const glm::dvec3 cross = glm::cross(ray, e2);
                        const double determinant = glm::dot(e1, cross);
                        if (std::fabs(determinant) < 1e-25)
                            continue;
                        const glm::dvec3 from_vertex = -p[0];
                        const double a = glm::dot(from_vertex, cross) / determinant;
                        const glm::dvec3 q = glm::cross(from_vertex, e1);
                        const double b = glm::dot(ray, q) / determinant;
                        const double distance = glm::dot(e2, q) / determinant;
                        if (a >= -1e-8 && b >= -1e-8 && a + b <= 1.0 + 1e-8 && distance > 0.0)
                        {
                            height = distance - 1.0;
                            return true;
                        }
                    }
                }
        }
        detail::set_error("PlanetTiles: no ready drawn relief mesh covers this direction");
        return false;
    }

    void PlanetTiles::draw_relative(const glm::dmat4 &model, const Material &material)
    {
        draw_relative_impl(model, material, nullptr);
    }

    bool PlanetTiles::draw_relative_microdetail(const glm::dmat4 &model, const Material &material,
                                                 const MicrodetailMapper &mapper)
    {
        if (!mapper || material.microdetail.array == 0)
        {
            detail::set_error("PlanetTiles: microdetail requires an array and a mapping callback");
            return false;
        }
        return draw_relative_impl(model, material, &mapper);
    }

    bool PlanetTiles::draw_relative_impl(const glm::dmat4 &model, const Material &material,
                                         const MicrodetailMapper *mapper)
    {
        Impl &m = *impl_;
        struct MappedPatch
        {
            Mesh *under = nullptr;
            Mesh *colour = nullptr;
            PlanetMicrodetail under_detail;
            PlanetMicrodetail colour_detail;
        };
        std::vector<MappedPatch> mapped;
        if (mapper)
        {
            mapped.resize(m.leaves.size());
            const auto prepare = [&](const Impl::Leaf &leaf, int up, Mesh *mesh, PlanetMicrodetail &mapping)
            {
                if (!mesh)
                    return true;
                mapping = material.microdetail;
                std::string error;
                if (!(*mapper)(leaf.level, leaf.column, leaf.row, up, mapping, error))
                {
                    detail::set_error("PlanetTiles: microdetail mapping failed" + (error.empty() ? std::string() : ": " + error));
                    return false;
                }
                for (const auto &range : mapping.ranges)
                    for (float value : range)
                        if (!std::isfinite(value))
                        {
                            detail::set_error("PlanetTiles: microdetail mapping contains a non-finite range");
                            return false;
                        }
                return true;
            };
            for (std::size_t i = 0; i < m.leaves.size(); ++i)
            {
                const Impl::Leaf &leaf = m.leaves[i];
                MappedPatch &patch = mapped[i];
                if (leaf.under)
                {
                    patch.under = m.patch_mesh(leaf.level, leaf.column, leaf.row, 1, 2);
                    if (!prepare(leaf, 1, patch.under, patch.under_detail))
                        return false;
                }
                else
                {
                    patch.colour = m.patch_mesh(leaf.level, leaf.column, leaf.row, leaf.up, 0);
                    if (!prepare(leaf, leaf.up, patch.colour, patch.colour_detail))
                        return false;
                }
            }
            for (std::size_t i = 0; i < m.leaves.size(); ++i)
            {
                const Impl::Leaf &leaf = m.leaves[i];
                if (!leaf.under)
                    continue;
                MappedPatch &patch = mapped[i];
                patch.colour = leaf.edges ? m.patch_mesh(leaf.level, leaf.column, leaf.row, 0, 3, leaf.edges)
                                          : m.patch_mesh(leaf.level, leaf.column, leaf.row, 0, 0);
                if (!prepare(leaf, 0, patch.colour, patch.colour_detail))
                    return false;
            }
        }
        const auto patch_model = [&](const Impl::Leaf &leaf, const glm::dmat4 &transform)
        {
            if (!m.options.elevation)
                return glm::mat4(transform);
            const double cols = columns(leaf.level, m.sets[colour_layer].base_split);
            const double rows_count = rows(leaf.level, m.sets[colour_layer].base_split);
            const glm::dvec3 centre = sphere_point((leaf.column + 0.5) / cols, (leaf.row + 0.5) / rows_count);
            return glm::mat4(glm::translate(transform, centre));
        };
        const auto draw_patch = [&](const Impl::Leaf &leaf, Material &patch, Mesh *mesh,
                                    const PlanetMicrodetail *detail)
        {
            if (!mesh)
                return true;
            if (detail)
                patch.microdetail = *detail;
            const bool drawn = draw_mesh(mesh, patch_model(leaf, model), patch);
            return !mapper || drawn;
        };
        // Opaque patches first; those fading in or feathered at their edges put their parent tile down here and are blended over it after.
        for (std::size_t i = 0; i < m.leaves.size(); ++i)
        {
            const Impl::Leaf &leaf = m.leaves[i];
            Material patch = material;
            if (leaf.under)
            {
                patch.texture = leaf.under;
                Mesh *mesh = mapper ? mapped[i].under : m.patch_mesh(leaf.level, leaf.column, leaf.row, 1, 2);
                if (!draw_patch(leaf, patch, mesh, mapper ? &mapped[i].under_detail : nullptr))
                    return false;
                continue;
            }
            patch.texture = leaf.colour;
            patch.normal_map = leaf.normal;
            Mesh *mesh = mapper ? mapped[i].colour : m.patch_mesh(leaf.level, leaf.column, leaf.row, leaf.up, 0);
            if (!draw_patch(leaf, patch, mesh, mapper ? &mapped[i].colour_detail : nullptr))
                return false;
        }
        for (std::size_t i = 0; i < m.leaves.size(); ++i)
        {
            const Impl::Leaf &leaf = m.leaves[i];
            if (!leaf.under)
                continue;
            Material patch = material;
            patch.texture = leaf.colour;
            patch.normal_map = leaf.normal;
            patch.blend = MaterialBlend::alpha;
            patch.tint.alpha = static_cast<std::uint8_t>(static_cast<float>(material.tint.alpha) * leaf.fade);
            Mesh *mesh = mapper ? mapped[i].colour :
                         leaf.edges ? m.patch_mesh(leaf.level, leaf.column, leaf.row, 0, 3, leaf.edges) :
                                      m.patch_mesh(leaf.level, leaf.column, leaf.row, 0, 0);
            if (!draw_patch(leaf, patch, mesh, mapper ? &mapped[i].colour_detail : nullptr))
                return false;
        }
        if (!m.sets[specular_layer].valid() || m.options.specular_strength <= 0.0f)
            return true;
        // The glint is lifted a hair off the surface because backends draw with a strict depth test.
        const glm::dmat4 lifted = model * glm::scale(glm::dmat4(1.0), glm::dvec3(1.0003));
        for (const Impl::Leaf &leaf : m.leaves)
        {
            if (!leaf.specular)
                continue;
            Material glint;
            glint.lit = true;
            glint.pbr = true;
            glint.fog = false;
            glint.cull = material.cull;
            glint.blend = MaterialBlend::additive;
            glint.tint = {0, 0, 0, static_cast<std::uint8_t>(std::clamp(m.options.specular_strength, 0.0f, 1.0f) * 255.0f)};
            glint.metallic = 0.0f;
            glint.roughness = 1.0f;
            glint.occlusion_strength = 0.0f;
            glint.metallic_roughness = leaf.specular;
            if (Mesh *mesh = m.patch_mesh(leaf.level, leaf.column, leaf.row, leaf.spec_up, 1))
                if (!draw_mesh(mesh, patch_model(leaf, lifted), glint) && mapper)
                    return false;
        }
        return true;
    }

    int PlanetTiles::deepest_level() const { return impl_->sets[colour_layer].max_level; }
    int PlanetTiles::tile_size() const { return impl_->tile_px; }
    bool PlanetTiles::has_specular() const { return impl_->sets[specular_layer].valid(); }
    void PlanetTiles::set_specular_strength(float strength) { impl_->options.specular_strength = strength; }

    bool PlanetTiles::set_max_relief_patches(int limit)
    {
        const TileSet &colour = impl_->sets[colour_layer];
        const int roots = columns(colour.min_level, colour.base_split) * rows(colour.min_level, colour.base_split);
        if (limit < 0 || (impl_->options.elevation && limit > 0 && limit < roots))
        {
            detail::set_error("PlanetTiles: invalid relief patch limit");
            return false;
        }
        impl_->options.max_relief_patches = limit;
        return true;
    }

    PlanetTilesStats PlanetTiles::stats() const
    {
        const Impl &m = *impl_;
        PlanetTilesStats s;
        s.patches = static_cast<int>(m.leaves.size());
        for (const Impl::Leaf &leaf : m.leaves)
        {
            s.deepest_level = std::max(s.deepest_level, leaf.level - leaf.up);
            s.deepest_geometry_level = std::max(s.deepest_geometry_level, leaf.level);
            s.patches_using_parent += leaf.up > 0 ? 1 : 0;
            s.max_colour_levels_up = std::max(s.max_colour_levels_up, leaf.up);
            glm::dvec3 centre;
            double radius;
            m.bounds(leaf.level, leaf.column, leaf.row, centre, radius);
            s.undersampled_patches += m.wants_detail(leaf.level - leaf.up, centre, radius) ? 1 : 0;
        }
        for (const auto &map : m.tiles)
            for (const auto &entry : map)
            {
                s.tiles_loaded += entry.second.bitmap ? 1 : 0;
                s.tiles_loading += entry.second.loading ? 1 : 0;
                s.tiles_failed += entry.second.failed ? 1 : 0;
            }
        s.texture_limited = m.texture_limited;
        s.failure_limited = m.failure_limited;
        s.meshes = static_cast<int>(m.meshes.size());
        for (const auto &entry : m.meshes)
            s.mesh_jobs += entry.second.load.valid() ? 1 : 0;
        s.matched_edges = m.matched_edges;
        s.relief_limited = m.relief_limited;
        s.max_relief_patches = m.options.max_relief_patches;
        return s;
    }

} // namespace sl

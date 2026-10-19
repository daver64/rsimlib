#include "terrain.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <limits>
#include <thread>
#include <unordered_map>
#include <utility>

namespace sl
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;

        struct FaceBasis
        {
            dvec3 n, u, v;
        };

        /** u x v == -n, which is also true of the flat terrain's (x, z), so every face shares one winding. */
        FaceBasis face_basis(int face)
        {
            static const dvec3 normals[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
            FaceBasis basis;
            basis.n = normals[std::clamp(face, 0, 5)];
            basis.u = std::fabs(basis.n.y) > 0.5 ? dvec3(1, 0, 0) : dvec3(0, 1, 0);
            basis.v = glm::cross(basis.u, basis.n);
            return basis;
        }

        double smoothstep(double a, double b, double x)
        {
            if (b <= a)
                return x >= b ? 1.0 : 0.0;
            const double t = std::clamp((x - a) / (b - a), 0.0, 1.0);
            return t * t * (3.0 - 2.0 * t);
        }

        /** 1 inside [low, high], fading to 0 over `blend` outside. */
        double band(double x, double low, double high, double blend)
        {
            const double rise = smoothstep(low - blend, low, x);
            const double fall = 1.0 - smoothstep(high, high + blend, x);
            return rise * fall;
        }

        int deepest_level(const TerrainConfig &config)
        {
            if (config.max_level >= 0)
                return std::min(config.max_level, 24);
            const double root = config.shape == TerrainShape::flat ? config.size : config.radius * pi * 0.5;
            const double cells = std::max(config.chunk_cells, 2);
            const double finest = std::max(config.finest_cell_size, 1.0e-3);
            const double ratio = root / (cells * finest);
            return ratio <= 1.0 ? 0 : std::clamp(static_cast<int>(std::ceil(std::log2(ratio))), 0, 24);
        }

        /** Geometry shared by chunk building, selection and queries. */
        struct Geometry
        {
            const TerrainConfig &config;
            const TerrainSource &source;
            bool sphere;
            int cells;
            double root_size;
            int max_level;

            Geometry(const TerrainConfig &c, const TerrainSource &s)
                : config(c), source(s), sphere(c.shape == TerrainShape::sphere), cells(std::max(c.chunk_cells, 2)),
                  root_size(c.shape == TerrainShape::flat ? c.size : c.radius * pi * 0.5), max_level(deepest_level(c))
            {
            }

            double chunk_size(int level) const { return std::ldexp(root_size, -level); }
            double cell_size(int level) const { return chunk_size(level) / cells; }

            /** Position along a face in 0..1 of grid line `i` of the node whose index along that axis is `index`. */
            double unit(int level, int index, double i) const
            {
                return std::ldexp(static_cast<double>(index) * cells + i, -level) / cells;
            }

            /** Flat: the world (x, 0, z); planet: the unit direction from the centre. */
            dvec3 ground(int face, double a, double b) const
            {
                if (!sphere)
                    return {config.origin.x + a * config.size, 0.0, config.origin.z + b * config.size};
                const FaceBasis basis = face_basis(face);
                const double s = -1.0 + 2.0 * a;
                const double t = -1.0 + 2.0 * b;
                const dvec3 cube = basis.n + s * basis.u + t * basis.v;
                // The tangent warp evens out the cell size across a cube face.
                const dvec3 warped(std::tan(cube.x * pi * 0.25), std::tan(cube.y * pi * 0.25), std::tan(cube.z * pi * 0.25));
                return glm::normalize(warped);
            }

            double sample(const dvec3 &g) const
            {
                return sphere ? source.height_on_sphere(g, config.radius) : source.height(g.x, g.z);
            }

            dvec3 position(const dvec3 &g, double h) const
            {
                if (!sphere)
                    return {g.x, config.origin.y + h, g.z};
                return config.origin + g * (config.radius + h);
            }

            dvec3 up(const dvec3 &g) const { return sphere ? g : dvec3(0.0, 1.0, 0.0); }

            /** Surface normal from heights sampled eps metres to each side, so it does not depend on the mesh. */
            dvec3 normal(const dvec3 &g, double eps) const
            {
                if (!sphere)
                {
                    const double hx = (source.height(g.x + eps, g.z) - source.height(g.x - eps, g.z)) / (2.0 * eps);
                    const double hz = (source.height(g.x, g.z + eps) - source.height(g.x, g.z - eps)) / (2.0 * eps);
                    return glm::normalize(dvec3(-hx, 1.0, -hz));
                }
                const dvec3 axis = std::fabs(g.x) < 0.57 ? dvec3(1, 0, 0) : (std::fabs(g.y) < 0.57 ? dvec3(0, 1, 0) : dvec3(0, 0, 1));
                const dvec3 e1 = glm::normalize(glm::cross(g, axis));
                const dvec3 e2 = glm::cross(g, e1);
                const double angle = eps / config.radius;
                auto point = [&](const dvec3 &d)
                {
                    const dvec3 direction = glm::normalize(d);
                    return direction * (config.radius + source.height_on_sphere(direction, config.radius));
                };
                const dvec3 d1 = point(g + angle * e1) - point(g - angle * e1);
                const dvec3 d2 = point(g + angle * e2) - point(g - angle * e2);
                return glm::normalize(glm::cross(d1, d2));
            }
        };

        void layer_weights(const TerrainConfig &config, const std::vector<TerrainLayer> &layers, const TerrainSample &sample,
                           float weights[4])
        {
            for (int i = 0; i < 4; ++i)
            {
                if (i < static_cast<int>(layers.size()))
                {
                    const TerrainLayer &layer = layers[i];
                    const double w = band(sample.height, layer.min_height, layer.max_height, layer.height_blend) *
                                     band(sample.slope_degrees, layer.min_slope, layer.max_slope, layer.slope_blend) *
                                     layer.strength;
                    weights[i] = static_cast<float>(std::max(w, 0.0));
                }
                else
                    weights[i] = 0.0f;
            }
            if (config.weight_function)
                config.weight_function(sample, weights);
            float sum = 0.0f;
            for (int i = 0; i < 4; ++i)
            {
                weights[i] = std::max(weights[i], 0.0f);
                sum += weights[i];
            }
            if (sum <= 1.0e-6f)
            {
                weights[0] = 1.0f;
                weights[1] = weights[2] = weights[3] = 0.0f;
                return;
            }
            for (int i = 0; i < 4; ++i)
                weights[i] /= sum;
        }

        Vertex3D make_terrain_vertex(const dvec3 &relative, const dvec3 &normal, const float colour[4], float u, float v)
        {
            Vertex3D vertex;
            vertex.position[0] = static_cast<float>(relative.x);
            vertex.position[1] = static_cast<float>(relative.y);
            vertex.position[2] = static_cast<float>(relative.z);
            vertex.normal[0] = static_cast<float>(normal.x);
            vertex.normal[1] = static_cast<float>(normal.y);
            vertex.normal[2] = static_cast<float>(normal.z);
            for (int i = 0; i < 4; ++i)
                vertex.colour[i] = colour[i];
            vertex.uv[0] = u;
            vertex.uv[1] = v;
            // Geomorph offset, filled in after the grid is complete. A non-zero w stops create_mesh generating tangents.
            vertex.tangent[0] = vertex.tangent[1] = vertex.tangent[2] = 0.0f;
            vertex.tangent[3] = 1.0f;
            return vertex;
        }
    } // namespace

    TerrainView make_terrain_view(const Camera &camera, const dvec3 &world_position, int viewport_width, int viewport_height)
    {
        TerrainView view;
        view.position = world_position;
        const glm::vec3 forward = camera.target - camera.position;
        view.forward = glm::length(forward) > 0.0f ? dvec3(glm::normalize(forward)) : dvec3(0.0, 0.0, -1.0);
        view.up = dvec3(camera.up);
        view.fov_y_degrees = camera.field_of_view;
        view.viewport_height = std::max(viewport_height, 1);
        view.aspect = static_cast<double>(std::max(viewport_width, 1)) / std::max(viewport_height, 1);
        view.far_distance = camera.far_plane;
        return view;
    }

    TerrainChunkData build_terrain_chunk(const TerrainConfig &config, const TerrainSource &source, const TerrainNodeKey &key)
    {
        const Geometry geo(config, source);
        const int cells = geo.cells;
        const int stride = cells + 1;
        std::vector<TerrainLayer> layers = config.layers;
        if (layers.empty())
            layers = default_terrain_layers(source.min_height(), source.max_height());

        TerrainChunkData data;
        data.key = key;
        data.grid_vertices = stride;

        const std::size_t grid_count = static_cast<std::size_t>(stride) * stride;
        std::vector<dvec3> ground(grid_count);
        std::vector<double> height(grid_count);
        std::vector<dvec3> position(grid_count);
        double min_h = std::numeric_limits<double>::max();
        double max_h = std::numeric_limits<double>::lowest();
        for (int j = 0; j < stride; ++j)
            for (int i = 0; i < stride; ++i)
            {
                const std::size_t index = static_cast<std::size_t>(j) * stride + i;
                ground[index] = geo.ground(key.face, geo.unit(key.level, key.x, i), geo.unit(key.level, key.y, j));
                height[index] = geo.sample(ground[index]);
                position[index] = geo.position(ground[index], height[index]);
                min_h = std::min(min_h, height[index]);
                max_h = std::max(max_h, height[index]);
            }
        data.min_height = min_h;
        data.max_height = max_h;
        const dvec3 centre_ground =
            geo.ground(key.face, std::ldexp(key.x + 0.5, -key.level), std::ldexp(key.y + 0.5, -key.level));
        data.center = geo.position(centre_ground, 0.5 * (min_h + max_h));

        const double cell = geo.cell_size(key.level);
        const double depth = std::max(config.skirt_scale, 0.0) * cell;
        const double tile = std::max(config.uv_tile_metres, 1.0e-3);
        const FaceBasis basis = face_basis(key.face);

        // Texture coordinates are in tile units; subtracting a whole number of tiles keeps them small and precise.
        auto uv_of = [&](const dvec3 &g, double a, double b, double &u, double &v)
        {
            if (!geo.sphere)
            {
                u = g.x / tile;
                v = g.z / tile;
            }
            else
            {
                const double metres = config.radius * pi * 0.25;
                u = (2.0 * a - 1.0) * metres / tile;
                v = (2.0 * b - 1.0) * metres / tile;
            }
        };
        double u0, v0;
        uv_of(ground[0], geo.unit(key.level, key.x, 0), geo.unit(key.level, key.y, 0), u0, v0);
        u0 = std::floor(u0);
        v0 = std::floor(v0);

        const double normal_eps = std::max(cell, 1.0e-3);
        data.vertices.reserve(grid_count + static_cast<std::size_t>(stride) * 4);
        std::vector<dvec3> normals(grid_count);
        for (int j = 0; j < stride; ++j)
            for (int i = 0; i < stride; ++i)
            {
                const std::size_t index = static_cast<std::size_t>(j) * stride + i;
                const dvec3 normal = geo.normal(ground[index], normal_eps);
                normals[index] = normal;
                TerrainSample sample;
                sample.position = ground[index];
                sample.height = height[index];
                sample.normal = normal;
                sample.slope_degrees = std::acos(std::clamp(glm::dot(normal, geo.up(ground[index])), -1.0, 1.0)) * 180.0 / pi;
                float weights[4];
                layer_weights(config, layers, sample, weights);
                float colour[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                if (config.weights_in_vertex_colour)
                {
                    for (int k = 0; k < 4; ++k)
                        colour[k] = weights[k];
                }
                else
                {
                    colour[0] = colour[1] = colour[2] = 0.0f;
                    for (int k = 0; k < 4 && k < static_cast<int>(layers.size()); ++k)
                    {
                        colour[0] += weights[k] * layers[k].colour.red / 255.0f;
                        colour[1] += weights[k] * layers[k].colour.green / 255.0f;
                        colour[2] += weights[k] * layers[k].colour.blue / 255.0f;
                    }
                }
                double u, v;
                uv_of(ground[index], geo.unit(key.level, key.x, i), geo.unit(key.level, key.y, j), u, v);
                data.vertices.push_back(make_terrain_vertex(position[index] - data.center, normal, colour,
                                                            static_cast<float>(u - u0), static_cast<float>(v - v0)));
            }

        if (config.geomorph && key.level > 0)
        {
            const int parent_x = key.x >> 1, parent_y = key.y >> 1;
            std::vector<dvec3> parent(grid_count);
            for (int j = 0; j < stride; ++j)
                for (int i = 0; i < stride; ++i)
                {
                    const dvec3 g = geo.ground(key.face, geo.unit(key.level - 1, parent_x, i),
                                               geo.unit(key.level - 1, parent_y, j));
                    parent[static_cast<std::size_t>(j) * stride + i] = geo.position(g, geo.sample(g));
                }
            auto at = [&](int i, int j) { return parent[static_cast<std::size_t>(j) * stride + i]; };
            for (int j = 0; j < stride; ++j)
                for (int i = 0; i < stride; ++i)
                {
                    // A child vertex sits on a parent vertex, on a parent edge midpoint, or at a parent quad centre.
                    const int si = i + cells * (key.x & 1), sj = j + cells * (key.y & 1);
                    const int pi = si / 2, pj = sj / 2;
                    const bool odd_i = (si & 1) != 0, odd_j = (sj & 1) != 0;
                    dvec3 target;
                    if (!odd_i && !odd_j)
                        target = at(pi, pj);
                    else if (odd_i && !odd_j)
                        target = 0.5 * (at(pi, pj) + at(pi + 1, pj));
                    else if (!odd_i && odd_j)
                        target = 0.5 * (at(pi, pj) + at(pi, pj + 1));
                    else
                        target = 0.5 * (at(pi + 1, pj) + at(pi, pj + 1));
                    const std::size_t index = static_cast<std::size_t>(j) * stride + i;
                    const dvec3 delta = target - position[index];
                    data.vertices[index].tangent[0] = static_cast<float>(delta.x);
                    data.vertices[index].tangent[1] = static_cast<float>(delta.y);
                    data.vertices[index].tangent[2] = static_cast<float>(delta.z);
                }
        }

        data.indices.reserve(static_cast<std::size_t>(cells) * cells * 6 + static_cast<std::size_t>(cells) * 4 * 6);
        auto grid = [&](int i, int j) { return static_cast<std::uint32_t>(j * stride + i); };
        for (int j = 0; j < cells; ++j)
            for (int i = 0; i < cells; ++i)
            {
                const std::uint32_t a = grid(i, j), b = grid(i + 1, j), c = grid(i, j + 1), d = grid(i + 1, j + 1);
                data.indices.insert(data.indices.end(), {a, c, b, b, c, d});
            }

        // Skirts: a lowered copy of each border vertex. Each edge runs in the direction that faces the quad outward.
        std::vector<std::uint32_t> border;
        for (int i = 0; i < stride; ++i)
            border.push_back(grid(i, 0));
        for (int j = 1; j < stride; ++j)
            border.push_back(grid(cells, j));
        for (int i = cells - 1; i >= 0; --i)
            border.push_back(grid(i, cells));
        for (int j = cells - 1; j >= 1; --j)
            border.push_back(grid(0, j));
        std::vector<std::uint32_t> skirt(border.size());
        for (std::size_t k = 0; k < border.size(); ++k)
        {
            Vertex3D vertex = data.vertices[border[k]];
            const std::size_t index = border[k];
            const dvec3 drop = geo.up(ground[index]) * depth;
            vertex.position[0] -= static_cast<float>(drop.x);
            vertex.position[1] -= static_cast<float>(drop.y);
            vertex.position[2] -= static_cast<float>(drop.z);
            skirt[k] = static_cast<std::uint32_t>(data.vertices.size());
            data.vertices.push_back(vertex);
        }
        for (std::size_t k = 0; k < border.size(); ++k)
        {
            const std::size_t next = (k + 1) % border.size();
            const std::uint32_t p = border[k], q = border[next], sp = skirt[k], sq = skirt[next];
            data.indices.insert(data.indices.end(), {p, q, sq, p, sq, sp});
        }

        double radius_squared = 0.0;
        for (const Vertex3D &vertex : data.vertices)
        {
            const double x = vertex.position[0], y = vertex.position[1], z = vertex.position[2];
            radius_squared = std::max(radius_squared, x * x + y * y + z * z);
        }
        data.bounding_radius = std::sqrt(radius_squared);
        return data;
    }

    // ---- Terrain ----

    namespace
    {
        struct Frustum
        {
            dvec3 camera{0.0};
            dvec3 forward{0.0, 0.0, -1.0};
            dvec3 planes[4];
            double far_distance = 1.0e30;
            /** Pixels per metre at one metre distance for a one-metre error. */
            double pixel_scale = 1.0;
            bool test(const dvec3 &centre, double radius) const
            {
                const dvec3 offset = centre - camera;
                for (const dvec3 &plane : planes)
                    if (glm::dot(plane, offset) < -radius)
                        return false;
                return glm::dot(forward, offset) - radius <= far_distance;
            }
        };

        struct Pending
        {
            double priority = 0.0;
            unsigned version = 0;
            std::uint64_t last_wanted = 0;
        };

        struct Result
        {
            TerrainNodeKey key;
            unsigned version = 0;
            TerrainChunkData data;
        };
    } // namespace

    namespace
    {
        // Lays out a 2x2 layer atlas with its mip chain in one texture, because the backends only guarantee
        // clamped, unmipmapped samplers. Level 0 is the 2x2 atlas at the origin; each coarser level is a 2x2 atlas stacked
        // down a column to its right. Every tile has a one texel wrapped border so bilinear filtering repeats seamlessly.
        // The shader mirrors this layout (tile size S>>level, cell size +2).
        struct Pyramid
        {
            Bitmap *bitmap = nullptr;
            int levels = 0;
            int layer_size = 0;
        };

        Pyramid build_pyramid(Bitmap *source)
        {
            Pyramid result;
            if (!source || source->width < 8 || source->width != source->height || (source->width & 1))
                return result;
            if (source->pixels.empty())
                download_bitmap(source);
            const std::size_t needed = static_cast<std::size_t>(source->width) * source->height * 4;
            if (source->pixels.size() < needed)
                return result;
            const int base = source->width / 2;
            int levels = 1;
            while (levels < 8 && (base >> levels) >= 2)
                ++levels;

            // tiles[level][layer] is a size x size RGBA tile.
            std::vector<std::array<std::vector<Uint8>, 4>> tiles(static_cast<std::size_t>(levels));
            for (int layer = 0; layer < 4; ++layer)
            {
                auto &tile = tiles[0][static_cast<std::size_t>(layer)];
                tile.resize(static_cast<std::size_t>(base) * base * 4);
                for (int y = 0; y < base; ++y)
                    for (int x = 0; x < base; ++x)
                    {
                        const std::size_t from =
                            (static_cast<std::size_t>((layer >> 1) * base + y) * source->width + (layer & 1) * base + x) * 4;
                        std::copy_n(&source->pixels[from], 4, &tile[(static_cast<std::size_t>(y) * base + x) * 4]);
                    }
            }
            for (int level = 1; level < levels; ++level)
            {
                const int from_size = base >> (level - 1);
                const int size = base >> level;
                for (int layer = 0; layer < 4; ++layer)
                {
                    const auto &from = tiles[static_cast<std::size_t>(level - 1)][static_cast<std::size_t>(layer)];
                    auto &to = tiles[static_cast<std::size_t>(level)][static_cast<std::size_t>(layer)];
                    to.resize(static_cast<std::size_t>(size) * size * 4);
                    for (int y = 0; y < size; ++y)
                        for (int x = 0; x < size; ++x)
                            for (int c = 0; c < 4; ++c)
                            {
                                int sum = 0;
                                for (int dy = 0; dy < 2; ++dy)
                                    for (int dx = 0; dx < 2; ++dx)
                                        sum += from[(static_cast<std::size_t>((2 * y + dy) % from_size) * from_size +
                                                     (2 * x + dx) % from_size) * 4 + c];
                                to[(static_cast<std::size_t>(y) * size + x) * 4 + c] = static_cast<Uint8>((sum + 2) / 4);
                            }
                }
            }

            const int cell0 = base + 2;
            int width = 2 * cell0;
            int column_height = 0;
            if (levels > 1)
            {
                width += 2 * ((base >> 1) + 2);
                for (int level = 1; level < levels; ++level)
                    column_height += 2 * ((base >> level) + 2);
            }
            const int height = std::max(2 * cell0, column_height);
            Bitmap *bitmap = create_bitmap(width, height);
            if (!bitmap)
                return result;
            bitmap->smooth = true;
            if (bitmap->pixels.size() < static_cast<std::size_t>(width) * height * 4)
                bitmap->pixels.resize(static_cast<std::size_t>(width) * height * 4);
            std::fill(bitmap->pixels.begin(), bitmap->pixels.end(), static_cast<Uint8>(0));

            int column_y = 0;
            for (int level = 0; level < levels; ++level)
            {
                const int size = base >> level;
                const int cell = size + 2;
                const int origin_x = level == 0 ? 0 : 2 * cell0;
                const int origin_y = level == 0 ? 0 : column_y;
                for (int layer = 0; layer < 4; ++layer)
                {
                    const auto &tile = tiles[static_cast<std::size_t>(level)][static_cast<std::size_t>(layer)];
                    const int cell_x = origin_x + (layer & 1) * cell;
                    const int cell_y = origin_y + (layer >> 1) * cell;
                    for (int y = -1; y <= size; ++y)
                        for (int x = -1; x <= size; ++x)
                        {
                            const int wrapped_x = (x + size) % size;
                            const int wrapped_y = (y + size) % size;
                            std::copy_n(&tile[(static_cast<std::size_t>(wrapped_y) * size + wrapped_x) * 4], 4,
                                        &bitmap->pixels[(static_cast<std::size_t>(cell_y + y + 1) * width + cell_x + x + 1) * 4]);
                        }
                }
                if (level > 0)
                    column_y += 2 * cell;
            }
            bitmap->ram_dirty = true;
            result.bitmap = bitmap;
            result.levels = levels;
            result.layer_size = base;
            return result;
        }
    }

    struct Terrain::Impl
    {
        struct Node
        {
            Mesh *mesh = nullptr;
            bool queued = false;
            bool stale = false;
            unsigned version = 0;
            double min_height = 0.0;
            double max_height = 0.0;
            dvec3 center{0.0};
            double radius = 0.0;
            std::uint64_t last_used = 0;
            std::uint64_t retry_after = 0;
            bool split = false;
            int triangles = 0;
        };

        Impl(const TerrainConfig &c, std::shared_ptr<TerrainSource> s) : config(c), source(std::move(s)), geo(config, *source)
        {
            if (config.worker_threads < 0)
            {
                const unsigned hardware = std::thread::hardware_concurrency();
                thread_count = static_cast<int>(std::clamp<unsigned>(hardware > 1 ? hardware - 1 : 1, 1, 4));
            }
            else
                thread_count = std::min(config.worker_threads, 16);
            for (int i = 0; i < thread_count; ++i)
                workers.emplace_back([this] { worker_loop(); });
        }

        ~Impl()
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                stop = true;
            }
            condition.notify_all();
            for (std::thread &worker : workers)
                worker.join();
            for (auto &entry : nodes)
                if (entry.second.mesh)
                    destroy_mesh(entry.second.mesh);
            if (atlas.bitmap)
                destroy_bitmap(atlas.bitmap);
            if (normal_atlas.bitmap)
                destroy_bitmap(normal_atlas.bitmap);
        }

        TerrainConfig config;
        std::shared_ptr<TerrainSource> source;
        Geometry geo;

        std::unordered_map<TerrainNodeKey, Node> nodes;
        std::vector<TerrainNodeKey> visible;
        std::uint64_t frame = 0;
        TerrainStats stats;

        Pyramid atlas;
        Pyramid normal_atlas;
        std::atomic<bool> atlas_in_use{false};
        double fade_start = 0.0;
        double fade_end = 0.0;
        dvec3 view_position{0.0};
        double pixel_scale = 1.0;

        int thread_count = 0;
        std::vector<std::thread> workers;
        std::mutex mutex;
        std::condition_variable condition;
        std::unordered_map<TerrainNodeKey, Pending> pending;
        std::vector<Result> done;
        bool stop = false;
        int in_flight = 0;

        TerrainChunkData build_chunk(const TerrainNodeKey &key) const
        {
            TerrainConfig local = config;
            local.weights_in_vertex_colour = config.weights_in_vertex_colour || atlas_in_use.load();
            return build_terrain_chunk(local, *source, key);
        }

        void worker_loop()
        {
            for (;;)
            {
                TerrainNodeKey key;
                unsigned version = 0;
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    condition.wait(lock, [this] { return stop || (!pending.empty() && done.size() < 16); });
                    if (stop)
                        return;
                    auto best = pending.begin();
                    for (auto it = pending.begin(); it != pending.end(); ++it)
                        if (it->second.priority < best->second.priority)
                            best = it;
                    key = best->first;
                    version = best->second.version;
                    pending.erase(best);
                    ++in_flight;
                }
                Result result;
                result.key = key;
                result.version = version;
                result.data = build_chunk(key);
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    done.push_back(std::move(result));
                    --in_flight;
                }
            }
        }

        void request(const TerrainNodeKey &key, double priority, Node &node)
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (node.queued)
            {
                auto it = pending.find(key);
                if (it != pending.end())
                {
                    it->second.priority = priority;
                    it->second.last_wanted = frame;
                }
                return;
            }
            if (frame < node.retry_after)
                return;
            node.queued = true;
            pending[key] = {priority, node.version, frame};
            condition.notify_one();
        }

        // Bounds of a node that has no mesh yet, from the height range of its nearest built ancestor.
        void estimate_bounds(const TerrainNodeKey &key, double hmin, double hmax, dvec3 &centre, double &radius) const
        {
            if (!geo.sphere)
            {
                const double size = geo.chunk_size(key.level);
                const double x0 = config.origin.x + key.x * size;
                const double z0 = config.origin.z + key.y * size;
                centre = {x0 + size * 0.5, config.origin.y + 0.5 * (hmin + hmax), z0 + size * 0.5};
                radius = 0.5 * std::sqrt(2.0 * size * size + (hmax - hmin) * (hmax - hmin));
                return;
            }
            dvec3 sum(0.0);
            dvec3 points[50];
            int count = 0;
            for (int j = 0; j <= 4; ++j)
                for (int i = 0; i <= 4; ++i)
                {
                    const dvec3 g = geo.ground(key.face, std::ldexp(key.x + i * 0.25, -key.level),
                                               std::ldexp(key.y + j * 0.25, -key.level));
                    points[count++] = geo.position(g, hmin);
                    points[count++] = geo.position(g, hmax);
                }
            for (int k = 0; k < count; ++k)
                sum += points[k];
            centre = sum / static_cast<double>(count);
            double r2 = 0.0;
            for (int k = 0; k < count; ++k)
                r2 = std::max(r2, glm::dot(points[k] - centre, points[k] - centre));
            radius = std::sqrt(r2);
        }

        bool occluded(const Frustum &frustum, const dvec3 &centre, double radius) const
        {
            if (!geo.sphere)
                return false;
            const dvec3 e = frustum.camera - config.origin;
            const double d = glm::length(e);
            const double occluder = config.radius + source->min_height();
            if (d <= occluder)
                return false;
            const dvec3 c = centre - config.origin;
            const double cl = glm::length(c);
            if (cl <= radius)
                return false;
            const double angle = std::acos(std::clamp(glm::dot(e, c) / (d * cl), -1.0, 1.0));
            const double angular_radius = std::asin(std::min(1.0, radius / cl));
            const double camera_horizon = std::acos(std::min(1.0, occluder / d));
            const double node_horizon = std::acos(std::min(1.0, occluder / (cl + radius)));
            return angle - angular_radius > camera_horizon + node_horizon;
        }

        void visit(const TerrainNodeKey &key, double hmin, double hmax, const Frustum &frustum, bool interactive,
                   std::vector<TerrainNodeKey> *ideal)
        {
            Node *node = nullptr;
            if (interactive)
                node = &nodes[key];
            else
            {
                const auto it = nodes.find(key);
                if (it != nodes.end())
                    node = &it->second;
            }
            const bool built = node && node->mesh;
            dvec3 centre;
            double radius;
            if (built)
            {
                centre = node->center;
                radius = node->radius;
            }
            else
                estimate_bounds(key, hmin, hmax, centre, radius);
            if (!frustum.test(centre, radius) || occluded(frustum, centre, radius))
                return;
            if (interactive)
                node->last_used = frame;

            const double distance = std::max(glm::length(centre - frustum.camera) - radius, 0.1);
            const double cell = geo.cell_size(key.level);
            const double error = cell * config.error_scale * frustum.pixel_scale / distance;
            const bool was_split = interactive && node->split;
            const bool wants_split = key.level < geo.max_level && error > config.max_screen_error * (was_split ? 0.8 : 1.0);
            const double child_min = built ? node->min_height - cell : hmin;
            const double child_max = built ? node->max_height + cell : hmax;

            auto emit = [&]()
            {
                if (!interactive)
                {
                    ideal->push_back(key);
                    return;
                }
                node->split = false;
                if (!built || node->stale)
                    request(key, distance, *node);
                visible.push_back(key);
            };
            if (!wants_split)
            {
                emit();
                return;
            }
            TerrainNodeKey children[4];
            for (int k = 0; k < 4; ++k)
                children[k] = {key.face, key.level + 1, key.x * 2 + (k & 1), key.y * 2 + (k >> 1)};
            if (!interactive)
            {
                for (const TerrainNodeKey &child : children)
                    visit(child, child_min, child_max, frustum, false, ideal);
                return;
            }
            bool all_ready = true;
            for (const TerrainNodeKey &child : children)
            {
                Node &child_node = nodes[child];
                if (!child_node.mesh)
                {
                    all_ready = false;
                    request(child, distance, child_node);
                }
            }
            if (!all_ready)
            {
                emit();
                return;
            }
            node->split = true;
            for (const TerrainNodeKey &child : children)
                visit(child, child_min, child_max, frustum, true, nullptr);
        }

        Frustum make_frustum(const TerrainView &view) const
        {
            Frustum frustum;
            frustum.camera = view.position;
            dvec3 forward = glm::length(view.forward) > 0.0 ? glm::normalize(view.forward) : dvec3(0.0, 0.0, -1.0);
            dvec3 up = view.up - forward * glm::dot(view.up, forward);
            up = glm::length(up) > 1.0e-9 ? glm::normalize(up) : dvec3(0.0, 1.0, 0.0);
            const dvec3 right = glm::cross(forward, up);
            const double tan_y = std::tan(glm::radians(std::clamp(view.fov_y_degrees, 1.0, 170.0)) * 0.5);
            const double tan_x = tan_y * std::max(view.aspect, 0.01);
            frustum.forward = forward;
            frustum.planes[0] = glm::normalize(right + forward * tan_x);
            frustum.planes[1] = glm::normalize(-right + forward * tan_x);
            frustum.planes[2] = glm::normalize(up + forward * tan_y);
            frustum.planes[3] = glm::normalize(-up + forward * tan_y);
            frustum.far_distance = view.far_distance;
            frustum.pixel_scale = std::max(view.viewport_height, 1.0) / (2.0 * tan_y);
            return frustum;
        }

        std::vector<TerrainNodeKey> roots() const
        {
            std::vector<TerrainNodeKey> keys;
            const int count = geo.sphere ? 6 : 1;
            for (int face = 0; face < count; ++face)
                keys.push_back({face, 0, 0, 0});
            return keys;
        }

        void invalidate_node(Node &node)
        {
            node.stale = true;
            ++node.version;
        }

        void invalidate_all()
        {
            for (auto &entry : nodes)
                invalidate_node(entry.second);
        }

        void invalidate_region(double x0, double z0, double x1, double z1)
        {
            if (geo.sphere)
            {
                invalidate_all();
                return;
            }
            for (auto &entry : nodes)
            {
                const TerrainNodeKey &key = entry.first;
                const double size = geo.chunk_size(key.level);
                const double margin = geo.cell_size(key.level) * 2.0;
                const double nx0 = config.origin.x + key.x * size - margin;
                const double nz0 = config.origin.z + key.y * size - margin;
                if (nx0 <= x1 && nx0 + size + 2.0 * margin >= x0 && nz0 <= z1 && nz0 + size + 2.0 * margin >= z0)
                    invalidate_node(entry.second);
            }
        }

        bool upload(Result &result)
        {
            auto it = nodes.find(result.key);
            if (it == nodes.end())
                return false;
            Node &node = it->second;
            if (node.version != result.version)
            {
                node.queued = false;
                return false;
            }
            Mesh *mesh = create_mesh(result.data.vertices, result.data.indices);
            node.queued = false;
            if (!mesh)
            {
                node.retry_after = frame + 120;
                return false;
            }
            if (node.mesh)
                destroy_mesh(node.mesh);
            node.mesh = mesh;
            node.stale = false;
            node.center = result.data.center;
            node.radius = result.data.bounding_radius;
            node.min_height = result.data.min_height;
            node.max_height = result.data.max_height;
            node.triangles = static_cast<int>(result.data.indices.size() / 3);
            return true;
        }

        void process_uploads()
        {
            int budget = std::max(config.max_uploads_per_frame, 1);
            if (thread_count == 0)
            {
                while (budget > 0)
                {
                    TerrainNodeKey key;
                    Pending job;
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        if (pending.empty())
                            break;
                        auto best = pending.begin();
                        for (auto it = pending.begin(); it != pending.end(); ++it)
                            if (it->second.priority < best->second.priority)
                                best = it;
                        key = best->first;
                        job = best->second;
                        pending.erase(best);
                    }
                    Result result;
                    result.key = key;
                    result.version = job.version;
                    result.data = build_chunk(key);
                    if (upload(result))
                        --budget;
                }
                return;
            }
            std::vector<Result> ready;
            {
                std::lock_guard<std::mutex> lock(mutex);
                const std::size_t take = std::min<std::size_t>(done.size(), 64);
                ready.assign(std::make_move_iterator(done.begin()), std::make_move_iterator(done.begin() + take));
                done.erase(done.begin(), done.begin() + take);
            }
            // Anything beyond this frame's budget goes back to the front of the queue.
            std::vector<Result> deferred;
            for (Result &result : ready)
            {
                if (budget <= 0)
                {
                    deferred.push_back(std::move(result));
                    continue;
                }
                if (upload(result))
                    --budget;
            }
            if (!deferred.empty())
            {
                std::lock_guard<std::mutex> lock(mutex);
                done.insert(done.begin(), std::make_move_iterator(deferred.begin()), std::make_move_iterator(deferred.end()));
            }
            condition.notify_all();
        }

        void cancel_unwanted()
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (auto it = pending.begin(); it != pending.end();)
            {
                if (it->second.last_wanted + 3 < frame)
                {
                    auto node = nodes.find(it->first);
                    if (node != nodes.end())
                        node->second.queued = false;
                    it = pending.erase(it);
                }
                else
                    ++it;
            }
        }

        void evict()
        {
            if (static_cast<int>(nodes.size()) <= config.max_cached_chunks)
                return;
            std::vector<std::pair<std::uint64_t, TerrainNodeKey>> candidates;
            for (const auto &entry : nodes)
                if (entry.first.level > 0 && entry.second.last_used + config.evict_after_frames < frame)
                    candidates.emplace_back(entry.second.last_used, entry.first);
            std::sort(candidates.begin(), candidates.end(),
                      [](const auto &a, const auto &b) { return a.first < b.first; });
            const std::size_t target = static_cast<std::size_t>(config.max_cached_chunks * 0.9);
            for (const auto &candidate : candidates)
            {
                if (nodes.size() <= target)
                    break;
                auto it = nodes.find(candidate.second);
                if (it->second.mesh)
                    destroy_mesh(it->second.mesh);
                nodes.erase(it);
            }
        }

        void update(const TerrainView &view)
        {
            ++frame;
            double x0, z0, x1, z1;
            if (source->take_dirty_region(x0, z0, x1, z1))
                invalidate_region(x0, z0, x1, z1);
            const Frustum frustum = make_frustum(view);
            view_position = view.position;
            pixel_scale = frustum.pixel_scale;
            visible.clear();
            const double hmin = source->min_height();
            const double hmax = source->max_height();
            for (const TerrainNodeKey &root : roots())
                visit(root, hmin, hmax, frustum, true, nullptr);
            cancel_unwanted();
            process_uploads();
            evict();

            stats = {};
            for (const TerrainNodeKey &key : visible)
            {
                const auto it = nodes.find(key);
                if (it == nodes.end() || !it->second.mesh)
                    continue;
                ++stats.visible_chunks;
                stats.visible_triangles += it->second.triangles;
                stats.deepest_level = std::max(stats.deepest_level, key.level);
            }
            for (const auto &entry : nodes)
                if (entry.second.mesh)
                    ++stats.resident_chunks;
            std::lock_guard<std::mutex> lock(mutex);
            stats.pending_chunks = static_cast<int>(pending.size()) + in_flight + static_cast<int>(done.size());
        }
    };

    Terrain::Terrain(const TerrainConfig &config, std::shared_ptr<TerrainSource> source)
        : impl_(std::make_unique<Impl>(config, std::move(source)))
    {
    }

    Terrain::~Terrain() = default;

    const TerrainConfig &Terrain::config() const { return impl_->config; }
    const std::shared_ptr<TerrainSource> &Terrain::source() const { return impl_->source; }
    int Terrain::max_level() const { return impl_->geo.max_level; }

    void Terrain::update(const TerrainView &view) { impl_->update(view); }

    bool Terrain::set_layer_atlas(Bitmap *atlas, double fade_start, double fade_end, Bitmap *normal_atlas)
    {
        Impl &impl = *impl_;
        if (impl.atlas.bitmap)
            destroy_bitmap(impl.atlas.bitmap);
        if (impl.normal_atlas.bitmap)
            destroy_bitmap(impl.normal_atlas.bitmap);
        impl.atlas = {};
        impl.normal_atlas = {};
        bool ok = true;
        if (atlas)
        {
            impl.atlas = build_pyramid(atlas);
            ok = impl.atlas.bitmap != nullptr;
            if (ok && normal_atlas)
            {
                impl.normal_atlas = build_pyramid(normal_atlas);
                if (impl.normal_atlas.bitmap && impl.normal_atlas.layer_size != impl.atlas.layer_size)
                {
                    destroy_bitmap(impl.normal_atlas.bitmap);
                    impl.normal_atlas = {};
                }
                ok = impl.normal_atlas.bitmap != nullptr;
            }
        }
        impl.fade_start = fade_start;
        impl.fade_end = fade_end;
        const bool in_use = impl.atlas.bitmap != nullptr;
        if (impl.atlas_in_use.exchange(in_use) != in_use)
            impl.invalidate_all();
        return ok;
    }

    bool Terrain::draw(const dvec3 &origin, const Material &material) const
    {
        const Impl &impl = *impl_;
        Material shaded = material;
        shaded.texture = impl.atlas.bitmap;
        shaded.normal_map = impl.normal_atlas.bitmap;
        shaded.terrain.atlas_levels = impl.atlas.levels;
        shaded.terrain.layer_size = impl.atlas.layer_size;
        shaded.terrain.normal_atlas = impl.normal_atlas.bitmap != nullptr;
        shaded.terrain.enabled = true;
        shaded.terrain.fade_start = static_cast<float>(impl.fade_start);
        shaded.terrain.fade_end = static_cast<float>(impl.fade_end);
        bool ok = true;
        for (const TerrainNodeKey &key : impl.visible)
        {
            const auto it = impl.nodes.find(key);
            if (it == impl.nodes.end() || !it->second.mesh)
                continue;
            const dvec3 centre = it->second.center;
            const glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(centre - origin));
            const dvec3 eye = impl.view_position - centre;
            shaded.terrain.eye[0] = static_cast<float>(eye.x);
            shaded.terrain.eye[1] = static_cast<float>(eye.y);
            shaded.terrain.eye[2] = static_cast<float>(eye.z);
            if (impl.config.geomorph && key.level > 0)
            {
                // A chunk becomes its parent's shape by the distance at which the parent would have been split.
                const double split = 2.0 * impl.geo.cell_size(key.level) * impl.config.error_scale * impl.pixel_scale /
                                     std::max(impl.config.max_screen_error, 1.0e-6);
                shaded.terrain.morph_start = static_cast<float>(0.7 * split);
                shaded.terrain.morph_end = static_cast<float>(split);
            }
            else
            {
                shaded.terrain.morph_start = 1.0e30f;
                shaded.terrain.morph_end = 2.0e30f;
            }
            ok = draw_mesh(it->second.mesh, model, shaded) && ok;
        }
        return ok;
    }

    void Terrain::wait_until_ready(const TerrainView &view, int max_updates)
    {
        for (int i = 0; i < max_updates; ++i)
        {
            impl_->update(view);
            bool complete = true;
            for (const TerrainNodeKey &key : select_nodes(view))
            {
                const auto it = impl_->nodes.find(key);
                if (it == impl_->nodes.end() || !it->second.mesh || it->second.stale)
                {
                    complete = false;
                    break;
                }
            }
            if (complete)
                return;
            if (impl_->thread_count > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    std::vector<TerrainNodeKey> Terrain::select_nodes(const TerrainView &view) const
    {
        std::vector<TerrainNodeKey> keys;
        const Frustum frustum = impl_->make_frustum(view);
        const double hmin = impl_->source->min_height();
        const double hmax = impl_->source->max_height();
        for (const TerrainNodeKey &root : impl_->roots())
            impl_->visit(root, hmin, hmax, frustum, false, &keys);
        return keys;
    }

    TerrainStats Terrain::stats() const { return impl_->stats; }

    double Terrain::height_at(double x, double z) const
    {
        return impl_->config.origin.y + impl_->source->height(x, z);
    }

    dvec3 Terrain::normal_at(double x, double z) const
    {
        return impl_->geo.normal({x, 0.0, z}, std::max(impl_->config.finest_cell_size * 0.5, 1.0e-3));
    }

    double Terrain::height_at_direction(const dvec3 &direction) const
    {
        const double length = glm::length(direction);
        if (length <= 0.0)
            return 0.0;
        return impl_->source->height_on_sphere(direction / length, impl_->config.radius);
    }

    dvec3 Terrain::normal_at_direction(const dvec3 &direction) const
    {
        const double length = glm::length(direction);
        if (length <= 0.0)
            return {0.0, 1.0, 0.0};
        return impl_->geo.normal(direction / length, std::max(impl_->config.finest_cell_size * 0.5, 1.0e-3));
    }

    dvec3 Terrain::surface_point(const dvec3 &direction) const
    {
        const double length = glm::length(direction);
        if (length <= 0.0)
            return impl_->config.origin;
        const dvec3 unit = direction / length;
        return impl_->geo.position(unit, impl_->source->height_on_sphere(unit, impl_->config.radius));
    }

    double Terrain::altitude_of(const dvec3 &position) const
    {
        if (!impl_->geo.sphere)
            return position.y - height_at(position.x, position.z);
        const dvec3 offset = position - impl_->config.origin;
        const double distance = glm::length(offset);
        if (distance <= 0.0)
            return -impl_->config.radius;
        return distance - impl_->config.radius - height_at_direction(offset);
    }

    dvec3 Terrain::normal_below(const dvec3 &position) const
    {
        if (!impl_->geo.sphere)
            return normal_at(position.x, position.z);
        return normal_at_direction(position - impl_->config.origin);
    }

    TerrainHit Terrain::raycast(const dvec3 &origin, const dvec3 &direction, double max_distance) const
    {
        TerrainHit hit;
        const double length = glm::length(direction);
        if (length <= 0.0 || !(max_distance > 0.0))
            return hit;
        const dvec3 dir = direction / length;
        const double minimum_step = std::max(impl_->config.finest_cell_size * 0.5, 1.0e-3);
        auto altitude = [&](double t) { return altitude_of(origin + dir * t); };

        double t = 0.0;
        double previous_t = 0.0;
        double f = altitude(0.0);
        if (f <= 0.0)
        {
            hit.hit = true;
            hit.position = origin;
            hit.normal = normal_below(origin);
            hit.distance = 0.0;
            return hit;
        }
        for (int iteration = 0; iteration < 200000 && t < max_distance; ++iteration)
        {
            // Stepping a fraction of the height above the surface cannot jump over terrain gentler than about 60 degrees.
            const double step = std::max(minimum_step, 0.3 * std::fabs(f));
            previous_t = t;
            t = std::min(t + step, max_distance);
            f = altitude(t);
            if (f <= 0.0)
            {
                double low = previous_t;
                double high = t;
                for (int k = 0; k < 40; ++k)
                {
                    const double mid = 0.5 * (low + high);
                    if (altitude(mid) <= 0.0)
                        high = mid;
                    else
                        low = mid;
                }
                hit.hit = true;
                hit.distance = high;
                hit.position = origin + dir * high;
                hit.normal = normal_below(hit.position);
                return hit;
            }
            if (t >= max_distance)
                break;
        }
        return hit;
    }

    void Terrain::invalidate_region(double x0, double z0, double x1, double z1)
    {
        impl_->invalidate_region(std::min(x0, x1), std::min(z0, z1), std::max(x0, x1), std::max(z0, z1));
    }

    void Terrain::invalidate_all() { impl_->invalidate_all(); }

} // namespace sl

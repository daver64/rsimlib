#include "sl.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <utility>
#include <vector>

using namespace sl;

static int failures = 0;
#define CHECK(cond)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while (0)

static TerrainConfig flat_config()
{
    TerrainConfig c;
    c.size = 4096.0;
    c.chunk_cells = 16;
    c.finest_cell_size = 1.0;
    c.worker_threads = 0;
    return c;
}

static std::shared_ptr<TerrainSource> hills()
{
    return std::make_shared<FunctionSource>([](double x, double z) { return 20.0 * std::sin(x * 0.01) * std::cos(z * 0.013); },
                                            -20.0, 20.0);
}

static void test_lod()
{
    Terrain terrain(flat_config(), hills());
    TerrainView view;
    view.position = {2048.0, 50.0, 2048.0};
    view.forward = {0.0, -0.2, -1.0};
    const auto near_nodes = terrain.select_nodes(view);
    view.position.y = 3000.0;
    view.forward = {0.0, -1.0, 0.0};
    view.up = {0.0, 0.0, -1.0};
    const auto far_nodes = terrain.select_nodes(view);
    CHECK(!near_nodes.empty());
    int near_deepest = 0, far_deepest = 0;
    for (auto &k : near_nodes)
        near_deepest = std::max(near_deepest, k.level);
    for (auto &k : far_nodes)
        far_deepest = std::max(far_deepest, k.level);
    CHECK(near_deepest > far_deepest);
    CHECK(near_deepest <= terrain.max_level());

    // Selected leaves must tile the visible area without overlap.
    double area = 0.0;
    for (auto &k : near_nodes)
        area += std::ldexp(1.0, -2 * k.level);
    CHECK(area <= 1.0 + 1e-9);

    // Higher error tolerance gives fewer nodes.
    TerrainConfig loose = flat_config();
    loose.max_screen_error = 20.0;
    Terrain coarse(loose, hills());
    view.position = {2048.0, 50.0, 2048.0};
    view.forward = {0.0, -0.2, -1.0};
    view.up = {0.0, 1.0, 0.0};
    CHECK(coarse.select_nodes(view).size() < near_nodes.size());
}

static void test_flat_chunks()
{
    TerrainConfig c = flat_config();
    auto source = hills();
    TerrainNodeKey a{0, 3, 2, 2}, b{0, 3, 3, 2};
    TerrainChunkData da = build_terrain_chunk(c, *source, a);
    TerrainChunkData db = build_terrain_chunk(c, *source, b);
    const int s = da.grid_vertices;
    CHECK(s == 17);
    // The shared border must agree exactly in world space.
    for (int j = 0; j < s; ++j)
    {
        const Vertex3D &va = da.vertices[j * s + (s - 1)];
        const Vertex3D &vb = db.vertices[j * s];
        for (int k = 0; k < 3; ++k)
        {
            const double pa = va.position[k] + da.center[k];
            const double pb = vb.position[k] + db.center[k];
            CHECK(std::fabs(pa - pb) < 1e-3);
        }
    }
    // Deterministic.
    TerrainChunkData again = build_terrain_chunk(c, *source, a);
    CHECK(again.vertices.size() == da.vertices.size());
    CHECK(std::memcmp(again.vertices.data(), da.vertices.data(), da.vertices.size() * sizeof(Vertex3D)) == 0);
    // Upward facing triangles on flat ground.
    int bad = 0;
    for (size_t i = 0; i + 2 < static_cast<size_t>(s - 1) * (s - 1) * 6; i += 3)
    {
        const Vertex3D &p = da.vertices[da.indices[i]];
        const Vertex3D &q = da.vertices[da.indices[i + 1]];
        const Vertex3D &r = da.vertices[da.indices[i + 2]];
        glm::vec3 e1(q.position[0] - p.position[0], q.position[1] - p.position[1], q.position[2] - p.position[2]);
        glm::vec3 e2(r.position[0] - p.position[0], r.position[1] - p.position[1], r.position[2] - p.position[2]);
        if (glm::cross(e1, e2).y <= 0.0f)
            ++bad;
    }
    CHECK(bad == 0);
}

static void test_geomorph()
{
    TerrainConfig c = flat_config();
    auto source = hills();
    const TerrainNodeKey parent_key{0, 3, 2, 2};
    const TerrainChunkData parent = build_terrain_chunk(c, *source, parent_key);
    const int s = parent.grid_vertices;
    const double parent_cell = c.size / 8.0 / (s - 1);
    int bad = 0, nonzero = 0;
    for (int cx = 0; cx < 2; ++cx)
        for (int cy = 0; cy < 2; ++cy)
        {
            const TerrainChunkData child = build_terrain_chunk(c, *source, {0, 4, 4 + cx, 4 + cy});
            for (int k = 0; k < s * s; ++k)
            {
                const Vertex3D &v = child.vertices[k];
                CHECK(v.tangent[3] == 1.0f);
                const double x = v.position[0] + child.center.x, z = v.position[2] + child.center.z;
                // Parent surface at (x, z), interpolating its triangles (a,c,b) and (b,c,d).
                const double lx = (x - (c.origin.x + parent_key.x * parent_cell * (s - 1))) / parent_cell;
                const double lz = (z - (c.origin.z + parent_key.y * parent_cell * (s - 1))) / parent_cell;
                const int i = std::min(static_cast<int>(std::floor(lx + 1e-9)), s - 2);
                const int j = std::min(static_cast<int>(std::floor(lz + 1e-9)), s - 2);
                const double fx = lx - i, fz = lz - j;
                auto h = [&](int a, int b) { return parent.vertices[b * s + a].position[1] + parent.center.y; };
                const double expected = fx + fz <= 1.0 ? h(i, j) + fx * (h(i + 1, j) - h(i, j)) + fz * (h(i, j + 1) - h(i, j))
                                                       : h(i + 1, j + 1) + (1 - fx) * (h(i, j + 1) - h(i + 1, j + 1)) +
                                                             (1 - fz) * (h(i + 1, j) - h(i + 1, j + 1));
                nonzero += std::fabs(v.tangent[1]) > 1e-4f;
                const double morphed = v.position[1] + child.center.y + v.tangent[1];
                if (std::fabs(morphed - expected) > 2e-3 || std::fabs(v.tangent[0]) > 1e-3 || std::fabs(v.tangent[2]) > 1e-3)
                    ++bad;
            }
        }
    CHECK(bad == 0);
    CHECK(nonzero > 0);
    // Without geomorph the offsets are zero.
    c.geomorph = false;
    const TerrainChunkData flat_child = build_terrain_chunk(c, *source, {0, 4, 4, 4});
    CHECK(flat_child.vertices[5].tangent[1] == 0.0f && flat_child.vertices[5].tangent[3] == 1.0f);
}

static void test_queries()
{
    Terrain terrain(flat_config(), hills());
    CHECK(std::fabs(terrain.height_at(100.0, 200.0) - 20.0 * std::sin(1.0) * std::cos(2.6)) < 1e-9);
    const dvec3 n = terrain.normal_at(300.0, 300.0);
    CHECK(std::fabs(glm::length(n) - 1.0) < 1e-9);
    CHECK(n.y > 0.9);
    const TerrainHit hit = terrain.raycast({500.0, 200.0, 500.0}, {0.0, -1.0, 0.0}, 1000.0);
    CHECK(hit.hit);
    CHECK(std::fabs(hit.position.y - terrain.height_at(500.0, 500.0)) < 1e-3);
    const TerrainHit miss = terrain.raycast({500.0, 200.0, 500.0}, {0.0, 1.0, 0.0}, 1000.0);
    CHECK(!miss.hit);
    CHECK(terrain.altitude_of({500.0, terrain.height_at(500.0, 500.0) + 7.0, 500.0}) > 6.999);
}

static void test_sphere()
{
    TerrainConfig c;
    c.shape = TerrainShape::sphere;
    c.radius = 1.0e5;
    c.chunk_cells = 16;
    c.finest_cell_size = 4.0;
    c.worker_threads = 0;
    auto source = std::make_shared<FunctionSource>([](double, double) { return 0.0; }, 0.0, 100.0,
                                                   [](const dvec3 &d, double) { return 50.0 * d.y + 10.0 * d.x * d.z; });
    Terrain terrain(c, source);

    // Adjacent faces share bit-identical edge vertices.
    TerrainNodeKey px{0, 2, 3, 1}, pz{4, 2, 3, 1};
    (void)pz;
    const int s = c.chunk_cells + 1;
    std::vector<std::pair<int, dvec3>> points;
    for (int face = 0; face < 6; ++face)
    {
        TerrainChunkData d = build_terrain_chunk(c, *source, {face, 0, 0, 0});
        for (int k = 0; k < s * s; ++k)
        {
            const Vertex3D &v = d.vertices[k];
            points.push_back({face, dvec3(v.position[0], v.position[1], v.position[2]) + d.center});
        }
    }
    int shared = 0;
    for (size_t i = 0; i < points.size(); ++i)
        for (size_t j = 0; j < points.size(); ++j)
            if (points[i].first != points[j].first && glm::length(points[i].second - points[j].second) < 0.05)
            {
                ++shared;
                break;
            }
    // 12 cube edges of 17 vertices, each shared by two faces (corners shared by three).
    CHECK(shared >= 12 * (s - 2) * 2);

    const dvec3 dir = glm::normalize(dvec3(1.0, 2.0, 3.0));
    const dvec3 point = terrain.surface_point(dir);
    CHECK(std::fabs(glm::length(point) - (c.radius + terrain.height_at_direction(dir))) < 1e-6);
    CHECK(std::fabs(terrain.altitude_of(point * (1.0 + 20.0 / glm::length(point))) - 20.0) < 1e-3);
    CHECK(glm::dot(terrain.normal_at_direction(dir), dir) > 0.9);

    TerrainView view;
    view.position = {0.0, c.radius + 500.0, 0.0};
    view.forward = {0.0, -1.0, 0.0};
    view.up = {0.0, 0.0, -1.0};
    const auto nodes = terrain.select_nodes(view);
    CHECK(!nodes.empty());
    bool has_deep = false;
    for (auto &k : nodes)
        has_deep |= k.level >= 3;
    CHECK(has_deep);
    // Horizon culling: from above one pole, nothing on the far side of the planet is selected.
    for (auto &k : nodes)
        CHECK(k.face != 3);
}

static void test_grid()
{
    auto grid = std::make_shared<GridSource>(65, 65, 64.0, 64.0, 0.0, 0.0, 0.0f);
    CHECK(grid->height(32.0, 32.0) == 0.0);
    grid->raise(32.0, 32.0, 10.0, 5.0);
    CHECK(grid->height(32.0, 32.0) > 4.9);
    CHECK(grid->height(60.0, 60.0) == 0.0);
    double x0, z0, x1, z1;
    CHECK(grid->take_dirty_region(x0, z0, x1, z1));
    CHECK(x0 < 32.0 && x1 > 32.0);
    CHECK(!grid->take_dirty_region(x0, z0, x1, z1));
    grid->flatten(32.0, 32.0, 10.0, 1.0, 1.0);
    CHECK(std::fabs(grid->height(32.0, 32.0) - 1.0) < 1e-6);
}

static void test_noise()
{
    NoiseParameters p;
    p.seed = 7;
    NoiseSource a(p), b(p);
    CHECK(a.height(12.5, 99.0) == b.height(12.5, 99.0));
    CHECK(a.height(12.5, 99.0) >= a.min_height() && a.height(12.5, 99.0) <= a.max_height());
}

static void test_flight_integration()
{
    Terrain flat(flat_config(), hills());
    FlightWorld world;
    world.flat_ground = true;
    world.flat_terrain = &flat;
    const double h = flat.height_at(500.0, 700.0);
    CHECK(std::fabs(world.altitude_of({500.0, h + 12.0, 700.0}) - 12.0) < 1e-9);

    TerrainConfig c;
    c.shape = TerrainShape::sphere;
    c.radius = 1.0e5;
    c.worker_threads = 0;
    Terrain planet(c, std::make_shared<FunctionSource>([](double, double) { return 0.0; }, 0.0, 100.0,
                                                       [](const dvec3 &d, double) { return 100.0 * d.y; }));
    FlightWorld space;
    const int id = space.create_body("planet");
    FlightBody *body = space.body(id);
    body->mode = FlightBodyMode::kinematic;
    body->radius = c.radius;
    body->terrain = &planet;
    body->position = dvec3(5000.0, 0.0, 0.0);
    const dvec3 up(0.0, 1.0, 0.0);
    const dvec3 point = body->position + up * (c.radius + 100.0 + 30.0);
    CHECK(std::fabs(space.altitude_of(point) - 30.0) < 1e-6);
}

int main()
{
    test_lod();
    test_flat_chunks();
    test_geomorph();
    test_queries();
    test_sphere();
    test_grid();
    test_noise();
    test_flight_integration();
    if (failures)
    {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("terrain tests passed\n");
    return 0;
}

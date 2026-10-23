#ifdef NDEBUG
#undef NDEBUG
#endif
#include "sl.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace
{
    struct Part
    {
        const char *name;
        std::function<sl::MeshData(std::uint32_t)> build;
        float max_extent; // longest allowed dimension, in metres
    };

    void check(const char *name, const sl::MeshData &data, float max_extent)
    {
        assert(!data.vertices.empty());
        assert(data.indices.size() % 3 == 0);
        assert(data.indices.size() > 0 && data.indices.size() < 12000);
        glm::vec3 lo{1e9f}, hi{-1e9f};
        for (const sl::Vertex3D &v : data.vertices)
        {
            const glm::vec3 p{v.position[0], v.position[1], v.position[2]};
            const glm::vec3 n{v.normal[0], v.normal[1], v.normal[2]};
            assert(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
            assert(std::abs(glm::length(n) - 1.0f) < 1e-3f);
            lo = glm::min(lo, p);
            hi = glm::max(hi, p);
        }
        for (const std::uint32_t index : data.indices)
            assert(index < data.vertices.size());
        // Stands on the ground and is no larger than its parameters imply.
        if (lo.y < -1e-3f || lo.y > 0.05f || std::max({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z}) > max_extent)
        {
            std::fprintf(stderr, "%s: min y %.3f extent %.2f %.2f %.2f\n", name, lo.y, hi.x - lo.x, hi.y - lo.y, hi.z - lo.z);
            assert(false);
        }
        // Centred on x and z (within the part's own size).
        assert(std::abs(0.5f * (lo.x + hi.x)) < 0.45f * (hi.x - lo.x) + 1.0f);
        assert(std::abs(0.5f * (lo.z + hi.z)) < 0.45f * (hi.z - lo.z) + 1.0f);
    }

    bool same(const sl::MeshData &a, const sl::MeshData &b)
    {
        if (a.vertices.size() != b.vertices.size() || a.indices != b.indices)
            return false;
        for (std::size_t i = 0; i < a.vertices.size(); ++i)
            for (int k = 0; k < 3; ++k)
                if (a.vertices[i].position[k] != b.vertices[i].position[k] || a.vertices[i].colour[k] != b.vertices[i].colour[k])
                    return false;
        return true;
    }
} // namespace

int main()
{
    const std::vector<Part> parts = {
        {"house", [](std::uint32_t s) { return sl::make_house_data(6, 5, 3, {214, 196, 160, 255}, {150, 64, 48, 255}, s); }, 12.0f},
        {"office", [](std::uint32_t s) { return sl::make_office_data(14, 10, 4, {170, 172, 176, 255}, s); }, 20.0f},
        {"skyscraper", [](std::uint32_t s) { return sl::make_skyscraper_data(14, 30, {66, 96, 128, 255}, s); }, 140.0f},
        {"hangar", [](std::uint32_t) { return sl::make_hangar_data(); }, 32.0f},
        {"wall", [](std::uint32_t) { return sl::make_wall_data(); }, 7.0f},
        {"fence", [](std::uint32_t) { return sl::make_fence_data(); }, 7.0f},
        {"water tower", [](std::uint32_t) { return sl::make_water_tower_data(); }, 20.0f},
        {"radar", [](std::uint32_t) { return sl::make_radar_data(); }, 12.0f},
        {"domed radar", [](std::uint32_t) { return sl::make_domed_radar_data(); }, 12.0f},
        {"control tower", [](std::uint32_t s) { return sl::make_control_tower_data(24, s); }, 30.0f},
        {"street lamp", [](std::uint32_t) { return sl::make_street_lamp_data(); }, 8.0f},
        {"road", [](std::uint32_t) { return sl::make_road_data(); }, 12.0f},
        {"barrel", [](std::uint32_t) { return sl::make_barrel_data(); }, 2.0f},
        {"crate", [](std::uint32_t) { return sl::make_crate_data(); }, 2.0f},
        {"container", [](std::uint32_t s) { return sl::make_container_data(6.1f, 2.44f, 2.6f, {160, 52, 42, 255}, s); }, 8.0f},
        {"tank", [](std::uint32_t) { return sl::make_tank_data(); }, 12.0f},
        {"tree", [](std::uint32_t s) { return sl::make_tree_data(6, 2, {104, 74, 48, 255}, {64, 134, 58, 255}, s); }, 8.0f},
        {"bush", [](std::uint32_t s) { return sl::make_bush_data(0.8f, {58, 118, 52, 255}, s); }, 3.0f},
        {"rock cluster", [](std::uint32_t s) { return sl::make_rock_cluster_data(1.5f, {120, 118, 112, 255}, s); }, 6.0f},
        {"car", [](std::uint32_t s) { return sl::make_car_data(4.4f, {190, 44, 40, 255}, s); }, 5.5f},
        {"spaceship", [](std::uint32_t) { return sl::make_spaceship_data(); }, 12.0f},
        {"aeroplane", [](std::uint32_t) { return sl::make_aeroplane_data(); }, 14.0f},
    };

    for (const Part &part : parts)
    {
        const sl::MeshData a = part.build(1), again = part.build(1);
        check(part.name, a, part.max_extent);
        assert(same(a, again));
        check(part.name, part.build(7), part.max_extent);
    }

    // Seeds vary the parts that document it.
    assert(!same(sl::make_house_data(6, 5, 3, {214, 196, 160, 255}, {150, 64, 48, 255}, 1), sl::make_house_data(6, 5, 3, {214, 196, 160, 255}, {150, 64, 48, 255}, 2)));
    assert(!same(sl::make_rock_cluster_data(1.5f, {120, 118, 112, 255}, 1), sl::make_rock_cluster_data(1.5f, {120, 118, 112, 255}, 4)));

    // Size parameters scale the result.
    auto height_of = [](const sl::MeshData &data)
    {
        float top = 0.0f;
        for (const sl::Vertex3D &v : data.vertices)
            top = std::max(top, v.position[1]);
        return top;
    };
    assert(height_of(sl::make_skyscraper_data(14, 40)) > height_of(sl::make_skyscraper_data(14, 20)));
    assert(height_of(sl::make_office_data(14, 10, 6)) > height_of(sl::make_office_data(14, 10, 3)));
    assert(height_of(sl::make_barrel_data(0.4f, 2.0f)) > 1.9f);

    // Parts combine and bake into one mesh.
    sl::MeshData street;
    sl::append_mesh_data(street, sl::make_house_data());
    sl::append_mesh_data(street, sl::make_fence_data());
    assert(street.indices.size() == sl::make_house_data().indices.size() + sl::make_fence_data().indices.size());

    std::printf("props3d_test passed\n");
    return 0;
}

#include "sl.h"

#include <cmath>
#include <cstdio>

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

static Environment3D planet_environment()
{
    Environment3D env;
    env.planet = true;
    env.sun_intensity = 20.0f;
    return env;
}

static void test_atmosphere()
{
    Environment3D env = planet_environment();
    // High sun: the light stays close to white and is brighter than a low sun.
    env.sun_direction = glm::normalize(glm::vec3{0.0f, -1.0f, 0.05f});
    const glm::vec3 noon = atmosphere_sun_colour(env);
    CHECK(noon.b > 0.5f * noon.r);
    // Low sun: blue is scattered away, so the light reddens and dims.
    env.sun_direction = glm::normalize(glm::vec3{1.0f, -0.03f, 0.0f});
    const glm::vec3 dusk = atmosphere_sun_colour(env);
    CHECK(dusk.r > dusk.b);
    CHECK(dusk.r / std::max(dusk.b, 1e-4f) > noon.r / std::max(noon.b, 1e-4f));
    CHECK(dusk.r < noon.r + 1e-3f);

    // Fog transmittance is bounded, monotonic and 1 at zero distance.
    Environment3D flat;
    flat.fog_density = 2e-4f;
    flat.fog_enabled = true;
    float previous = 1.0f;
    CHECK(atmosphere_transmittance(flat, 0.0f).r > 0.999f);
    for (float distance = 100.0f; distance <= 20000.0f; distance *= 2.0f)
    {
        const float t = atmosphere_transmittance(flat, distance).r;
        CHECK(t >= 0.0f && t <= previous + 1e-6f);
        previous = t;
    }
    CHECK(previous < 0.5f);

    // No fog without density, or when disabled.
    flat.fog_density = 0.0f;
    CHECK(atmosphere_transmittance(flat, 5000.0f).r > 0.999f);
    flat.fog_density = 2e-4f;
    flat.fog_enabled = false;
    CHECK(atmosphere_transmittance(flat, 5000.0f).r > 0.999f);

    // The gradient sky of a flat world stays within display range.
    Environment3D sky;
    for (float y : {-0.2f, 0.0f, 0.4f, 1.0f})
    {
        const glm::vec3 c = atmosphere_sky_colour(sky, glm::normalize(glm::vec3{0.3f, y, 0.9f}));
        CHECK(c.r >= 0.0f && c.g >= 0.0f && c.b >= 0.0f);
        CHECK(c.r <= 2.0f && c.g <= 2.0f && c.b <= 2.0f);
    }
}

static bool valid(const MeshData &data)
{
    if (data.vertices.empty() || data.indices.empty() || data.indices.size() % 3 != 0)
        return false;
    for (const std::uint32_t index : data.indices)
        if (index >= data.vertices.size())
            return false;
    return true;
}

static void test_clutter_data()
{
    const MeshData tree = make_conifer_data();
    CHECK(valid(tree));
    float top = 0.0f;
    for (const Vertex3D &v : tree.vertices)
        top = std::max(top, v.position[1]);
    CHECK(std::fabs(top - 8.0f) < 1e-3f);

    const MeshData rock = make_rock_data(2.0f, {120, 120, 120, 255}, 3);
    CHECK(valid(rock));
    for (const Vertex3D &v : rock.vertices)
        CHECK(v.position[1] >= -1e-4f);

    MeshData both = tree;
    append_mesh_data(both, rock);
    CHECK(valid(both));
    CHECK(both.vertices.size() == tree.vertices.size() + rock.vertices.size());
}

static void test_impostor_views()
{
    // Views are chosen from the viewer's compass direction; no GPU is needed for the lookup.
    Impostor impostor;
    impostor.views.assign(8, nullptr);
    CHECK(impostor_view(impostor, {0.0f, 0.0f, 1.0f}) == 0);
    CHECK(impostor_view(impostor, {1.0f, 0.0f, 0.0f}) == 2);
    CHECK(impostor_view(impostor, {0.0f, 0.0f, -1.0f}) == 4);
    CHECK(impostor_view(impostor, {-1.0f, 0.0f, 0.0f}) == 6);
    // Height above the horizon does not change the view, and every direction maps into range.
    CHECK(impostor_view(impostor, glm::normalize(glm::vec3{0.0f, 0.7f, 1.0f})) == 0);
    for (int step = 0; step < 64; ++step)
    {
        const float angle = 0.1f * static_cast<float>(step);
        const int view = impostor_view(impostor, {std::sin(angle), 0.2f, std::cos(angle)});
        CHECK(view >= 0 && view < 8);
    }
    // A tilted up axis (a point on a planet) still gives a valid view.
    const int tilted = impostor_view(impostor, {0.0f, 0.0f, 1.0f}, glm::normalize(glm::vec3{1.0f, 1.0f, 0.0f}));
    CHECK(tilted >= 0 && tilted < 8);
}

int main()
{
    test_atmosphere();
    test_clutter_data();
    test_impostor_views();
    if (failures)
    {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("world flavour tests passed\n");
    return 0;
}

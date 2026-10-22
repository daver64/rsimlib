#undef NDEBUG
#include "sl.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

// Parallel command recording must not change the picture: the same scene rendered with it on and off has to be
// identical pixel for pixel, including blending order, instancing, sky, nested surfaces and MSAA.
// Vulkan and Direct3D 12 record in parallel; OpenGL ignores the setting and must still pass. Run with --gl/--vulkan.
namespace
{
    int failures = 0;

    void check(bool ok, const char *name)
    {
        if (!ok)
        {
            ++failures;
            std::printf("FAIL: %s\n", name);
        }
    }

    constexpr int size = 96;

    // Draw a grid of cubes: opaque, alpha-blended and additive layers interleaved so draw order matters.
    void draw_grid(sl::Mesh *cube, int side, float y, sl::MaterialBlend blend, sl::Colour tint, sl::Bitmap *texture,
                   sl::RenderQueue3D *queue = nullptr)
    {
        for (int i = 0; i < side * side; ++i)
        {
            sl::Material material;
            material.tint = {static_cast<std::uint8_t>((tint.red + i * 7) % 256), tint.green, tint.blue, tint.alpha};
            material.lit = (i % 3) != 0;
            material.blend = blend;
            material.texture = (i % 5) == 0 ? texture : nullptr;
            material.cull = (i % 4) == 0 ? sl::CullMode::none : sl::CullMode::back;
            const glm::mat4 model =
                glm::scale(glm::translate(glm::mat4(1.0f), {(i % side - side / 2) * 1.2f, y, (i / side - side / 2) * 1.2f}),
                           glm::vec3(1.0f, 0.4f, 1.0f));
            if (queue)
                queue->draw_mesh(cube, model, material);
            else
                sl::draw_mesh(cube, model, material);
        }
    }

    std::vector<sl::Colour> render(sl::Surface3D *surface, sl::Mesh *cube, sl::Bitmap *texture, sl::Bitmap *inset)
    {
        sl::Camera camera;
        camera.position = {0.0f, 22.0f, 24.0f};
        camera.target = {0.0f, 0.0f, 0.0f};
        sl::begin_surface_3d(surface, camera);
        sl::clear_surface_3d({20, 30, 50, 255});
        draw_grid(cube, 24, 0.0f, sl::MaterialBlend::opaque, {60, 160, 90, 255}, texture);
        draw_grid(cube, 24, 0.5f, sl::MaterialBlend::alpha, {200, 90, 40, 140}, nullptr);
        draw_grid(cube, 24, 0.9f, sl::MaterialBlend::additive, {40, 70, 120, 90}, nullptr);
        std::vector<sl::InstanceData3D> instances;
        for (int i = 0; i < 16; ++i)
        {
            sl::InstanceData3D instance;
            instance.model = glm::translate(glm::mat4(1.0f), {i * 1.5f - 12.0f, 2.5f, 0.0f});
            instance.tint = {255, 220, 80, 255};
            instances.push_back(instance);
        }
        sl::draw_mesh_instanced(cube, instances, {});
        if (inset)
        {
            sl::Material shown;
            shown.texture = inset;
            shown.lit = false;
            sl::draw_mesh(cube, glm::scale(glm::translate(glm::mat4(1.0f), {0, 4, 0}), glm::vec3(4.0f)), shown);
        }
        sl::end_surface_3d();
        sl::show_video_bitmap();
        sl::end_frame();
        sl::Bitmap *image = sl::surface_3d_bitmap(surface);
        if (!sl::download_bitmap(image))
            std::abort();
        std::vector<sl::Colour> pixels;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                pixels.push_back(sl::getpixel(image, x, y));
        return pixels;
    }

    std::vector<sl::Colour> read_back(sl::Surface3D *surface)
    {
        sl::show_video_bitmap();
        sl::end_frame();
        sl::Bitmap *image = sl::surface_3d_bitmap(surface);
        if (!sl::download_bitmap(image))
            std::abort();
        std::vector<sl::Colour> pixels;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                pixels.push_back(sl::getpixel(image, x, y));
        return pixels;
    }

    // The same three layers as render(), each filled into a queue on its own thread and submitted in order.
    std::vector<sl::Colour> render_queued(sl::Surface3D *surface, sl::Mesh *cube, sl::Bitmap *texture)
    {
        sl::Camera camera;
        camera.position = {0.0f, 22.0f, 24.0f};
        camera.target = {0.0f, 0.0f, 0.0f};
        sl::begin_surface_3d(surface, camera);
        sl::clear_surface_3d({20, 30, 50, 255});
        sl::RenderQueue3D queues[3];
        std::thread threads[3] = {
            std::thread([&] { draw_grid(cube, 24, 0.0f, sl::MaterialBlend::opaque, {60, 160, 90, 255}, texture, &queues[0]); }),
            std::thread([&] { draw_grid(cube, 24, 0.5f, sl::MaterialBlend::alpha, {200, 90, 40, 140}, nullptr, &queues[1]); }),
            std::thread([&] { draw_grid(cube, 24, 0.9f, sl::MaterialBlend::additive, {40, 70, 120, 90}, nullptr, &queues[2]); })};
        for (std::thread &thread : threads)
            thread.join();
        for (const sl::RenderQueue3D &queue : queues)
            check(sl::submit_render_queue(queue), "submit_render_queue");
        sl::end_surface_3d();
        return read_back(surface);
    }

    std::vector<sl::Colour> render_direct(sl::Surface3D *surface, sl::Mesh *cube, sl::Bitmap *texture)
    {
        sl::Camera camera;
        camera.position = {0.0f, 22.0f, 24.0f};
        camera.target = {0.0f, 0.0f, 0.0f};
        sl::begin_surface_3d(surface, camera);
        sl::clear_surface_3d({20, 30, 50, 255});
        draw_grid(cube, 24, 0.0f, sl::MaterialBlend::opaque, {60, 160, 90, 255}, texture);
        draw_grid(cube, 24, 0.5f, sl::MaterialBlend::alpha, {200, 90, 40, 140}, nullptr);
        draw_grid(cube, 24, 0.9f, sl::MaterialBlend::additive, {40, 70, 120, 90}, nullptr);
        sl::end_surface_3d();
        return read_back(surface);
    }

    bool same(const std::vector<sl::Colour> &a, const std::vector<sl::Colour> &b)
    {
        if (a.size() != b.size())
            return false;
        for (std::size_t i = 0; i < a.size(); ++i)
            if (a[i].red != b[i].red || a[i].green != b[i].green || a[i].blue != b[i].blue || a[i].alpha != b[i].alpha)
                return false;
        return true;
    }
} // namespace

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, size, size))
    {
        std::printf("no display available, skipping\n");
        return 0;
    }
    sl::set_light_3d({{-0.4f, -1.0f, -0.6f}, 0.3f});
    sl::Mesh *cube = sl::create_cube_mesh();
    sl::Bitmap *texture = sl::create_bitmap(8, 8);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            sl::putpixel(texture, x, y, ((x + y) & 1) ? sl::Colour{240, 240, 240, 255} : sl::Colour{40, 40, 200, 255});

    // Fixed settings so the test does not depend on the machine's core count.
    sl::set_parallel_recording_threshold(32);
    sl::set_parallel_recording_threads(4);

    for (const int samples : {1, 4})
    {
        sl::Surface3D *surface = sl::create_surface_3d(size, size, samples);
        sl::Surface3D *nested = sl::create_surface_3d(size, size, 1);
        check(surface && nested, "surfaces");
        if (!surface || !nested)
            return 1;

        // The nested surface is itself drawn with many draws, then sampled by a mesh in the main one.
        sl::set_parallel_recording(false);
        const std::vector<sl::Colour> nested_reference = render(nested, cube, texture, nullptr);
        const std::vector<sl::Colour> reference = render(surface, cube, texture, sl::surface_3d_bitmap(nested));
        const std::uint32_t inline_parallel = sl::get_frame_stats().parallel_draws;
        sl::set_parallel_recording(true);
        const std::vector<sl::Colour> nested_parallel = render(nested, cube, texture, nullptr);
        const std::vector<sl::Colour> parallel = render(surface, cube, texture, sl::surface_3d_bitmap(nested));
        sl::end_frame();
        const sl::FrameStats stats = sl::get_frame_stats();
        (void)stats;

        check(inline_parallel == 0, "no parallel draws while disabled");
        check(same(nested_reference, nested_parallel), samples == 1 ? "nested surface matches" : "nested surface matches (msaa)");
        check(same(reference, parallel), samples == 1 ? "parallel output matches inline" : "parallel output matches inline (msaa)");

        // Repeated frames must stay identical (worker pools are reset and reused between frames).
        bool stable = true;
        for (int frame = 0; frame < 5; ++frame)
            stable = stable && same(parallel, render(surface, cube, texture, sl::surface_3d_bitmap(nested)));
        check(stable, "repeated parallel frames are stable");

        sl::destroy_surface_3d(nested);
        sl::destroy_surface_3d(surface);
    }

    // Parallel recording on Vulkan/D3D12 should actually engage: count it through the frame stats.
    {
        sl::Surface3D *surface = sl::create_surface_3d(size, size, 1);
        sl::set_parallel_recording(true);
        render(surface, cube, texture, nullptr);
        const sl::FrameStats stats = sl::get_frame_stats();
        const bool supported = stats.recording_threads > 1;
        std::printf("recording threads: %u, parallel draws last frame: %u\n", stats.recording_threads,
                    stats.parallel_draws);
        if (supported)
            check(stats.parallel_draws > 0, "queued draws were recorded in parallel");
        else
            check(stats.parallel_draws == 0, "no parallel draws without backend support");
        sl::set_parallel_recording_threads(1);
        render(surface, cube, texture, nullptr);
        check(sl::get_frame_stats().parallel_draws == 0, "one thread disables parallel recording");
        sl::destroy_surface_3d(surface);
    }

    // A render queue filled by worker threads must match the same draws issued directly.
    {
        sl::Surface3D *surface = sl::create_surface_3d(size, size, 1);
        sl::set_parallel_recording_threads(4);
        sl::set_parallel_recording(true);
        const std::vector<sl::Colour> direct = render_direct(surface, cube, texture);
        const std::vector<sl::Colour> queued = render_queued(surface, cube, texture);
        check(same(direct, queued), "render queue matches direct draws");
        sl::destroy_surface_3d(surface);
    }

    sl::destroy_bitmap(texture);
    sl::destroy_mesh(cube);
    sl::shutdown();
    if (failures)
    {
        std::printf("%d parallel recording checks failed\n", failures);
        return 1;
    }
    std::printf("parallel_record_test passed\n");
    return 0;
}

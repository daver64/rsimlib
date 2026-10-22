#undef NDEBUG
#include "sl.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

// Async loading: the results must match the synchronous loaders, uploads must respect the per-frame budget,
// and cancelling or shutting down with loads in flight must be safe. Run with --gl and --vulkan.
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

    // Pump frames until the handle is done; returns false on timeout.
    bool wait_frames(const sl::AsyncHandle &handle, int max_frames = 3000)
    {
        for (int i = 0; i < max_frames && !handle.done(); ++i)
        {
            sl::end_frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return handle.done();
    }

    sl::Colour render(sl::Surface3D *surface, sl::Model *model)
    {
        sl::Camera camera;
        camera.position = {2.0f, 1.5f, 3.0f};
        camera.target = {0.0f, 0.0f, 0.0f};
        sl::begin_surface_3d(surface, camera);
        sl::clear_surface_3d({10, 20, 30, 255});
        sl::draw_model(model, sl::model_matrix({0, 0, 0}, {0, 0.5f, 0}));
        sl::end_surface_3d();
        sl::show_video_bitmap();
        sl::end_frame();
        sl::Bitmap *image = sl::surface_3d_bitmap(surface);
        if (!sl::download_bitmap(image))
            std::abort();
        return sl::getpixel(image, 32, 32);
    }
} // namespace

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 64, 64))
    {
        std::printf("no display available, skipping\n");
        return 0;
    }
    const std::string root = std::string(SIMLIB_SOURCE_DIR) + "/assets/";
    const std::string image_path = root + "textures/atlas_grid.png";

    // Bitmaps match load_bitmap() and arrive with their texture already uploaded.
    {
        sl::Bitmap *reference = sl::load_bitmap(image_path);
        sl::AsyncBitmapOptions options;
        options.smooth = true;
        options.mipmaps = true;
        sl::AsyncBitmap async = sl::load_bitmap_async(image_path, options);
        check(async.valid(), "async bitmap handle is valid");
        check(wait_frames(async) && async.ready(), "async bitmap finishes");
        sl::Bitmap *loaded = async.get();
        check(loaded != nullptr, "async bitmap result");
        check(!async.valid() && async.get() == nullptr, "result can be taken once");
        if (reference && loaded)
        {
            check(loaded->width == reference->width && loaded->height == reference->height, "bitmap size matches");
            check(loaded->pixels == reference->pixels, "bitmap pixels match");
            check(loaded->gpu_texture != 0, "bitmap texture uploaded before ready");
            check(loaded->smooth && loaded->mipmaps, "bitmap options applied");
        }
        else
            check(false, "bitmaps loaded");
        sl::destroy_bitmap(reference);
        sl::destroy_bitmap(loaded);
    }

    // Failures report why and never produce a result.
    {
        sl::AsyncBitmap missing = sl::load_bitmap_async(root + "textures/does_not_exist.png");
        check(wait_frames(missing) && missing.failed(), "missing bitmap fails");
        check(!missing.error().empty(), "failure has an error message");
        check(missing.get() == nullptr, "failed load has no result");
        sl::AsyncModel bad = sl::load_model_async(root + "models/does_not_exist.glb");
        check(wait_frames(bad) && bad.failed(), "missing model fails");
    }

    // Models match the synchronous loaders and render identically.
    sl::Surface3D *surface = sl::create_surface_3d(64, 64, 1);
    check(surface != nullptr, "surface");
    sl::set_light_3d({{-0.4f, -1.0f, -0.6f}, 0.3f});
    for (const char *name : {"models/crate.obj", "models/box.glb", "models/gem.gltf"})
    {
        const std::string path = root + name;
        sl::Model *reference = sl::load_model(path);
        sl::AsyncModel async = sl::load_model_async(path);
        check(wait_frames(async) && async.ready(), name);
        sl::Model *loaded = async.get();
        check(reference && loaded, name);
        if (!reference || !loaded)
            continue;
        check(reference->parts.size() == loaded->parts.size(), "model part count matches");
        check(reference->meshes.size() == loaded->meshes.size(), "model mesh count matches");
        check(reference->textures.size() == loaded->textures.size(), "model texture count matches");
        bool uploaded = true;
        for (std::size_t i = 0; i < loaded->meshes.size() && i < reference->meshes.size(); ++i)
        {
            uploaded = uploaded && loaded->meshes[i]->handle != 0 &&
                       loaded->meshes[i]->vertex_count == reference->meshes[i]->vertex_count;
            check(loaded->meshes[i]->vertices.size() == reference->meshes[i]->vertices.size(),
                  "mesh CPU data retention matches");
        }
        check(uploaded, "model meshes uploaded before ready");
        const sl::Colour a = render(surface, reference);
        const sl::Colour b = render(surface, loaded);
        check(a.red == b.red && a.green == b.green && a.blue == b.blue, "model renders identically");
        sl::destroy_model(reference);
        sl::destroy_model(loaded);
    }

    // The per-frame budget limits uploads: with a one-byte budget exactly one item is uploaded per frame.
    {
        sl::set_async_upload_budget(1);
        std::vector<sl::AsyncBitmap> loads;
        for (int i = 0; i < 4; ++i)
            loads.push_back(sl::load_bitmap_async(image_path));
        std::size_t previous = 0;
        bool within_budget = true;
        bool all_ready = false;
        for (int frame = 0; frame < 3000 && !all_ready; ++frame)
        {
            sl::end_frame();
            std::size_t ready = 0;
            for (const sl::AsyncBitmap &load : loads)
                ready += load.ready() ? 1 : 0;
            within_budget = within_budget && ready <= previous + 1;
            previous = ready;
            all_ready = ready == loads.size();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        check(all_ready, "budgeted loads all finish");
        check(within_budget, "at most one upload per frame under a tiny budget");
        for (sl::AsyncBitmap &load : loads)
            sl::destroy_bitmap(load.get());
        sl::set_async_upload_budget(8u * 1024u * 1024u);
    }

    // Cancelling, including before the load has run, must not leak or crash.
    {
        for (int i = 0; i < 8; ++i)
        {
            sl::AsyncModel doomed = sl::load_model_async(root + "models/box.glb");
            if (i % 2)
                sl::end_frame();
        }
        sl::AsyncBitmap explicit_cancel = sl::load_bitmap_async(image_path);
        explicit_cancel.cancel();
        check(!explicit_cancel.valid(), "cancelled handle is empty");
        for (int i = 0; i < 200 && sl::async_loads_pending() > 0; ++i)
        {
            sl::end_frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        check(sl::async_loads_pending() == 0, "cancelled loads drain");
    }

    // wait() blocks until the result is usable.
    {
        sl::AsyncBitmap blocking = sl::load_bitmap_async(image_path);
        check(blocking.wait(), "wait() returns once ready");
        sl::destroy_bitmap(blocking.get());
    }

    // Shutting down with loads in flight and finished-but-untaken results releases everything.
    sl::AsyncModel untaken = sl::load_model_async(root + "models/crate.obj");
    check(untaken.wait(), "model ready before shutdown");
    std::vector<sl::AsyncModel> in_flight;
    for (int i = 0; i < 6; ++i)
        in_flight.push_back(sl::load_model_async(root + "models/gem.gltf"));
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    check(untaken.failed() && untaken.get() == nullptr, "untaken result is released by shutdown");
    for (const sl::AsyncModel &load : in_flight)
        check(load.done(), "in-flight load is resolved by shutdown");
    in_flight.clear();

    if (failures)
    {
        std::printf("%d async checks failed\n", failures);
        return 1;
    }
    std::printf("async_load_test passed\n");
    return 0;
}

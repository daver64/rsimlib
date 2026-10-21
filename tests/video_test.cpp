#include "sl.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
    int failures = 0;

    void check(bool ok, const char *what)
    {
        if (!ok)
        {
            std::fprintf(stderr, "FAIL: %s (last_error: %s)\n", what, sl::last_error().c_str());
            ++failures;
        }
    }

    bool has_colour(sl::Video *video)
    {
        const sl::Bitmap *bitmap = sl::video_bitmap(video);
        if (!bitmap)
            return false;
        for (std::size_t i = 0; i + 2 < bitmap->pixels.size(); i += 4)
        {
            if (bitmap->pixels[i] > 40 || bitmap->pixels[i + 1] > 40 || bitmap->pixels[i + 2] > 40)
                return true;
        }
        return false;
    }

    void play(sl::Video *video, double seconds)
    {
        for (double t = 0.0; t < seconds; t += 0.04)
            sl::update_video(video, 0.04);
    }

    void test_clip(const std::string &path, const char *label)
    {
        std::string what = label;

        sl::Video *video = sl::open_video(path, false);
        check(video != nullptr, (what + ": open").c_str());
        if (!video)
            return;

        check(sl::video_width(video) == 96 && sl::video_height(video) == 64, (what + ": size").c_str());
        check(std::fabs(sl::video_duration(video) - 1.0) < 0.3, (what + ": duration").c_str());
        check(!sl::video_has_audio(video), (what + ": audio disabled").c_str());

        play(video, 0.5);
        check(has_colour(video), (what + ": frame decoded").c_str());
        check(!sl::video_has_ended(video), (what + ": not ended at 0.5s").c_str());

        check(sl::seek_video(video, 0.25), (what + ": seek").c_str());
        check(std::fabs(sl::video_position(video) - 0.25) < 0.15, (what + ": position after seek").c_str());
        play(video, 0.2);
        check(has_colour(video), (what + ": frame after seek").c_str());

        play(video, 2.0);
        check(sl::video_has_ended(video), (what + ": ended").c_str());

        sl::seek_video(video, 0.0);
        sl::set_video_loop(video, true);
        play(video, 3.0);
        check(!sl::video_has_ended(video), (what + ": loops").c_str());
        sl::destroy_video(video);

        std::ifstream file(path, std::ios::binary);
        const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        sl::Video *memory = sl::open_video_from_memory(data.data(), data.size(), false);
        check(memory != nullptr, (what + ": open from memory").c_str());
        if (memory)
        {
            play(memory, 0.5);
            check(has_colour(memory), (what + ": memory frame decoded").c_str());
            sl::destroy_video(memory);
        }

        sl::Video *with_audio = sl::open_video(path, true);
        check(with_audio && sl::video_has_audio(with_audio), (what + ": audio track detected").c_str());
        sl::destroy_video(with_audio);
    }
} // namespace

int main()
{
    const std::string backend = sl::video_backend_name();
    const std::string data_dir = SIMLIB_TEST_DATA_DIR;

    sl::Video *missing = sl::open_video(data_dir + "/does_not_exist.mpg");
    check(missing == nullptr, "missing file returns nullptr");
    check(!sl::last_error().empty(), "missing file sets last_error");

    sl::Video *url = sl::open_video("http://127.0.0.1:9/none.mpg");
    check(url == nullptr, "unreachable URL returns nullptr");
    check(sl::video_has_ended(nullptr), "null video counts as ended");

#if defined(SIMLIB_VIDEO_BACKEND_none)
    check(backend == "none", "backend name is none");
    check(sl::open_video(data_dir + "/tiny.mpg") == nullptr, "none backend refuses to open");
#elif defined(SIMLIB_VIDEO_BACKEND_plmpeg)
    check(backend == "pl_mpeg", "backend name is pl_mpeg");
    test_clip(data_dir + "/tiny.mpg", "pl_mpeg tiny.mpg");
#else
    check(backend == "FFmpeg", "backend name is FFmpeg");
    test_clip(data_dir + "/tiny.mpg", "FFmpeg tiny.mpg");
    test_clip(data_dir + "/tiny.mp4", "FFmpeg tiny.mp4");

    sl::Video *file_url = sl::open_video("file://" + data_dir + "/tiny.mp4", false);
    check(file_url != nullptr, "FFmpeg opens file:// URLs");
    sl::destroy_video(file_url);
#endif

    if (failures)
        return EXIT_FAILURE;
    std::printf("video_test passed (%s)\n", backend.c_str());
    return EXIT_SUCCESS;
}

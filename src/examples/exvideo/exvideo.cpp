// Plays an MPEG1 (.mpg) video with MP2 audio into a bitmap, for use as a cutscene player.
#include "sl.h"

#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    std::string path;
    bool loop = false;

    // configure_graphics_backend_from_args() rejects any argument it doesn't recognize as a
    // backend flag, so only forward those (plus argv[0]) and keep the rest for ourselves.
    std::vector<char *> backend_args{argv[0]};

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--gl" || arg == "--vulkan" || arg == "--d3d11" || arg == "--d3d12")
            backend_args.push_back(argv[i]);
        else if (arg == "--loop")
            loop = true;
        else if (arg.rfind("-", 0) != 0 && path.empty())
            path = arg;
    }

    if (path.empty())
    {
        std::fprintf(stderr, "Usage: %s <video.mpg> [--loop]\n", argv[0]);
        return 1;
    }

    sl::configure_graphics_backend_from_args(static_cast<int>(backend_args.size()), backend_args.data());

    sl::Video *video = sl::open_video(path);
    if (!video)
    {
        std::fprintf(stderr, "Could not open video: %s\n", path.c_str());
        return 1;
    }

    std::fprintf(stderr, "Opened %s: %dx%d, duration=%.2fs\n",
                 path.c_str(), sl::video_width(video), sl::video_height(video), sl::video_duration(video));

    if (loop)
        sl::set_video_loop(video, true);

    const int width = sl::video_width(video);
    const int height = sl::video_height(video);

    if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, width, height, width, height))
    {
        sl::destroy_video(video);
        return 1;
    }

    sl::set_window_title("simlib video playback");
    sl::set_vsync(true);
    sl::set_fps(60);

    bool running = true;
    sl::Event event;

    while (running)
    {
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
            {
                running = false;
            }
            sl::display_handle_event(event);
        }

        sl::update_video(video, sl::get_frame_time() / 1000.0);

        if (sl::video_has_ended(video))
            running = false;

        sl::clear_to_colour(sl::screen, sl::Black);
        sl::draw_sprite_stretched(sl::video_bitmap(video), 0, 0, sl::screen_width(), sl::screen_height());

        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::destroy_video(video);
    sl::shutdown();

    return 0;
}

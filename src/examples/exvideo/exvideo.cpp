// Plays a video into a bitmap, for use as a cutscene player. The pl_mpeg backend plays MPEG1/MP2 (.mpg) files;
// the FFmpeg backend (SIMLIB_VIDEO=ffmpeg) plays most formats and URLs.
#include "sl.h"

#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    sl::set_text_panel_style({true});
    std::string path;
    bool loop = false;

    // configure_graphics_backend_from_args() rejects any argument it doesn't recognize as a
    // backend flag, so only forward those (plus argv[0]) and keep the rest for ourselves.
    std::vector<char *> backend_args{argv[0]};

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--gl" || arg == "--vulkan" || arg == "--d3d12")
            backend_args.push_back(argv[i]);
        else if (arg == "--loop")
            loop = true;
        else if (arg.rfind("-", 0) != 0 && path.empty())
            path = arg;
    }

    if (path.empty())
    {
        std::fprintf(stderr, "Usage: %s <video file or URL> [--loop]\n", argv[0]);
        return 1;
    }

    sl::configure_graphics_backend_from_args(static_cast<int>(backend_args.size()), backend_args.data());

    sl::Video *video = sl::open_video(path);
    if (!video)
    {
        std::fprintf(stderr, "Could not open video: %s\n%s\n", path.c_str(), sl::last_error().c_str());
        return 1;
    }

    std::fprintf(stderr, "Opened %s (%s): %dx%d, duration=%.2fs\n", path.c_str(), sl::video_backend_name(),
                 sl::video_width(video), sl::video_height(video), sl::video_duration(video));

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
    bool paused = false;
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
            else if (event.type() == sl::Event::Type::key_down)
            {
                if (event.key() == sl::Event::Key::space)
                    paused = !paused;
                else if (event.key() == sl::Event::Key::arrow_left)
                    sl::seek_video(video, sl::video_position(video) - 5.0);
                else if (event.key() == sl::Event::Key::arrow_right)
                    sl::seek_video(video, sl::video_position(video) + 5.0);
            }
            sl::display_handle_event(event);
        }

        if (!paused)
            sl::update_video(video, sl::get_frame_time() / 1000.0);

        if (sl::video_has_ended(video))
            running = false;

        sl::clear_to_colour(sl::screen, sl::Black);
        sl::draw_sprite_stretched(sl::video_bitmap(video), 0, 0, sl::screen_width(), sl::screen_height());

        sl::gprintf(8, 8, sl::White, "%s  %.1f / %.1f s  Space: pause  Left/Right: seek 5s%s", sl::video_backend_name(),
                    sl::video_position(video), sl::video_duration(video), paused ? "  [paused]" : "");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::destroy_video(video);
    sl::shutdown();

    return 0;
}

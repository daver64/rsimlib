// Plays a video into a bitmap, for use as a cutscene player. The pl_mpeg backend plays MPEG1/MP2 (.mpg) files;
// the FFmpeg backend (SIMLIB_VIDEO=ffmpeg) plays most formats and URLs.
#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
    using sl::Colour;

    std::string format_time(double seconds)
    {
        const int total = static_cast<int>(std::max(0.0, seconds));
        char text[32];
        if (total >= 3600)
            std::snprintf(text, sizeof(text), "%d:%02d:%02d", total / 3600, total / 60 % 60, total % 60);
        else
            std::snprintf(text, sizeof(text), "%d:%02d", total / 60, total % 60);
        return text;
    }

    bool inside(int x, int y, float left, float top, float right, float bottom)
    {
        return x >= left && x <= right && y >= top && y <= bottom;
    }

    // A slim horizontal slider: track, filled part and a knob that grows while hovered or dragged.
    void draw_slider(float left, float right, float y, float value, bool active, int alpha)
    {
        const float x = left + (right - left) * value;
        const float half = active ? 3.0f : 2.0f;
        sl::rectfill(sl::screen, left, y - half, right, y + half, Colour{255, 255, 255, static_cast<Uint8>(alpha * 0.25f)});
        sl::rectfill(sl::screen, left, y - half, x, y + half, Colour{255, 255, 255, static_cast<Uint8>(alpha * 0.85f)});
        sl::circlefill(sl::screen, x, y, active ? 7.0f : 5.0f, Colour{255, 255, 255, static_cast<Uint8>(alpha)});
    }

    void draw_play_icon(float x, float y, bool paused, Colour colour)
    {
        if (paused)
            sl::trianglefill(sl::screen, x - 5, y - 8, x - 5, y + 8, x + 8, y, colour);
        else
        {
            sl::rectfill(sl::screen, x - 6, y - 8, x - 2, y + 8, colour);
            sl::rectfill(sl::screen, x + 2, y - 8, x + 6, y + 8, colour);
        }
    }

    void draw_speaker_icon(float x, float y, float volume, Colour colour)
    {
        sl::rectfill(sl::screen, x - 8, y - 3, x - 4, y + 3, colour);
        sl::trianglefill(sl::screen, x - 4, y - 3, x - 4, y + 3, x + 2, y + 8, colour);
        sl::trianglefill(sl::screen, x - 4, y - 3, x + 2, y - 8, x + 2, y + 8, colour);
        if (volume <= 0.0f)
        {
            sl::line(sl::screen, x + 5, y - 5, x + 12, y + 5, colour, 2.0f);
            sl::line(sl::screen, x + 5, y + 5, x + 12, y - 5, colour, 2.0f);
        }
        else
        {
            sl::line(sl::screen, x + 5, y - 3, x + 5, y + 3, colour, 2.0f);
            if (volume > 0.5f)
                sl::line(sl::screen, x + 9, y - 6, x + 9, y + 6, colour, 2.0f);
        }
    }
} // namespace

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
    bool show_overlay = true;
    float overlay_alpha = 1.0f;
    float volume = 1.0f;
    float volume_before_mute = 1.0f;
    bool scrubbing = false;
    bool dragging_volume = false;
    double scrub_fraction = 0.0;
    bool pressed = false;
    bool released = false;
    const bool has_audio = sl::video_has_audio(video);
    const std::string name = path.substr(path.find_last_of("/\\") == std::string::npos ? 0 : path.find_last_of("/\\") + 1);
    sl::Event event;

    while (running)
    {
        pressed = false;
        released = false;
        while (sl::poll_event(&event))
        {
            // Button events (not polled state) so a click shorter than one frame is not missed.
            if (event.type() == sl::Event::Type::mouse_button_down && event.mouse_button() == 1)
                pressed = true;
            else if (event.type() == sl::Event::Type::mouse_button_up && event.mouse_button() == 1)
                released = true;

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
                else if (event.key() == sl::Event::Key::letter_i)
                    show_overlay = !show_overlay;
                else if (event.key() == sl::Event::Key::arrow_up)
                    volume = std::min(1.0f, volume + 0.05f);
                else if (event.key() == sl::Event::Key::arrow_down)
                    volume = std::max(0.0f, volume - 0.05f);
                else if (event.key() == sl::Event::Key::letter_m)
                {
                    if (volume > 0.0f)
                    {
                        volume_before_mute = volume;
                        volume = 0.0f;
                    }
                    else
                        volume = volume_before_mute;
                }
            }
            sl::display_handle_event(event);
        }

        const float sw = static_cast<float>(sl::screen_width());
        const float sh = static_cast<float>(sl::screen_height());
        const double duration = sl::video_duration(video);

        // Layout of the bottom control bar.
        const float margin = std::max(16.0f, sw * 0.02f);
        const float bar_top = sh - 104.0f;
        const float play_x = margin + 10.0f;
        const float hint_y = bar_top + 6.0f;
        const float seek_y = bar_top + 46.0f;
        const float row_y = bar_top + 78.0f;
        const float seek_left = margin;
        const float seek_right = sw - margin;
        const float volume_right = sw - margin;
        const float volume_left = volume_right - std::min(120.0f, sw * 0.2f);

        const int mx = sl::mouse_x();
        const int my = sl::mouse_y();

        // Controls only react while visible, so hiding them never causes accidental clicks.
        if (show_overlay)
        {
            if (pressed && inside(mx, my, seek_left, seek_y - 12, seek_right, seek_y + 12) && duration > 0.0)
                scrubbing = true;
            else if (pressed && has_audio && inside(mx, my, volume_left - 8, row_y - 12, volume_right + 8, row_y + 12))
                dragging_volume = true;
            else if (pressed && inside(mx, my, play_x - 16, row_y - 14, play_x + 16, row_y + 14))
                paused = !paused;
        }
        if (scrubbing)
        {
            scrub_fraction = std::clamp((mx - seek_left) / (seek_right - seek_left), 0.0f, 1.0f);
            if (released)
            {
                sl::seek_video(video, scrub_fraction * duration);
                scrubbing = false;
            }
        }
        if (dragging_volume)
        {
            volume = std::clamp((mx - volume_left) / (volume_right - volume_left), 0.0f, 1.0f);
            if (released)
                dragging_volume = false;
        }
        sl::set_video_volume(video, volume);

        if (!paused && !scrubbing)
            sl::update_video(video, sl::get_frame_time() / 1000.0);

        if (sl::video_has_ended(video))
            running = false;

        const float dt = static_cast<float>(sl::get_frame_time()) / 1000.0f;
        const float target = show_overlay ? 1.0f : 0.0f;
        overlay_alpha += std::clamp(target - overlay_alpha, -dt * 6.0f, dt * 6.0f);

        sl::clear_to_colour(sl::screen, sl::Black);
        sl::draw_sprite_stretched(sl::video_bitmap(video), 0, 0, sl::screen_width(), sl::screen_height());

        if (overlay_alpha > 0.01f)
        {
            const int alpha = static_cast<int>(255 * overlay_alpha);
            const auto fade = [&](int value) { return static_cast<Uint8>(value * overlay_alpha); };
            const Colour text{240, 240, 240, static_cast<Uint8>(alpha)};
            const Colour dim{200, 200, 200, static_cast<Uint8>(alpha * 0.85f)};

            // Soft dark gradients at the top and bottom keep the text legible over bright footage.
            constexpr int bands = 12;
            for (int i = 0; i < bands; ++i)
            {
                const float t = 1.0f - static_cast<float>(i) / bands;
                const Uint8 a = fade(static_cast<int>(200 * t));
                const float top_band = 60.0f / bands;
                const float bottom_band = (sh - bar_top + 12.0f) / bands;
                sl::rectfill(sl::screen, 0, i * top_band, sw, (i + 1) * top_band, Colour{0, 0, 0, a});
                sl::rectfill(sl::screen, 0, sh - (i + 1) * bottom_band, sw, sh - i * bottom_band, Colour{0, 0, 0, a});
            }

            sl::gprintf(static_cast<int>(margin), 14, text, "%s", name.c_str());
            sl::gprintf(static_cast<int>(margin), 34, dim, "%s  %dx%d%s", sl::video_backend_name(), sl::video_width(video),
                        sl::video_height(video), has_audio ? "" : "  (no audio)");

            const double shown = scrubbing ? scrub_fraction * duration : sl::video_position(video);
            const float fraction = duration > 0.0 ? static_cast<float>(std::clamp(shown / duration, 0.0, 1.0)) : 0.0f;
            const bool seek_hot = scrubbing || inside(mx, my, seek_left, seek_y - 12, seek_right, seek_y + 12);
            draw_slider(seek_left, seek_right, seek_y, fraction, seek_hot, alpha);

            draw_play_icon(play_x, row_y, paused, text);
            sl::gprintf(static_cast<int>(play_x + 24), static_cast<int>(row_y - 8), text, "%s / %s", format_time(shown).c_str(),
                        format_time(duration).c_str());

            if (has_audio)
            {
                const bool volume_hot = dragging_volume || inside(mx, my, volume_left - 8, row_y - 12, volume_right + 8, row_y + 12);
                draw_speaker_icon(volume_left - 24, row_y, volume, text);
                draw_slider(volume_left, volume_right, row_y, volume, volume_hot, alpha);
            }

            sl::gprintf_center(static_cast<int>(hint_y), dim, "Space pause | Left/Right skip | Up/Down volume | M mute | I hide");
            if (paused)
                sl::gprintf_center(static_cast<int>(sh / 2 - 8), text, "PAUSED");
        }

        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::destroy_video(video);
    sl::shutdown();

    return 0;
}

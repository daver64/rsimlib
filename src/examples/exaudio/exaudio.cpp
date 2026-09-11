#include "sl.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace
{
    struct AudioState
    {
        sl::Sample *sample = nullptr;
        sl::Stream *stream = nullptr;
        std::uint64_t voice = 0;
        int volume = 192;
        bool music_paused = false;
        std::string error;
    };

    void cleanup_audio(AudioState &audio)
    {
        if (audio.voice != 0)
            sl::stop_voice(audio.voice);
        if (audio.sample)
            sl::destroy_sample(audio.sample);
        if (audio.stream)
            sl::destroy_stream(audio.stream);
        sl::audio_fx_shutdown();
        sl::music_shutdown();
        audio = {};
    }

    bool load_audio(AudioState &audio)
    {
        if (!sl::audio_fx_init())
        {
            audio.error = "Sound effects unavailable: " + sl::last_error();
            return false;
        }
        if (!sl::music_init())
        {
            audio.error = "Music unavailable: " + sl::last_error();
            return false;
        }

        audio.sample = sl::load_sample("assets/sfx/attack.wav");
        audio.stream = sl::load_stream("assets/music/Solar Serenity.ogg");
        if (!audio.sample || !audio.stream)
        {
            audio.error = "Unable to load the audio example assets.";
            return false;
        }
        sl::audio_fx_set_volume(audio.volume);
        sl::music_set_volume(audio.volume);
        sl::play_stream(audio.stream);
        return true;
    }

    void process_input(AudioState &audio, bool &running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down &&
                 event.key() == sl::Event::Key::escape))
            {
                running = false;
            }
            if (event.type() != sl::Event::Type::key_down)
            {
                sl::display_handle_event(event);
                continue;
            }

            switch (event.key())
            {
            case sl::Event::Key::space:
                if (audio.sample)
                    audio.voice = sl::play_sample(audio.sample, audio.volume);
                break;
            case sl::Event::Key::letter_m:
                if (audio.stream)
                {
                    if (audio.music_paused)
                        sl::resume_stream();
                    else
                        sl::pause_stream();
                    audio.music_paused = !audio.music_paused;
                }
                break;
            case sl::Event::Key::letter_s:
                sl::stop_stream();
                audio.music_paused = false;
                break;
            case sl::Event::Key::arrow_left:
                audio.volume = std::max(0, audio.volume - 16);
                sl::audio_fx_set_volume(audio.volume);
                sl::music_set_volume(audio.volume);
                break;
            case sl::Event::Key::arrow_right:
                audio.volume = std::min(255, audio.volume + 16);
                sl::audio_fx_set_volume(audio.volume);
                sl::music_set_volume(audio.volume);
                break;
            default:
                break;
            }
            sl::display_handle_event(event);
        }
    }

    void draw_screen(const AudioState &audio, bool loaded)
    {
        sl::clear_to_colour(sl::screen, {24, 29, 38});
        sl::gprintf_center(48, {235, 220, 155}, "Audio example");
        if (!loaded)
        {
            sl::gprintf_center(112, {240, 130, 120}, "%s", audio.error.c_str());
            sl::gprintf_center(160, {170, 185, 205}, "Press Escape to exit");
        }
        else
        {
            sl::gprintf_center(104, {218, 226, 235}, "Space  play sound effect");
            sl::gprintf_center(132, {218, 226, 235}, "M      pause/resume music");
            sl::gprintf_center(160, {218, 226, 235}, "S      stop music");
            sl::gprintf_center(188, {218, 226, 235}, "Left/Right  volume: %d / 255", audio.volume);
            sl::gprintf_center(232, {145, 160, 178}, "Escape  exit");
        }
        sl::show_video_bitmap();
        sl::end_frame();
    }
}

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 360))
    {
        return -1;
    }

    AudioState audio;
    const bool loaded = load_audio(audio);
    if (!loaded && !audio.error.empty())
        std::fprintf(stderr, "%s\n", audio.error.c_str());

    bool running = true;
    while (running)
    {
        process_input(audio, running);
        draw_screen(audio, loaded);
    }

    cleanup_audio(audio);
    sl::wait_for_graphics();
    sl::shutdown();
    return 0;
}
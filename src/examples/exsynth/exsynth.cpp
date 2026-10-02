#include "sl.h"
#include "audio_synth.h"

#include <cstdio>
#include <vector>

namespace
{
    struct Patch
    {
        const char *name;
        sl::Sample *sample;
    };

    sl::Sample *make_laser()
    {
        sl::SynthParams p;
        p.duration = 0.35f;
        p.envelope = {0.001f, 0.05f, 0.5f, 0.15f};
        p.oscillators.push_back({sl::Waveform::square, 880.0f, 0.6f, {1.0f, 0.15f}});
        return sl::generate_sample(p);
    }

    sl::Sample *make_explosion()
    {
        sl::SynthParams p;
        p.duration = 0.9f;
        p.envelope = {0.005f, 0.3f, 0.4f, 0.5f};
        p.oscillators.push_back({sl::Waveform::noise, 1.0f, 0.7f, {}});
        p.oscillators.push_back({sl::Waveform::saw, 110.0f, 0.5f, {1.0f, 0.3f}});
        return sl::generate_sample(p);
    }

    sl::Sample *make_coin()
    {
        sl::SynthParams p;
        p.duration = 0.25f;
        p.envelope = {0.001f, 0.05f, 0.6f, 0.1f};
        p.oscillators.push_back({sl::Waveform::triangle, 988.0f, 0.5f, {1.0f, 1.0f}});
        p.oscillators.push_back({sl::Waveform::triangle, 1318.0f, 0.5f, {1.0f, 1.0f}});
        return sl::generate_sample(p);
    }

    sl::Sample *make_tone()
    {
        sl::SynthParams p;
        p.duration = 0.6f;
        p.envelope = {0.02f, 0.1f, 0.7f, 0.2f};
        p.oscillators.push_back({sl::Waveform::sine, 440.0f, 1.0f, {}});
        return sl::generate_sample(p);
    }
}

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 640, 300))
    {
        return -1;
    }

    if (!sl::audio_fx_init())
    {
        std::fprintf(stderr, "Audio unavailable: %s\n", sl::last_error().c_str());
        return -1;
    }

    std::vector<Patch> patches = {
        {"1  Laser", make_laser()},
        {"2  Explosion", make_explosion()},
        {"3  Coin", make_coin()},
        {"4  Tone (A4)", make_tone()},
    };

    bool running = true;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
            {
                running = false;
            }
            if (event.type() == sl::Event::Type::key_down)
            {
                int index = -1;
                switch (event.key())
                {
                case sl::Event::Key::digit_1: index = 0; break;
                case sl::Event::Key::digit_2: index = 1; break;
                case sl::Event::Key::digit_3: index = 2; break;
                case sl::Event::Key::digit_4: index = 3; break;
                default: break;
                }
                if (index >= 0 && patches[index].sample)
                    sl::play_sample(patches[index].sample, 220);
            }
            sl::display_handle_event(event);
        }

        sl::clear_to_colour(sl::screen, {24, 29, 38});
        sl::gprintf_center(32, {235, 220, 155}, "Synth example");
        int y = 80;
        for (const Patch &patch : patches)
        {
            sl::gprintf_center(y, patch.sample ? sl::Colour{218, 226, 235} : sl::Colour{240, 130, 120},
                               "%s", patch.name);
            y += 28;
        }
        sl::gprintf_center(240, {145, 160, 178}, "Escape  exit");
        sl::show_video_bitmap();
        sl::end_frame();
    }

    for (Patch &patch : patches)
        if (patch.sample)
            sl::destroy_sample(patch.sample);
    sl::audio_fx_shutdown();
    sl::wait_for_graphics();
    sl::shutdown();
    return 0;
}

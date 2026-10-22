# Lesson 4 (2D): Text and sound

[← Tutorial index](README.md) · Previous: [Tile maps and collision](2d-03-tilemaps-and-collision.md)

**Goal:** draw text in different fonts, play a sound effect on a key press and loop background music.

## The idea

- **Text:** `sl::gprintf(x, y, colour, "format", ...)` draws printf-style text in the built-in monospace font. Pass a `Font *` before the colour for another font: `sl::open_sans_font(size)` gives a proportional one. Close fonts you open with `sl::close_font`.
- **Sound effects** are short samples loaded fully into memory: `sl::load_sample(path)`, then `sl::play_sample(sample, volume)` (volume 0–255). Many can play at once; `play_sample` returns a voice id.
- **Music** is streamed from disk, so long files cost little memory: `sl::load_stream(path)` then `sl::play_stream(stream)` (loops forever by default).
- The two audio systems start separately: `sl::audio_fx_init()` for samples and `sl::music_init()` for streams. Either can fail on a machine with no sound device, so check them and carry on without audio if they do.

## The program

```cpp
#include "sl.h"

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }
    sl::set_fps(60);

    sl::Font *big = sl::open_sans_font(36);

    const bool have_fx = sl::audio_fx_init();
    const bool have_music = sl::music_init();
    sl::Sample *beep = have_fx ? sl::load_sample("assets/sfx/attack.wav") : nullptr;
    sl::Stream *song = have_music ? sl::load_stream("assets/music/Solar Serenity.ogg") : nullptr;
    if (have_fx)
        sl::audio_fx_set_volume(200);
    if (have_music)
        sl::music_set_volume(80);
    if (song)
        sl::play_stream(song);

    int plays = 0;
    bool running = true;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            if (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::space && beep)
            {
                sl::play_sample(beep, 255);
                ++plays;
            }
            sl::display_handle_event(event);
        }

        sl::clear_to_colour(sl::screen, {30, 30, 44});
        sl::gprintf_center(120, big, {255, 220, 120}, "Text and sound");
        sl::gprintf_center(200, {220, 220, 220}, "Press Space to play a sound effect (%d so far)", plays);
        sl::gprintf_center(230, {160, 160, 180}, "%s  /  %s", beep ? "effects ready" : "no effects",
                           song ? "music playing" : "no music");

        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    if (beep)
        sl::destroy_sample(beep);
    if (song)
        sl::destroy_stream(song);
    if (have_fx)
        sl::audio_fx_shutdown();
    if (have_music)
        sl::music_shutdown();
    sl::close_font(big);
    sl::shutdown();
    return 0;
}
```

## What just happened

- The key *event* is used for the sound, not `key_down()` state: holding Space would otherwise retrigger the sound every frame.
- Music and effects have separate volume controls, so you can keep effects audible while the music sits quietly behind them.
- Cleanup mirrors set-up: destroy what you loaded, shut down what you initialised, then `sl::shutdown()`.

## Try it

1. Add `sl::pause_stream()` / `sl::resume_stream()` on the `M` key.
2. Pan an effect left or right: `sl::play_sample(beep, 255, pan)`, where `pan` is 0 (left) to 255 (right), 128 is centre.
3. Skip sound files entirely: [`exsynth`](../examples/exsynth.md) builds sounds from code with the procedural audio synth.

## Go further

- Examples: [`exaudio`](../examples/exaudio.md), [`exfont`](../examples/exfont.md). API: [Typography](../api_reference.md#typography--text-rendering-includefonth), [Audio](../api_reference.md#audio-subsystem-samples--streams-includeaudioh).
- Next track: [Your first 3D scene](3d-01-first-scene.md), or [Physics bodies](sim-01-physics-bodies.md).

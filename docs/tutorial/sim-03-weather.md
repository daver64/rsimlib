# Lesson 3 (Simulation): Weather

[← Tutorial index](README.md) · Previous: [Joints](sim-02-joints.md)

**Goal:** add rain, hail and snow to a scene, and change the weather smoothly while it runs.

## The idea

- `sl::Precipitation2D` is a ready-made particle system for falling rain, hail and snow. Create it with the size of the area to fill, call `update(dt)` every frame, then `draw()` after your scene so the weather is in front.
- A **config** describes the weather: type, intensity (0–1), wind and more. `sl::precipitation_preset(type, level)` gives a good one for `light`, `moderate`, `heavy` or `storm`.
- You never jump to a new config. `precipitation.weather.transition_to(config, seconds)` fades there, so a shower can build and clear naturally.
- `set_ground(y)` sets where particles land: rain splashes and snow piles up on that line. It can also be a function of x for hills and platforms.
- Weather is for looks. It doesn't affect physics; if you want wet ground to be slippery you apply that yourself.

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

    sl::Precipitation2D weather(800, 600);
    weather.set_ground(520.0f);

    sl::PrecipitationType type = sl::PrecipitationType::rain;
    sl::PrecipitationLevel level = sl::PrecipitationLevel::moderate;
    weather.weather.transition_to(sl::precipitation_preset(type, level), 1.0f);

    bool running = true;
    while (running)
    {
        bool changed = false;
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit)
                running = false;
            if (event.type() == sl::Event::Type::key_down)
            {
                switch (event.key())
                {
                case sl::Event::Key::escape:
                    running = false;
                    break;
                case sl::Event::Key::digit_1:
                    type = sl::PrecipitationType::rain;
                    changed = true;
                    break;
                case sl::Event::Key::digit_2:
                    type = sl::PrecipitationType::hail;
                    changed = true;
                    break;
                case sl::Event::Key::digit_3:
                    type = sl::PrecipitationType::snow;
                    changed = true;
                    break;
                case sl::Event::Key::space:
                    level = level == sl::PrecipitationLevel::storm
                                ? sl::PrecipitationLevel::light
                                : static_cast<sl::PrecipitationLevel>(static_cast<int>(level) + 1);
                    changed = true;
                    break;
                default:
                    break;
                }
            }
            sl::display_handle_event(event);
        }

        if (changed)
            weather.weather.transition_to(sl::precipitation_preset(type, level), 3.0f);

        weather.update(static_cast<float>(1.0 / 60.0));

        sl::clear_to_colour(sl::screen, {40, 50, 70});
        sl::rectfill(sl::screen, 0.0f, 520.0f, 800.0f, 600.0f, {50, 90, 50});
        weather.draw();
        sl::gprintf(10, 10, {255, 255, 255}, "1 rain, 2 hail, 3 snow, Space: heavier. Escape quits.");
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::shutdown();
    return 0;
}
```

## What just happened

- Pressing a key doesn't switch weather instantly: it starts a 3-second transition, so rain thins out as snow builds up.
- The grass is drawn **before** `weather.draw()`, so snow and rain appear in front. The ground line (520) matches the top of the grass, which is why drops land there.
- `PrecipitationLevel` is a plain enum; stepping to the next level is just adding 1 until the last.

## Try it

1. Add wind: set `config.wind = {120.0f, 0.0f}` on the preset before the transition.
2. Make the ground uneven with a function: `weather.set_ground([](float x) { return 500.0f + 20.0f * std::sin(x * 0.02f); })`.
3. Add rain sound with `sl::PrecipitationAudio` ([`exprecip2d`](../examples/exprecip2d.md) shows it), or a lens-droplet effect with `sl::PrecipitationLens`.

## Go further

- Examples: [`exprecip2d`](../examples/exprecip2d.md), [`exprecip3d`](../examples/exprecip3d.md) (the 3D version for first-person scenes). API: [Precipitation](../api_reference.md#precipitation-includeprecipitationh).
- Back to the [tutorial index](README.md), or try the [Showcase Guide](../showcases.md).

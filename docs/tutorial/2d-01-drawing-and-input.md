# Lesson 1 (2D): Drawing and moving things

[← Tutorial index](README.md) · Previous: [Lesson 0](00-first-window.md)

**Goal:** draw shapes, move a player with the keyboard and follow the mouse.

## The idea

- Drawing calls take a target bitmap first, almost always `sl::screen`, then coordinates, then a colour: `sl::circlefill(sl::screen, x, y, radius, colour)`.
- The screen's origin `(0, 0)` is the **top-left**; x grows right, y grows **down**.
- To move something at a steady speed whatever the frame rate, multiply speed (pixels per second) by the frame time in seconds (`sl::get_frame_time() / 1000.0`). This is called a *delta time*.
- `sl::key_down(key)` asks "is this key held right now?", which suits movement. The `key_down` *event* tells you the moment of a press, which suits one-off actions.
- `sl::mouse_x()` and `sl::mouse_y()` give the pointer position.

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

    float x = 400.0f, y = 300.0f;
    const float speed = 240.0f; // pixels per second
    bool running = true;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            sl::display_handle_event(event);
        }

        const float dt = static_cast<float>(sl::get_frame_time()) / 1000.0f;
        if (sl::key_down(sl::Event::Key::arrow_left))
            x -= speed * dt;
        if (sl::key_down(sl::Event::Key::arrow_right))
            x += speed * dt;
        if (sl::key_down(sl::Event::Key::arrow_up))
            y -= speed * dt;
        if (sl::key_down(sl::Event::Key::arrow_down))
            y += speed * dt;

        sl::clear_to_colour(sl::screen, {24, 28, 40});
        sl::rectfill(sl::screen, 0.0f, 520.0f, 800.0f, 600.0f, {50, 110, 60}); // ground
        sl::line(sl::screen, x, y, static_cast<float>(sl::mouse_x()), static_cast<float>(sl::mouse_y()), {255, 220, 120}, 2.0f);
        sl::circlefill(sl::screen, x, y, 22.0f, {230, 90, 70});
        sl::circle(sl::screen, x, y, 28.0f, {255, 255, 255}, 2.0f);
        sl::gprintf(10, 10, {200, 200, 200}, "Arrow keys move the ball. Escape quits.");

        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::shutdown();
    return 0;
}
```

## What just happened

- The event loop only handles quitting. Movement reads the keyboard state directly each frame.
- `dt` converts "240 pixels per second" into "pixels to move this frame", so the ball moves at the same speed at 30 or 144 fps.
- Anything drawn later covers what was drawn earlier, so the order of your draw calls is the layering.
- Shapes come in outline (`circle`, `rect`, `triangle`, `ellipse`, `line`, `arc`) and filled (`circlefill`, `rectfill`, ...) forms. A colour's fourth value is alpha: `{255, 0, 0, 128}` is half-transparent red.

## Try it

1. Stop the ball leaving the window: clamp `x` to `[22, 778]` and `y` to `[22, 498]`.
2. Make it fall: add a `vy` that grows by 900 × `dt` each frame, and reset it to 0 when the ball touches the ground at `y = 498`. Jump on `Space` with `vy = -450`.
3. Draw a trail: remember the last 20 positions and draw smaller, fainter circles at them.

## Go further

- Examples: [`exinput`](../examples/exinput.md), [`exbitmap`](../examples/exbitmap.md). API: [Bitmaps & Drawing](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh), [Events, Input & Timing](../api_reference.md#events-input--timing-includeeventh-includeinputh-includesystemh-includegamepadh).
- Next: [Sprites and animation](2d-02-sprites-and-animation.md).

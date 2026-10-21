# exbitmap

Bitmap loading and blitting: loads a PNG and blits it to the screen at 60 FPS.

![exbitmap screenshot](img/exbitmap.png)

## What it shows
- Loading an image with `load_bitmap`
- Copying it to the screen with `blit`
- Frame pacing with `set_fps`
- Forwarding window events with `display_handle_event`

## Key code

In `exbitmap.cpp`, the image is loaded with `load_bitmap` and the frame rate is capped with `set_fps`.

```cpp
    sl::Bitmap *bitmap = sl::load_bitmap("assets/textures/balloon_red.png");
    if (!bitmap)
    {
        std::cerr << "Failed to load bitmap!" << std::endl;
        return -2;
    }
    sl::set_fps(60);
```

Each frame, events are forwarded with `display_handle_event` and the bitmap is copied to the screen with `blit`.

```cpp
        sl::Event event;
        while (sl::poll_event(&event))
        {
            // ...
            sl::display_handle_event(event);
        }

        sl::clear_to_colour(sl::screen, sl::Colour{45, 48, 56});
        sl::blit(bitmap, sl::screen, 0, 0, 100, 100, bitmap->width, bitmap->height);
```

## Running
```bash
./build/exbitmap
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Loads `assets/textures/balloon_red.png`, so run from the repository root.

## Controls
| Input | Action |
|-------|--------|
| Any key, or closing the window | Exit |

## Key APIs
- `load_bitmap`, `blit`, `destroy_bitmap` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)
- `set_fps`, `poll_event`, `display_handle_event` — [Events](../api_reference.md#events-input--timing-includeeventh-includeinputh-includesystemh-includegamepadh) · [`event.h`](../../include/event.h)
- `set_gfx_mode` — [Application Lifecycle](../api_reference.md#application-lifecycle--display-includedisplayh) · [`display.h`](../../include/display.h)

## Source
[src/examples/exbitmap/exbitmap.cpp](../../src/examples/exbitmap/exbitmap.cpp)

## See also
- [exrotatesprite](exrotatesprite.md)
- [exatlas](exatlas.md)
- [extriangle](extriangle.md)

# exhello

The smallest simlib program: open a window, draw centred text on a cleared background, and wait for a key press.

![exhello screenshot](img/exhello.png)

## What it shows
- Initialising a windowed display with `set_gfx_mode`
- Clearing the screen and drawing centred text with `gprintf_center`
- Querying the active graphics backend name
- Presenting a frame with `show_video_bitmap` and `end_frame`
- Clean shutdown with `wait_for_graphics` and `shutdown`

## Running
```bash
./build/exhello
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Any key, or closing the window | Exit |

## Key APIs
- `set_gfx_mode`, `configure_graphics_backend_from_args`, `shutdown` — [Application Lifecycle](../api_reference.md#application-lifecycle--display-includedisplayh) · [`display.h`](../../include/display.h)
- `poll_event`, `Event` — [Events](../api_reference.md#events-input--timing-includeeventh-includeinputh-includesystemh-includegamepadh) · [`event.h`](../../include/event.h)
- `clear_to_colour`, `show_video_bitmap`, `end_frame` — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)
- `gprintf_center`, `get_default_monospace_font`, `text_height` — [Typography](../api_reference.md#typography--text-rendering-includefonth) · [`font.h`](../../include/font.h)

## Source
[src/examples/exhello/exhello.cpp](../../src/examples/exhello/exhello.cpp)

## See also
- [exfont](exfont.md)
- [extriangle](extriangle.md)

# exfont

Loads TrueType fonts, measures text, and draws proportional and monospace samples side by side.

![exfont screenshot](img/exfont.png)

## What it shows
- Selecting the built-in monospace font and the Open Sans font
- Measuring strings with `text_length` to show wide and narrow glyph widths
- Drawing text with `textout`
- Releasing fonts with `close_font`

## Running
```bash
./build/exfont
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Any key, or closing the window | Exit |

## Key APIs
- `open_sans_font`, `get_default_monospace_font`, `close_font` — [Typography](../api_reference.md#typography--text-rendering-includefonth) · [`font.h`](../../include/font.h)
- `text_length`, `textout` — [Typography](../api_reference.md#typography--text-rendering-includefonth) · [`font.h`](../../include/font.h)
- `set_gfx_mode`, `shutdown` — [Application Lifecycle](../api_reference.md#application-lifecycle--display-includedisplayh) · [`display.h`](../../include/display.h)

## Source
[src/examples/exfont/exfont.cpp](../../src/examples/exfont/exfont.cpp)

## See also
- [exhello](exhello.md)
- [extriangle](extriangle.md)

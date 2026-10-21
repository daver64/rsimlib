# exfont

Loads TrueType fonts, measures text, and draws proportional and monospace samples side by side.

![exfont screenshot](img/exfont.png)

## What it shows
- Selecting the built-in monospace font and the Open Sans font
- Measuring strings with `text_length` to show wide and narrow glyph widths
- Drawing text with `textout`
- Releasing fonts with `close_font`

## Key code

In `exfont.cpp`, the Open Sans font and the built-in monospace font are selected.

```cpp
sl::Font *sans_font = sl::open_sans_font(28);
sl::Font *monospace_font = sl::get_default_monospace_font();
```

Strings are measured with `text_length` to centre them and drawn with `textout`, showing wide and narrow glyph widths.

```cpp
const std::string text = "Proportional TrueType: wide W, narrow i";
const int x = (sl::screen->width - sl::text_length(sans_font, text)) / 2;
sl::textout(sans_font, x, 150, {255, 220, 90}, text);
```

The loaded font is released with `close_font` on exit.

```cpp
if (sans_font)
{
    sl::close_font(sans_font);
}
```

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

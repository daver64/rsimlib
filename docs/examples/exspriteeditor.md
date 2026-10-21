# exspriteeditor

A pixel sprite and animation editor with a resizable canvas, undo, zoom, frames and PNG spritesheet save and load.

![exspriteeditor screenshot](img/exspriteeditor.png)

## What it shows
- Resizable documents (1x1 to 512x512)
- Per-frame undo
- Zoom and drawing tools
- Animation frames with playback
- Horizontal PNG spritesheet save and load

## Running
```bash
./build/exspriteeditor
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Only built when the library is configured with `SIMLIB_GUI=imgui` (the default).

## Controls
| Input | Action |
|-------|--------|
| Mouse | Draw with the selected tool |
| P / B / E / L / R / O / F / I / S | Pencil, Brush, Eraser, Line, Rectangle, Ellipse, Fill, Picker, Select |
| Ctrl+Z / Ctrl+Y | Undo / redo |
| Ctrl+C / X / V | Copy / cut / paste selection |
| Ctrl+S / Ctrl+N | Save / new document |
| Escape | Exit |

## Key APIs
- GUI functions — [Dear ImGui](../api_reference.md#dear-imgui-integration-includeguih) · [`gui.h`](../../include/gui.h)
- Bitmaps and PNG I/O — [Bitmaps](../api_reference.md#bitmaps-offscreen-render-targets--drawing-includedrawh) · [`draw.h`](../../include/draw.h)

## Source
[src/examples/exspriteeditor/exspriteeditor.cpp](../../src/examples/exspriteeditor/exspriteeditor.cpp)

## See also
- [exgui](exgui.md)
- [exanimsprite](exanimsprite.md)

See also the [slpaint guide](../slpaint_guide.md), the program it was forked from.

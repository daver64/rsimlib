# exspriteeditor

A pixel sprite and animation editor with a resizable canvas, undo, zoom, frames and PNG spritesheet save and load.

![exspriteeditor screenshot](img/exspriteeditor.png)

## What it shows
- Resizable documents (1x1 to 512x512)
- Per-frame undo
- Zoom and drawing tools
- Animation frames with playback
- Horizontal PNG spritesheet save and load

## Key code

In `exspriteeditor.cpp`, per-frame undo saves current pixels for redo, restores the previous snapshot, then pops it from the undo stack.
```cpp
        void undo()
        {
            if (selecting || selection)
            {
                cancel_selection();
                return;
            }

            Frame &f = current();
            if (f.undo_stack.empty())
                return;

            f.redo_stack.push_back(f.bitmap->pixels);
            restore_pixels(f.undo_stack.back());
            f.undo_stack.pop_back();
            status = "Undo";
        }
```

In `exspriteeditor.cpp`, playback uses the configured frame rate and wraps the active frame index to loop the animation.
```cpp
        void update_playback()
        {
            if (!playing || doc.frames.size() <= 1)
                return;

            const double interval = 1.0 / std::max(1, doc.fps);
            play_timer += sl::get_frame_time() / 1000.0; // get_frame_time() is milliseconds.

            while (play_timer >= interval)
            {
                play_timer -= interval;
                current_frame = (current_frame + 1) % static_cast<int>(doc.frames.size());
            }
        }
```

In `exspriteeditor.cpp`, saving blits every frame into a horizontal bitmap and writes the spritesheet.
```cpp
            for (int i = 0; i < frame_count; ++i)
            {
                sl::blit(
                    frame_bitmap(i),
                    sheet,
                    0,
                    0,
                    static_cast<float>(i * doc.width),
                    0,
                    doc.width,
                    doc.height);
            }

            const bool ok = sl::save_bitmap(sheet, path);
            sl::destroy_bitmap(sheet);

            status = ok ? ("Saved: " + path) : "Save failed";
```

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

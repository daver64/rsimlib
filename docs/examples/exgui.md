# exgui

Dear ImGui integration: the retained and debug GUI drawn on top of simlib, with the active backend and window size shown.

![exgui screenshot](img/exgui.png)

## What it shows
- ImGui windows rendered through `sl::gui`
- Widgets drawing over the 2D scene
- Backend and window readout

## Running
```bash
./build/exgui
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Only built when the library is configured with `SIMLIB_GUI=imgui` (the default).

## Controls
| Input | Action |
|-------|--------|
| Mouse and keyboard | Interact with the widgets |
| Escape | Exit |

## Key APIs
- GUI functions — [Dear ImGui](../api_reference.md#dear-imgui-integration-includeguih) · [`gui.h`](../../include/gui.h)

## Source
[src/examples/exgui/exgui.cpp](../../src/examples/exgui/exgui.cpp)

## See also
- [exspriteeditor](exspriteeditor.md)
- [exlobby](exlobby.md)

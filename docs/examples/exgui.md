# exgui

Dear ImGui integration: the retained and debug GUI drawn on top of simlib, with the active backend and window size shown.

![exgui screenshot](img/exgui.png)

## What it shows
- ImGui windows rendered through `sl::gui`
- Widgets drawing over the 2D scene
- Backend and window readout

## Key code

In `exgui.cpp`, the GUI is initialised with `gui_init` and window events are forwarded to ImGui.

```cpp
    sl::gui_init();
// ...
            sl::gui_handle_event(event);
            sl::display_handle_event(event);
```

Each frame, an ImGui window with widgets and a backend/window readout is built between `gui_new_frame` and `gui_render`, drawn over the 2D scene.

```cpp
        sl::clear_to_colour(sl::screen, {18, 23, 32});
        sl::gui_new_frame();
        ImGui::SetNextWindowBgAlpha(panel_alpha);
        ImGui::Begin("simlib GUI example", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Backend: %s", backend_name(sl::graphics_backend()));
        ImGui::Text("Window: %d x %d", sl::screen_width(), sl::screen_height());
        ImGui::SliderFloat("Panel alpha", &panel_alpha, 0.25f, 1.0f);
        ImGui::End();
        if (show_demo)
            sl::gui_show_demo_window();
        sl::gui_render();
```

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

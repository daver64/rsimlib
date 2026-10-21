# exlua

Runs a sandboxed Lua script (`assets/scripts/scene.lua`) on a `LuaCanvas` and shows the result.

![exlua screenshot](img/exlua.png)

## What it shows
- `LuaCanvas` running a script file
- Lua drawing bindings rendered into a bitmap
- Reporting script errors via `LuaScriptResult`

## Key code

In `exlua.cpp`, `LuaCanvas::run_file` executes the scene script and reports failure through `LuaScriptResult` before rendering the canvas.
```cpp
    sl::LuaCanvas canvas;

    const sl::LuaScriptResult result = canvas.run_file("assets/scripts/scene.lua");
    if (!result.success)
    {
        // Handle result.error
    }

    canvas.render(sl::screen);
    sl::draw_renderer_label();
    sl::show_video_bitmap();
    process_input();
    sl::wait_for_graphics();
    sl::shutdown();
```
## Running
```bash
./build/exlua
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Close the window | Exit |

## Key APIs
- `LuaCanvas`, `run_file`, `LuaScriptResult` — [Sandboxed Lua](../api_reference.md#sandboxed-lua-scripting-includelua_runtimeh-includelua_canvash) · [`lua_runtime.h`](../../include/lua_runtime.h)

## Source
[src/examples/exlua/exlua.cpp](../../src/examples/exlua/exlua.cpp)
[src/examples/exlua/../../../assets/scripts/scene.lua](../../src/examples/exlua/../../../assets/scripts/scene.lua)

## See also
- [exluaconsole](exluaconsole.md)
- [exresources](exresources.md)

See also the [Lua scripting guide](../scripting_lua.md).

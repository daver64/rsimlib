# exluaconsole

An interactive Lua console: type Lua on the input line and run it live against simlib’s Lua bindings.

![exluaconsole screenshot](img/exluaconsole.png)

## What it shows
- Entering and running Lua at runtime
- Scrolling output and error display
- Sandboxed script execution

## Key code

In `exluaconsole.cpp`, a `LuaCanvas` is initialised with an output callback, and the console is given its own `quit` binding.

```cpp
    lua_canvas.initialise(console_append);
    sol::table app = lua_canvas.runtime().state()["app"];
    app.set_function("quit", []()
                     { running = false; });
    lua_canvas.runtime().state()["quit"] = app["quit"];
```

Entered text is run at runtime with `run_text`, and any script error is appended to the scrolling output.

```cpp
    console_append(std::string(prompt) + input_line);
    if (!input_line.empty())
    {
        const sl::LuaScriptResult result = lua_canvas.run_text(input_line);
        if (!result.success)
            console_append(std::string("Error: ") + result.error);
    }
```

Each frame, the Lua canvas is rendered and the most recent scrollback lines are drawn over it.

```cpp
    lua_canvas.render(sl::screen);
// ...
    for (std::size_t index = first_line; index < total_lines; ++index)
    {
        sl::textout_cached(console_lines[index].cache, font, margin, y, text_colour, console_lines[index].text);
        y += fontheight;
    }
    sl::textout_cached(input_cache, font, margin, y, text_colour, prompt + input_line + "_");
```

## Running
```bash
./build/exluaconsole
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Type text | Edit the input line |
| Backspace | Delete |
| Enter | Run the line |
| Escape | Exit |

## Key APIs
- Lua bindings — [Sandboxed Lua](../api_reference.md#sandboxed-lua-scripting-includelua_runtimeh-includelua_canvash) · [`lua_runtime.h`](../../include/lua_runtime.h)

## Source
[src/examples/exluaconsole/exluaconsole.cpp](../../src/examples/exluaconsole/exluaconsole.cpp)

## See also
- [exlua](exlua.md)

See also the [Lua scripting guide](../scripting_lua.md).

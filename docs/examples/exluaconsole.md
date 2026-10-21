# exluaconsole

An interactive Lua console: type Lua on the input line and run it live against simlib’s Lua bindings.

![exluaconsole screenshot](img/exluaconsole.png)

## What it shows
- Entering and running Lua at runtime
- Scrolling output and error display
- Sandboxed script execution

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

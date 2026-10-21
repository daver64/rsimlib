# exconfig

The JSON configuration subsystem (`sl::Config`): nested dotted-path keys and typed values edited live, saved to and loaded from `config_demo.json`.

![exconfig screenshot](img/exconfig.png)

## What it shows
- Load and save with the config API
- Editing resolution, vsync and audio levels
- Reset to defaults

## Running
```bash
./build/exconfig
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| 1 / 2 | Master volume down / up |
| 3 | Toggle VSync |
| 4 | Cycle difficulty |
| 5 / 6 | Lives |
| T | Cycle theme |
| S | Save file |
| R | Reload file |
| D | Reset defaults |
| Escape | Exit |

## Key APIs
- Config — [JSON Configuration](../api_reference.md#json-configuration-includeconfigh) · [`config.h`](../../include/config.h)

## Source
[src/examples/exconfig/exconfig.cpp](../../src/examples/exconfig/exconfig.cpp)

## See also
- [exdb](exdb.md)
- [exresources](exresources.md)

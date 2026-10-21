# exconfig

The JSON configuration subsystem (`sl::Config`): nested dotted-path keys and typed values edited live, saved to and loaded from `config_demo.json`.

![exconfig screenshot](img/exconfig.png)

## What it shows
- Load and save with the config API
- Editing resolution, vsync and audio levels
- Reset to defaults

## Key code

In `exconfig.cpp`, `Config` loads the JSON file or initializes dotted-path defaults, then reads and writes a typed value while editing.
```cpp
    sl::Config config;
    if (!config.load(CONFIG_FILE))
    {
        init_default_config(config);
        config.save(CONFIG_FILE);
    }
    // ...
                else if (event.key() == sl::Event::Key::digit_1 || event.key() == sl::Event::Key::keypad_1)
                {
                    double vol = config.get<double>("audio.master_volume", 0.5);
                    vol = std::max(0.0, vol - 0.05);
                    config.set("audio.master_volume", vol);
                    status_message = "Master volume decreased";
                    status_colour = {218, 226, 235};
                }
```

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

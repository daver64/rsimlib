# exmodel3d

A model viewer for glTF/GLB, AC3D and FlightGear XML models with clip playback and property-driven animations.

![exmodel3d screenshot](img/exmodel3d.png)

## What it shows
- `load_model` and `load_flightgear_model`
- Clip playback (`play_model_clip`, `update_model_clip`)
- External animation JSON with `load_model_animations_json`
- Property channels driving part visibility and animation via `set_model_property`
- Orbit camera over a `Surface3D`

## Key code

In `exmodel3d.cpp`, the model is loaded (FlightGear `.xml` or a regular format), optional animations JSON is attached, and property channels are set with `set_model_property`:

```cpp
sl::Model *model = extension == ".xml"
                       ? sl::load_flightgear_model(model_path)
                       : sl::load_model(model_path);
// ...
if (!animation_path.empty() && !sl::load_model_animations_json(model, animation_path))
{
    std::fprintf(stderr, "Animation load failed: %s\n", sl::last_error().c_str());
    // ...
}
// ...
auto apply_channel = [&](Channel &channel, float value) {
    channel.value = std::clamp(value, channel.minimum, channel.maximum);
    return sl::set_model_property(model, channel.name, channel.value);
};
```

Clips are started with `play_model_clip` and advanced every frame before the model is drawn into the `Surface3D`:

```cpp
sl::play_model_clip(model, model->clips[static_cast<std::size_t>(index)].name, clip_loop, clip_speed);
// ...
if (!paused)
    sl::update_model_clip(model, static_cast<float>(sl::get_frame_time() / 1000.0));
if (sl::begin_surface_3d(surface, camera))
{
    sl::clear_surface_3d({30, 40, 58});
    sl::draw_model(model, glm::mat4(1.0f));
    sl::end_surface_3d();
}
```

## Running
```bash
./build/exmodel3d [model.glb|model.ac|flightgear-model.xml] [--animations file.json]
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Set `RSIMLIB_SCREENSHOT=<file.png>` to save a screenshot on an early frame.

## Controls
| Input | Action |
|-------|--------|
| Arrow keys | Orbit |
| + / - | Zoom |
| C | Next clip |
| P | Pause |
| L | Toggle loop |
| S / F | Slower / faster |
| Tab / PgUp / PgDn | Select a property channel |
| Space | Toggle the channel |
| , / . | Adjust the channel value |
| Home | Reset channels |
| Escape | Exit |

## Key APIs
- `load_model`, `load_flightgear_model`, `draw_model` — [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh) · [`surface3d.h`](../../include/surface3d.h)
- `play_model_clip`, `update_model_clip`, `set_model_property` — [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh) · [`surface3d.h`](../../include/surface3d.h)

## Source
[src/examples/exmodel3d/exmodel3d.cpp](../../src/examples/exmodel3d/exmodel3d.cpp)

## See also
- [ex3d_surface](ex3d_surface.md)
- [exflight](exflight.md)

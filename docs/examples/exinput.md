# exinput

Input mapping: one `InputMap` binds keyboard, mouse and gamepad inputs to named actions, with live rebinding.

![exinput screenshot](img/exinput.png)

## What it shows
- Binding keys, mouse buttons and gamepad buttons or sticks to named actions
- Moving a square with WASD, arrow keys or the gamepad left stick
- Rebinding the `fire` action at runtime
- Reading mouse position and buttons
- Gamepad initialisation, dead zones and connected-controller listing

## Key code

In `exinput.cpp`, keys, gamepad axes and buttons and mouse buttons are bound to named actions on an `InputMap`.

```cpp
input.bind_key("left", Key::arrow_left);
input.bind_key("left", Key::letter_a);
input.bind_gamepad_axis("left", sl::GamepadAxis::left_x, -1);
// ...
input.bind_gamepad_button("fire", sl::GamepadButton::south);
input.bind_mouse("fire", sl::MouseButton::left);
input.bind_key("fire", Key::space);
```

Each frame the actions move the square, and F2 rebinds `fire` to whatever input is captured next.

```cpp
if (state.input.pressed("remap"))
{
    state.remapping = true;
    state.input.clear_bindings("fire");
}
else if (state.remapping)
{
    if (const auto captured = sl::InputMap::capture();
        captured && !captured->is_key(Key::f2))
    {
        state.input.bind("fire", *captured);
        state.remapping = false;
    }
}

float horizontal = 0.0f, vertical = 0.0f;
state.input.vector("left", "right", "up", "down", horizontal, vertical);
```

The gamepad dead zone and subsystem are set up in `main`, and the mouse state is read for display.

```cpp
sl::gamepad_set_deadzone(0.15f);
if (!sl::gamepad_init())
    sl::gprintf_center(580, {240, 130, 120}, "Gamepad subsystem unavailable");
// ...
sl::gprintf(24, 72, {170, 185, 205}, "Mouse: %d, %d    Buttons: 0x%02X", mouse_x, mouse_y, mouse_buttons);
sl::gprintf(24, 96, {170, 185, 205}, "Gamepads connected: %d    Escape: exit", sl::gamepad_count());
```

## Running
```bash
./build/exinput
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| W A S D / arrow keys / left stick | Move the square |
| Space / left mouse button / gamepad south button | Fire (default `fire` bindings) |
| F2 | Rebind `fire` to the next key, button or stick direction pressed |
| Escape | Exit |

## Key APIs
- `InputMap`, `InputBinding`, `bind_key`, `bind_mouse`, `bind_gamepad_button` — [Events](../api_reference.md#events-input--timing-includeeventh-includeinputh-includesystemh-includegamepadh) · [`event.h`](../../include/event.h)
- `gamepad_init`, `gamepad_count`, `gamepad_name`, `gamepad_set_deadzone` — [Events](../api_reference.md#events-input--timing-includeeventh-includeinputh-includesystemh-includegamepadh) · [`event.h`](../../include/event.h)
- `mouse_x`, `mouse_y`, `mouse_buttons`, `get_frame_time` — [Events](../api_reference.md#events-input--timing-includeeventh-includeinputh-includesystemh-includegamepadh) · [`event.h`](../../include/event.h)

## Source
[src/examples/exinput/exinput.cpp](../../src/examples/exinput/exinput.cpp)

## See also
- [exgui](exgui.md)
- [exphysics](exphysics.md)

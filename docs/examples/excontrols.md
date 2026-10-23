# excontrols

Two instrument panels drawn flat in 2D and driven by the mouse: a light-aircraft cockpit panel (three dials, fuel and temperature meters, throttle slider, toggles, knobs, lamps and buttons) and a factory process cabinet. A small simulation behind each turns the controls into needle and meter readings, and the lamps glow through `Bloom`.

![excontrols screenshot](img/excontrols.png)

## What it shows
- `ControlPanel` layouts (`make_cockpit_panel()`, `make_cabinet_panel()`) and `find()` by label
- Inputs (knob, slider, toggle, momentary and latching buttons) read with `value()` / `on()`; outputs (dial, meter, lamp) written with `set_value()` / `set_on()`
- `control_pointer_2d()` + `update()` for mouse input, and `on_change` for change notifications
- `draw_control_panel()` into a render target, then `Bloom` so lamps and lit meter segments glow

## Key code

```cpp
sl::ControlPanel cockpit = sl::make_cockpit_panel();
const int thr = cockpit.find("THR"), rpm = cockpit.find("RPM");
cockpit.update(dt, sl::control_pointer_2d(x, y, 1.0f, mx, my, down));
cockpit.set_value(rpm, 700.0f + 2200.0f * cockpit.value(thr));
sl::draw_control_panel(sl::screen, cockpit, x, y);
```

## Controls
Mouse only. Cockpit: MASTER, PUMP and START, then raise THR. Cabinet: PUMP 1 or 2, then START; STOP or E-STOP halts it. `B` toggles bloom.

## Running
```
./build/excontrols        # or --gl / --vulkan
```

Tested on OpenGL. See the lesson [Control panels](../tutorial/2d-05-control-panels.md).

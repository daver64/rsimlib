# excockpit

A cockpit and a factory control cabinet in a dark hangar, built from `props3d.h` parts. The mouse operates the controls in the scene: a ray is cast through the camera onto each panel. Lamps and lit meter segments are emissive and bloom through `Hdr`, and each lit lamp adds a small local light.

![excockpit screenshot](img/excockpit.png)

## What it shows
- `ControlAssembly` (`make_cockpit()`, `make_control_cabinet()`): a panel, a body mesh and the pose that mounts one on the other
- `ControlPanelView3D::draw()` for moving needles, knobs, toggles and buttons, and `lights()` for lamp light
- `ControlPanelView3D::pick()` mapping the mouse to panel coordinates, with the pointer routed to the panel under the cursor
- Emissive values above 1 combined with `Hdr` for LED glow

## Key code

```cpp
sl::Pointer pointer;
if (view.pick(camera, w, h, mx, my, panel_world, &pointer))
{
    pointer.down = sl::mouse_button_down(sl::MouseButton::left);
    assembly.panel.update(dt, pointer);
}
view.draw(assembly.panel, panel_world);
```

## Controls
`1` pilot view, `2` cabinet, `3` overview, `L` room light, mouse to operate the controls.

## Running
```
./build/excockpit         # or --gl / --vulkan
```

Tested on OpenGL. See the lesson [A clickable 3D panel](../tutorial/3d-07-a-clickable-3d-panel.md).

# simlib Tutorial

Short, hands-on lessons that teach simlib in order. They complement the [Example Gallery](../examples/README.md): the examples show finished features, the tutorial shows how to build them up from nothing.

[← Back to README](../../README.md)

**Who it's for:** anyone new to simlib. You should know C++; no game programming experience is assumed, and terms are explained when they first appear.

## How the lessons work

- Every lesson is **independent** and ends with a complete program of about 40–70 lines. Copy it into one `.cpp` file, build, run, then try the exercises.
- Do [Lesson 0](00-first-window.md) first. It covers the window, the main loop and building, which every other lesson assumes.
- Lessons load assets such as `assets/textures/atlas_grid.png`, so **run from the repository root**.
- Every lesson links to the matching example and API section for going further.

## Setting up once

Add a target to your own `CMakeLists.txt` (see [Configure a Consumer](../../README.md#configure-a-consumer) for using an installed copy):

```cmake
add_executable(lesson src/lesson.cpp)
target_link_libraries(lesson PRIVATE simlib)
```

Build and run with `./lesson`. Add `--vulkan` (or `--gl`) to choose a graphics backend; every lesson passes its arguments to `configure_graphics_backend_from_args`.

## Lessons

**Start**
- [Lesson 0: Your first window](00-first-window.md): the window, the loop, events and frame pacing.

**Track 1: a 2D game**
1. [Drawing and moving things](2d-01-drawing-and-input.md): shapes, keyboard and mouse.
2. [Sprites and animation](2d-02-sprites-and-animation.md): images, atlases and animated sprites.
3. [Tile maps and collision](2d-03-tilemaps-and-collision.md): a scrolling tile world with solid walls, and click-to-stamp splat decals.
4. [Text and sound](2d-04-text-and-sound.md): on-screen text, sound effects and music.

**Track 2: a 3D scene**
1. [Your first 3D scene](3d-01-first-scene.md): a surface, a camera and meshes.
2. [Materials, light and transparency](3d-02-materials-and-light.md): textures, fog and blending.
3. [Models and a HUD](3d-03-models-and-hud.md): loading models and drawing 2D over 3D.
4. [Many objects and threads](3d-04-many-objects-and-threads.md): thousands of draws and parallel recording.
5. [Local lights and shadows](3d-05-local-lights-and-shadows.md): point and spot lights, sun and light shadows.

**Track 3: simulation**
1. [Physics bodies](sim-01-physics-bodies.md): gravity, collisions and contacts.
2. [Joints](sim-02-joints.md): hinges, springs and dragging with the mouse.
3. [Weather](sim-03-weather.md): rain, hail and snow that change over time.

## Where next

[Showcase Guide](../showcases.md) for bigger demos, the [API reference](../api_reference.md) for everything else.

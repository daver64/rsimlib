# simlib

`simlib` is a small Allegro-style 2D game/rendering library built on SDL2 and
OpenGL, with bitmap drawing primitives, TrueType text rendering, SDL_mixer
audio, a shader/effects layer, ZIP resource archives, Perlin/simplex noise,
and a sandboxed embedded Lua console (via [sol2](https://github.com/ThePhD/sol2)).

> **Experimental software:** simlib is under active development and expected to
> grow organically. APIs, behavior, and project structure may change as the
> library evolves.

This repository contains:

- **`simlib`** — the static engine library (`src/engine/`).
- **`sltest`** — a sample game/test application built on `simlib`, with a
    menu, simple physics/entity system, and Lua REPL console (`src/applications/sltest/`).
- **`slpack`** — a CLI tool for packing assets into ZIP archives (`src/applications/slpack/`).
- **`exhello`** — a minimal "hello world" example (`src/applications/exhello/`).

## Prerequisites

- CMake 3.16+
- A C++17 compiler
- SDL2, SDL2_ttf, SDL2_image, SDL2_mixer (development packages)
- OpenGL, libpng, zlib

On Debian/Ubuntu:

```bash
sudo apt install cmake build-essential libsdl2-dev libsdl2-ttf-dev \
    libsdl2-image-dev libsdl2-mixer-dev libpng-dev zlib1g-dev libgl1-mesa-dev
```

Dear ImGui, Lua, and sol2 are fetched automatically via CMake's `FetchContent`
— no manual setup required.

## Building

```bash
cmake -S . -B build
cmake --build build
```

This produces three executables in the repository root: `sltest`, `slpack`,
and `exhello`.

To run the test suite (currently covers the ZIP resource archive reader):

```bash
cmake --build build --target resource_test
ctest --test-dir build
```

## Running the examples

```bash
./exhello   # minimal window + text rendering demo
./sltest    # sample game: menu, physics playground, Lua console
./slpack    # pack files into a ZIP resource archive
```

In `sltest`, press `4` from the menu to open the embedded Lua console —
type `quit()` to exit, `os.clock()` / `os.time()` for the sandboxed clock,
or press `ESC` to return to the menu.

## Quick start

A minimal application only needs `sl.h`, which pulls in the whole public API:

```cpp
#include "sl.h"

int main(int argc, char *argv[])
{
    if (!simlib::set_gfx_mode(simlib::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }

    simlib::Font *font = simlib::get_default_monospace_font();
    const int fontheight = simlib::text_height(font);
    simlib::Colour text_colour{0, 255, 0};
    simlib::clear_to_colour(simlib::screen, simlib::Colour{45, 48, 56});

    simlib::gprintf_center(1 + fontheight, text_colour, "Hello, simlib!");
    simlib::show_video_bitmap();
    simlib::end_frame();

    simlib::rest(5000);
    simlib::shutdown();
    return 0;
}
```

Link your CMake target against the `simlib` library target:

```cmake
add_executable(my_app src/my_app.cpp)
target_link_libraries(my_app PRIVATE simlib)
```

## Event loop and input

```cpp
SDL_Event event;
bool running = true;
while (running)
{
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT) running = false;
        simlib::display_handle_event(event);
    }

    simlib::clear_to_colour(simlib::screen, simlib::Colour{0, 0, 0});
    // ... draw your frame ...
    simlib::show_video_bitmap();
    simlib::end_frame();
}
```

## Gamepads

`simlib` provides polling for standard mapped controllers through SDL's game
controller layer. Initialise it explicitly, forward SDL events to support
controller hot-plugging, and shut it down before `simlib::shutdown()`.

```cpp
if (!simlib::gamepad_init())
{
    // No controller subsystem is available.
}

while (running)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        simlib::gamepad_handle_event(event);
    }

    if (simlib::gamepad_connected(0))
    {
        const float move_x = simlib::gamepad_axis(0, simlib::GamepadAxis::left_x);
        if (simlib::gamepad_button(0, simlib::GamepadButton::south))
        {
            // Confirm, jump, or fire.
        }
    }
}

simlib::gamepad_shutdown();
```

`gamepad_count()` returns connected mapped controllers and `gamepad_name(index)`
returns a device name. Axis values are normalized to $[-1, 1]$ and filtered by a
default dead zone of $0.15$; configure it with `gamepad_set_deadzone()`.

## Drawing primitives

All shape/sprite coordinates are `float`, so positions can move smoothly
frame-to-frame; pixel-level operations (`putpixel`/`getpixel`) remain `int`.

```cpp
using simlib::Colour;

simlib::rectfill(simlib::screen, 10.0f, 10.0f, 110.0f, 60.0f, Colour{200, 40, 40});
simlib::circlefill(simlib::screen, 300.0f, 200.0f, 32.0f, Colour{40, 160, 40});
simlib::line(simlib::screen, 0.0f, 0.0f, 800.0f, 600.0f, Colour{255, 255, 0});
simlib::triangle(simlib::screen, 400.0f, 100.0f, 450.0f, 200.0f, 350.0f, 200.0f, Colour{0, 180, 255});
```

## Bitmaps and sprites

```cpp
simlib::Bitmap *sprite = simlib::load_bitmap("assets/textures/balloon_red.png");
if (sprite)
{
    simlib::draw_sprite(sprite, 100.0f, 100.0f);
    simlib::draw_sprite_stretched(sprite, 200.0f, 100.0f, 64, 64);
}
```

## Text rendering

```cpp
simlib::Font *font = simlib::get_default_monospace_font();
simlib::Colour green{0, 255, 0};

simlib::textout(font, 10, 10, green, "Raw text output");
simlib::gprintf(10, 40, green, "Score: %d", 42);
simlib::gprintf_center(80, green, "Centered text");
```

## Audio

```cpp
simlib::audio_fx_init();
simlib::Sample *sfx = simlib::load_sample("assets/sfx/jump.wav");
simlib::play_sample(sfx, /*volume*/255, /*pan*/128);

simlib::music_init();
simlib::Stream *music = simlib::load_stream("assets/music/theme.ogg");
// see audio.h for playback controls
```

## Resource archives (ZIP)

Load assets bundled by `slpack` directly from a ZIP archive:

```cpp
simlib::Archive archive;
if (archive.open("assets.zip"))
{
    simlib::Bitmap *bitmap = simlib::load_bitmap(archive, "textures/balloon_red.png");
    simlib::Sample *sample = simlib::load_sample(archive, "sfx/jump.wav");
}
```

## Sandboxed Lua scripting

`simlib`-based applications can embed a Lua console via
[sol2](https://github.com/ThePhD/sol2); see `src/applications/sltest/game_lua.cpp`
in `sltest` for a complete example, including:

- A restricted `open_libraries` set (no `io`, `package`, `debug`, or the real `os` library).
- `dofile`/`loadfile`/`load` disabled to prevent filesystem/code-injection escapes.
- A minimal safe `os` table (`os.time()`, `os.clock()` only).
- An `app` table reserved for future game-specific bindings.

```cpp
sol::state lua;
lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::math, sol::lib::table);
lua["dofile"] = sol::nil;
lua["loadfile"] = sol::nil;
lua["load"] = sol::nil;

sol::table app = lua.create_named_table("app");
app.set_function("quit", []() { /* ... */ });
```

### `sltest` Lua drawing API

The `sltest` console also exposes persistent drawing commands under `app`.
Each call is replayed every frame until `app.clear_drawings()` is called.
Colours use 8-bit RGB values with an optional alpha channel.

```lua
app.pixel(x, y, r, g, b [, a])
app.line(x1, y1, x2, y2, r, g, b [, a])
app.circle(x, y, radius, r, g, b [, a])
app.circlefill(x, y, radius, r, g, b [, a])
app.rect(left, top, right, bottom, r, g, b [, a])
app.rectfill(left, top, right, bottom, r, g, b [, a])
app.ellipse(x, y, radius_x, radius_y, r, g, b [, a])
app.ellipsefill(x, y, radius_x, radius_y, r, g, b [, a])
app.triangle(x1, y1, x2, y2, x3, y3, r, g, b [, a])
app.trianglefill(x1, y1, x2, y2, x3, y3, r, g, b [, a])
app.clear_drawings()
```

For example, this adds a persistent red diagonal behind the console text:

```lua
app.line(20, 120, 700, 500, 255, 0, 0)
```

Sprites are loaded once into a console-local cache and referred to by ID in
draw commands. Paths are relative to `assets/`, and duplicate IDs are rejected.

```lua
ok, message = app.load_sprite("red_balloon", "textures/balloon_red.png")
print(ok, message)

app.sprite("red_balloon", 100, 150)
app.sprite_stretched("red_balloon", 300, 150, 96, 128)
```

`app.sprite()` and `app.sprite_stretched()` return `false` when their ID has
not been loaded. Sprite draw commands persist alongside shapes until cleared;
unloading a sprite also removes its retained draw commands.

```lua
app.unload_sprite("red_balloon")
app.clear_sprites()
app.clear_drawings()
```

## Simple physics / entities

`sltest`'s playing screen (`src/applications/sltest/entity.cpp`) shows a small
`GameObject` system with gravity, mass, drag, and circle/AABB collision:

```cpp
#include "entity.h"

std::vector<game::GameObject> objects;
objects.push_back(game::make_circle_object(sprite, 100.0f, 100.0f, /*radius*/16.0f, /*mass*/1.0f));

// once per frame:
game::physics_step(objects, dt_seconds);
game::resolve_collisions(objects);
game::constrain_to_screen(objects);
game::render_objects(objects);
```

## `sltest` game source guide

`sltest` is a small, deliberately direct example of how an application can
compose `simlib` services. It is not a framework: game state is module-local,
the `Mode` enum selects the current screen, and each screen provides one input
handler and one update/render function.

```mermaid
flowchart TD
    main[main.cpp] --> initialise[game::initialise]
    main --> events[game::handle_events]
    main --> render[game::update_and_render]
    events --> mode_input[Current mode input handler]
    render --> mode_render[Current mode renderer]
    mode_render --> display[simlib display, drawing, ImGui, text, particles]
```

### `main.cpp`

The executable entry point. It calls `game::initialise()`, repeatedly polls
events and renders while `game::is_running()` is true, then calls
`game::shutdown()`. This demonstrates the smallest application loop around
`simlib`; timing is performed inside the active mode renderers via
`simlib::end_frame()` and `simlib::get_frame_time()`.

### `game.h`

The game-facing contract shared by all source files. `Mode` is the screen state
machine (`menu`, `playing`, `paused`, `help`, `gameover`, `settings`, and
`lua_console`). It declares the two-function interface implemented by each
screen: `handle_*_input(SDL_Event)` processes a relevant SDL event and
`update_and_render_*()` draws one frame. It also exposes the three balloon
`simlib::Bitmap` pointers loaded at startup for the physics example.

### `game.cpp`

Owns application-level state and routes work to the selected mode. `running`
ends the main loop, while `current_mode` selects both event and rendering
dispatch. `initialise()` creates an 800x600 SDL/OpenGL display through
`simlib::set_gfx_mode()`, starts ImGui, audio, and frame pacing, then loads
balloon textures with `simlib::load_bitmap()`. `handle_events()` receives SDL
events, delegates them to the active mode, and forwards display resize and
ImGui events through `simlib::display_handle_event()` and
`simlib::gui_handle_event()`. `shutdown()` releases the Lua canvas before the
graphics context and then shuts down GUI, display, audio, and core services.

### `game_menu.cpp`

Implements the main navigation screen. Numeric keys preserve keyboard access,
while its ImGui buttons assign `current_mode` to enter gameplay, settings,
help, or the Lua console. The renderer clears the `simlib::screen` bitmap,
opens an ImGui frame with `simlib::new_frame()`, creates a fixed centred window,
submits its widgets, calls `simlib::render()`, swaps with
`simlib::show_video_bitmap()`, and caps the frame through `simlib::end_frame()`.

### `game_settings.cpp`

Provides a minimal settings-screen template using the same ImGui frame lifecycle
as the menu. It demonstrates disabled controls with `ImGui::BeginDisabled()`
and an active third option that currently writes a message to standard output.
The Back button and Escape key return to the menu. This is the place to connect
real configuration values to `simlib` or application settings in the future.

### `game_help.cpp`, `game_paused.cpp`, and `game_gameover.cpp`

These are intentionally small modal screens. Each clears the background, builds
a fixed centred ImGui window, provides a Back/Return action, submits ImGui draw
data, and presents the frame. Their input handlers also support Escape to return
to the menu. Together they show that a mode needs no special base class: it only
needs to participate in the dispatcher declared by `game.h`.

### `game_playing.cpp`

Contains the gameplay demonstration and owns its transient simulation state.
On first entry, `ensure_playing_objects_initialised()` creates three circular
`GameObject`s from the balloon bitmaps, assigns mass and restitution, and saves
an initial copy for reset. It also configures two `simlib::ParticleEmitter`s:
a stationary fountain and a trail that follows the red balloon.

Each frame clamps `simlib::get_frame_time()` to avoid unstable physics after a
stall, advances the objects with `physics_step()`, resolves collisions, and
constrains them to `simlib::screen_width()` and `simlib::screen_height()`. It
updates emitters, renders particles into a `simlib::create_render_target()`
offscreen bitmap, draws the objects, then composites the target using
`simlib::draw_sprite_v_flip()` because render-target texture coordinates are
vertically inverted. Space resets the saved state; Escape returns to the menu.

### `entity.cpp`

Implements the small physics and rendering layer used by the playing mode.
`make_circle_object()` and `make_aabb_object()` initialize the two supported
collider shapes. `physics_step()` applies gravity, drag, and velocity
integration. `resolve_collisions()` checks circle-circle, AABB-AABB, and mixed
circle/AABB overlap, first correcting penetration by inverse mass and then
applying an impulse based on restitution. `constrain_to_screen()` bounces
dynamic objects from the display bounds obtained from `simlib`. Finally,
`render_objects()` uses `simlib::draw_sprite_stretched()` to place each object’s
bitmap around its physics centre.

### `game_lua.cpp`

Implements the interactive console UI, not the scripting renderer itself. It
uses SDL text-input events to build a command line, keeps bounded scrollback in
cached `simlib::TextCache` textures, and executes submitted text through an
engine-owned `simlib::LuaCanvas`. `LuaCanvas::render(simlib::screen)` redraws
the script’s retained background, shapes, and sprites before console text is
drawn over it. The console adds only the game-specific `quit()` binding; the
engine supplies the sandbox, Lua output callback, drawing commands, and sprite
cache. `shutdown_lua_console()` resets canvas resources before the display’s
OpenGL context is released.

## Project layout

```
include/                 Public simlib headers (draw.h, font.h, audio.h, ...)
src/engine/               simlib library implementation
src/applications/sltest/  sltest sample game (menu, physics, Lua console)
src/applications/slpack/  slpack asset-packing CLI
src/applications/exhello/ exhello minimal example
assets/                   Textures, music, and sound effects used by sltest
```

## Documentation

If Doxygen is installed, generate API docs with:

```bash
cmake --build build --target docs
```

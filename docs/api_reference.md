# Core API Reference

[← Back to README](../README.md)

This reference details the public C++ API provided by `simlib`. Including `sl.h` pulls in the entire public engine interface. Individual subsystem headers (located under [include/](include/)) can also be included independently when required.

---

## Table of Contents
1. [Error Handling](#error-handling-includeerrorh)
2. [Application Lifecycle & Display](#application-lifecycle--display-includedisplayh)
3. [Events, Input & Timing](#events-input--timing-includeeventh-includeinputh-includesystemh-includegamepadh)
4. [Bitmaps, Offscreen Render Targets & Drawing](#bitmaps-offscreen-render-targets--drawing-includedrawh)
5. [Typography & Text Rendering](#typography--text-rendering-includefonth)
6. [Shaders, Compute & Storage Buffers](#shaders-compute--storage-buffers-includegraphics_fxh)
7. [Post-Processing Graphics Effects](#post-processing-graphics-effects-includegraphics_fxh)
8. [2D Radial Lighting & Polygon Shadows](#2d-radial-lighting--polygon-shadows-includegraphics_fxh)
9. [Box2D Physics System](#box2d-physics-system-includephysicsh)
10. [3D Surfaces](#3d-surfaces-includesurface3dh)
11. [Voxel Chunks & World](#voxel-chunks--world-includevoxelh)
12. [Audio Subsystem (Samples & Streams)](#audio-subsystem-samples--streams-includeaudioh)
13. [Cellular Automata Fluid & Granular Simulation](#cellular-automata-fluid--granular-simulation-includefluidh)
14. [Particle Systems & Emitters](#particle-systems--emitters-includeparticlesh)
    - [Precipitation](#precipitation-includeprecipitationh)
15. [Procedural FastNoise Generation](#procedural-fastnoise-generation-includenoiseh)
16. [Embedded SQLite Persistence](#embedded-sqlite-persistence-srcexamplesexdbrdbh)
17. [Resource Archives](#resource-archives-includeresourceh)
18. [Isometric Math & Projection](#isometric-math--projection-includeisometrich)
19. [JSON Configuration](#json-configuration-includeconfigh)
20. [Tile Maps](#tile-maps-includetilemaph)
21. [3D Fluid Simulation](#3d-fluid-simulation-includefluid3dh)
22. [3D Flight & Orbital Dynamics](#3d-flight--orbital-dynamics-includeflighth)
23. [Chunked-LOD Terrain](#chunked-lod-terrain-includeterrainh)
24. [Atlases & Animated Sprites](#atlases--animated-sprites-includeatlash-includeanimated_spriteh)
25. [Procedural Audio Synthesis](#procedural-audio-synthesis-includeaudio_synthh)
26. [Video Playback](#video-playback-includevideoh)
27. [Dear ImGui Integration](#dear-imgui-integration-includeguih)
28. [Tile-Grid Pathfinding](#tile-grid-pathfinding-includepathfindingh)
29. [Networking](#networking-includeneth)
30. [Sandboxed Lua Scripting](#sandboxed-lua-scripting-includelua_runtimeh-includelua_canvash)

---

## Error Handling ([include/error.h](../include/error.h))

`simlib` records thread-local error messages when subsystem initialisation, file loading, or GPU resource allocation fails.

### Functions

#### `last_error()`
```cpp
const std::string &last_error();
```
- **Description**: Returns the most recent error message recorded by `simlib` on the calling thread.
- **Parameters**: None.
- **Returns**: A reference to a thread-local string describing the last error.
- **Example**:
```cpp
if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600)) {
    std::cerr << "Graphics init error: " << sl::last_error() << std::endl;
}
```

#### `clear_error()`
```cpp
void clear_error();
```
- **Description**: Clears any thread-local error string recorded by `simlib`.
- **Parameters**: None.
- **Returns**: None.

---

## Application Lifecycle & Display ([include/display.h](../include/display.h))

The display subsystem handles window creation, graphics backend selection (OpenGL 4.3, Vulkan, Direct3D), resolution configuration, vsync, and viewport management.

### Types & Enumerations

- `enum class GraphicsBackend { opengl, vulkan, d3d12 }`: Selects the hardware rendering backend.
- `constexpr int GFX_AUTODETECT_WINDOWED = 1`: Auto-detected windowed driver mode.
- `constexpr int GFX_AUTODETECT_FULLSCREEN = 2`: Auto-detected desktop fullscreen driver mode.

### Functions

#### `set_graphics_backend(backend)`
```cpp
bool set_graphics_backend(GraphicsBackend backend);
```
- **Description**: Explicitly selects the graphics rendering backend prior to window creation.
- **Parameters**:
  - `backend` (`GraphicsBackend`): The desired rendering backend (`opengl`, `vulkan`, or `d3d12`).
- **Returns**: `true` if the specified backend is available in the current build; `false` otherwise.

#### `configure_graphics_backend_from_args(argc, argv)`
```cpp
bool configure_graphics_backend_from_args(int argc, char *argv[]);
```
- **Description**: Parses command-line arguments for backend flags (`--gl`, `--vulkan`, `--d3d12`) and configures the active backend accordingly. Defaults to Direct3D 12 on Windows and OpenGL elsewhere if no flag is supplied.
- **Parameters**:
  - `argc` (`int`): Count of command-line arguments.
  - `argv` (`char**`): Array of argument strings.
- **Returns**: `true` if a valid backend was configured.
- **Example**:
```cpp
int main(int argc, char **argv) {
    sl::configure_graphics_backend_from_args(argc, argv);
    sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600);
}
```

#### `set_gfx_mode(driver, width, height, virtualWidth = 0, virtualHeight = 0)`
```cpp
bool set_gfx_mode(int driver, int width = 0, int height = 0, int virtualWidth = 0, int virtualHeight = 0);
```
- **Description**: Creates the main application window, initialises the selected graphics context, and constructs the primary display framebuffer (`sl::screen`).
- **Parameters**:
  - `driver` (`int`): Driver selection (`sl::GFX_AUTODETECT_WINDOWED` or `sl::GFX_AUTODETECT_FULLSCREEN`).
  - `width` (`int`): Window width in pixels; both `width` and `height` may be omitted for fullscreen mode to use the primary display's desktop resolution.
  - `height` (`int`): Window height in pixels.
  - `virtualWidth` (`int`, optional): Logical drawing canvas width (0 defaults to `width`).
  - `virtualHeight` (`int`, optional): Logical drawing canvas height (0 defaults to `height`).
- **Returns**: `true` on successful initialisation; `false` on failure.

#### `display_handle_event(event)`
```cpp
void display_handle_event(const Event &event);
```
- **Description**: Processes display-related SDL events (such as window resizing or DPI changes) to keep render targets and viewports properly sized.
- **Parameters**:
  - `event` (`const Event&`): The polled event to process.
- **Returns**: None.

#### `set_window_title(title)`
```cpp
void set_window_title(const char *title);
```
- **Description**: Updates the caption in the native window title bar.
- **Parameters**:
  - `title` (`const char*`): UTF-8 string for the window title.
- **Returns**: None.

#### `set_fullscreen(enabled)` / `toggle_fullscreen()`
```cpp
bool set_fullscreen(bool enabled);
bool toggle_fullscreen();
```
- **Description**: Switches or toggles desktop fullscreen mode.
- **Parameters**:
  - `enabled` (`bool`): `true` to enter fullscreen, `false` for windowed mode.
- **Returns**: `true` if the display mode transition succeeded.

#### `set_vsync(enabled)`
```cpp
bool set_vsync(bool enabled);
```
- **Description**: Configures vertical presentation synchronisation.
- **Parameters**:
  - `enabled` (`bool`): `true` to lock presentation to the monitor refresh rate, `false` to disable vsync.
- **Returns**: `true` if the swap interval was set successfully.

#### `screen_width()` / `screen_height()`
```cpp
int screen_width();
int screen_height();
```
- **Description**: Returns the actual drawable window dimensions in pixels.
- **Returns**: Current window width or height in pixels.

#### `restore_window_viewport()`
```cpp
void restore_window_viewport();
```
- **Description**: Restores the backend GPU viewport to match the primary window dimensions (useful after rendering into offscreen framebuffers).
- **Parameters**: None.
- **Returns**: None.

---

## Events, Input & Timing ([include/event.h](../include/event.h), [include/input.h](../include/input.h), [include/system.h](../include/system.h), [include/gamepad.h](../include/gamepad.h))

`simlib` abstracts window, keyboard, mouse, and gamepad events into unified event structures, alongside high-resolution frame pacing utilities.

### Functions

#### `poll_event(event)`
```cpp
bool poll_event(Event *event);
```
- **Description**: Retrieves the next pending input or window event from the queue.
- **Parameters**:
  - `event` (`Event*`): Pointer to an `Event` object to receive event details.
- **Returns**: `true` if an event was popped; `false` when the event queue is empty.
- **Example**:
```cpp
sl::Event event;
while (sl::poll_event(&event)) {
    if (event.type() == sl::Event::Type::quit) running = false;
    sl::display_handle_event(event);
}
```

#### `key_down(key)`
```cpp
bool key_down(Event::Key key);
int key_scancode(Event::Key key);   // backend scancode, used by InputMap
void start_text_input();            // begin Event::Type::text_input delivery
void stop_text_input();
bool key_down(SDL_Scancode key);
```
- **Description**: Queries whether a specific keyboard key is currently physically held down. Client applications should prefer `Event::Key` (e.g., `sl::Key::w`, `sl::Key::space`, `sl::Key::escape`) to avoid exposing platform/SDL scancodes.
- **Parameters**:
  - `key` (`Event::Key` or `SDL_Scancode`): Key identifier to test.
- **Returns**: `true` if the key is held; `false` otherwise.

#### `MouseButton`
```cpp
enum class MouseButton : std::uint8_t {
    left = 1,
    middle = 2,
    right = 3,
    x1 = 4,
    x2 = 5
};
```
- **Description**: Identifies mouse buttons portably without SDL constants. Bitmasks `MOUSE_BUTTON_LEFT_MASK`, `MOUSE_BUTTON_MIDDLE_MASK`, and `MOUSE_BUTTON_RIGHT_MASK` are also available.

#### `mouse_button_down(button)`
```cpp
bool mouse_button_down(MouseButton button);
bool mouse_button_down(int button);
```
- **Description**: Tests whether a specific mouse button is currently held down.
- **Parameters**:
  - `button` (`MouseButton` or 1-based `int`): The button to query.
- **Returns**: `true` if currently pressed; `false` otherwise.

#### `mouse_x()` / `mouse_y()` / `mouse_buttons()`
```cpp
int mouse_x();
int mouse_y();
std::uint32_t mouse_buttons();
```
- **Description**: Queries the current cursor position (in window pixel coordinates) and held mouse button bitmask.
- **Returns**: Integer coordinates or bitfield (`MOUSE_BUTTON_LEFT_MASK`, `MOUSE_BUTTON_RIGHT_MASK`, etc.).

#### `set_relative_mouse_mode(enabled)` / `relative_mouse_mode()`
```cpp
bool set_relative_mouse_mode(bool enabled);
bool relative_mouse_mode();
```
- **Description**: Enables or disables relative mouse mode (captures cursor, hides pointer, and reports continuous relative motion deltas), or queries current state.
- **Parameters**: `enabled` (`bool`): `true` to enable relative mode, `false` to disable.
- **Returns**: `true` on success.

#### `mouse_delta(dx, dy)` / `mouse_relative_state(dx, dy)`
```cpp
void mouse_delta(int *dx, int *dy);
void mouse_delta(int &dx, int &dy);
std::uint32_t mouse_relative_state(int *dx, int *dy);
```
- **Description**: Queries relative mouse motion deltas accumulated since the last check, and optionally returns the button bitmask.
- **Parameters**: `dx`, `dy`: References or pointers to receive horizontal and vertical motion deltas.

#### `gamepad_init()` / `gamepad_count()` / `gamepad_connected(index)`
```cpp
bool gamepad_init();
int gamepad_count();
bool gamepad_connected(int index);
```
- **Description**: Initialises controller subsystems, counts active gamepads, and tests connection status by index.
- **Parameters**: `index` (`int`): 0-based controller device index.

#### `time_ms()`
```cpp
std::uint64_t time_ms();
```
- **Description**: Returns elapsed time in milliseconds since system initialisation using SDL's monotonic timer.
- **Returns**: 64-bit integer millisecond timestamp.

#### `set_fps(fps)` / `get_fps()` / `get_frame_time()`
```cpp
void set_fps(int fps);
int get_fps();
double get_frame_time();
```
- **Description**: Sets or queries the target frame rate pacing cap and returns the duration of the last completed frame in milliseconds.
- **Parameters**:
  - `fps` (`int`): Target frame rate (e.g. `60`); passing `0` disables frame pacing.

#### `end_frame()`
```cpp
void end_frame();
```
- **Description**: Finishes frame presentation, calculates frame delta, and sleeps if necessary to maintain the configured target FPS.
- **Parameters**: None.

#### `rest(milliseconds)`
```cpp
void rest(std::uint32_t milliseconds);
```
- **Description**: Delays execution of the calling thread for the specified duration using `SDL_Delay()`.
- **Parameters**:
  - `milliseconds` (`std::uint32_t`): Duration to sleep in milliseconds.

#### Frame statistics (`get_frame_stats`, `draw_frame_stats`)
```cpp
struct FrameStats { double frame_time_ms, average_fps; uint32_t mesh_draws, instanced_draws, instances, blended_draws; uint64_t frame_number; };
FrameStats get_frame_stats();
void draw_frame_stats(Font *font, int x, int y);   // include/font.h
```
- **Description**: `end_frame()` publishes the previous frame's timing and 3D draw counts (`draw_mesh` calls, `draw_mesh_instanced` calls and their instances, and how many used alpha/additive blending). `draw_frame_stats` prints them as a small overlay; call it last in the frame. Pass `nullptr` for the default monospace font. Terrain and sky draws are not counted.

---

### Input actions ([include/input_actions.h](../include/input_actions.h))
Optional: nothing else in simlib depends on it, and raw `key_down()`, `mouse_buttons()` and `gamepad_*()` keep working alongside it. Name your game actions and bind any mix of inputs to each.
```cpp
sl::InputMap input;
input.bind_key("jump", sl::Event::Key::space);
input.bind_gamepad_button("jump", sl::GamepadButton::south);
input.bind_gamepad_axis("left", sl::GamepadAxis::left_x, -1);
input.bind_key("left", sl::Event::Key::letter_a);
// each frame:
input.update();
if (input.pressed("jump")) { /* ... */ }
float x = input.axis("left", "right");
```
- **Bindings**: `bind_key`, `bind_mouse`, `bind_gamepad_button`, `bind_gamepad_axis` (one direction per binding, e.g. `left_x` with -1), `bind_touch`, or `bind(action, InputBinding)`. An action can have any number of bindings and the strongest one wins. Gamepad bindings match any connected pad unless you give an index.
- **Queries** (after `update()`): `value()` (0-1, analogue aware), `down()`, `pressed()` and `released()` (true for one frame), `axis(negative, positive)` and `vector(left, right, up, down, x, y)` (length capped at 1 so diagonals are not faster). `press_threshold` (default 0.5) sets when an analogue input counts as down, with a little hysteresis on release. Gamepad dead zones come from `gamepad_set_deadzone()`.
- **Remapping**: `InputMap::capture()` returns the first input currently held, `describe(binding)` gives a label such as "Space" or "Pad South", `clear_bindings()` removes an action's bindings, and `save()` / `load()` convert to and from a text form for a settings file. `load()` leaves the map untouched when the text is malformed.
- **Virtual input**: `inject(action, value)` feeds an action for the next `update()` only, for on-screen touch buttons, AI, replays and tests.
- **Touch**: `bind_touch` triggers while any finger is down; `touch_points()` returns finger positions in screen coordinates for on-screen controls.
- Polling means no event hookup is required. Keys are bound by `SDL_Scancode`. See [`exinput`](../src/examples/exinput/exinput.cpp), which includes an F2 remap.

---

## Bitmaps, Offscreen Render Targets & Drawing ([include/draw.h](../include/draw.h))

Bitmaps represent 2D RGBA8 pixel buffers in RAM, on the GPU, or attached as offscreen framebuffer render targets.

### Key Types

#### `Colour`
```cpp
struct Colour {
    Uint8 red;
    Uint8 green;
    Uint8 blue;
    Uint8 alpha = 255;
};
```
Pre-defined constants available: `sl::White`, `sl::Black`, `sl::Red`, `sl::Green`, `sl::Blue`, `sl::Yellow`, `sl::Cyan`, `sl::Magenta`, `sl::Gold`, `sl::Orange`, `sl::Purple`, etc.

#### `Bitmap`
```cpp
struct Bitmap {
    int width = 0;
    int height = 0;
    std::vector<Uint8> pixels;
    std::uint32_t gpu_texture = 0;
    std::uint32_t fbo = 0; // Non-zero for offscreen render targets
    bool ram_dirty = false;
    bool gpu_dirty = false;
    BitmapKind kind = BitmapKind::Bitmap;
    Bitmap *normal_map = nullptr;
    bool smooth = false;
    float anisotropy = 1.0f;
    bool mipmaps = false;
};
```

`smooth` selects bilinear filtering, `anisotropy` requests anisotropic filtering (`1.0f` disables it), and `mipmaps` builds a full mip chain when the pixels are uploaded, for cleaner minified sampling (distant 3D surfaces, scaled-down sprites). Mipmaps are off by default so pixel art stays crisp; models loaded by `load_model()` enable them, with 16x anisotropy, on their textures. Anisotropy is clamped to the active renderer's supported maximum. All three settings are read when the GPU texture is first created, so set them before the first draw or `upload_bitmap()`. Mip levels are regenerated on each upload of the whole bitmap; offscreen render targets are not mipmapped.

Global screen pointer: `extern Bitmap *screen;`

### Functions

#### `create_bitmap(width, height)` / `create_video_bitmap(width, height)`
```cpp
Bitmap *create_bitmap(int width, int height);
Bitmap *create_video_bitmap(int width, int height);
```
- **Description**: Allocates a RAM-backed pixel buffer (`create_bitmap`) or a GPU texture (`create_video_bitmap`).
- **Parameters**: `width`, `height` (`int`): Dimensions in pixels (must be > 0).
- **Returns**: Pointer to the created `Bitmap`, or `nullptr` on allocation failure.

#### `create_render_target(width, height)`
```cpp
Bitmap *create_render_target(int width, int height);
```
- **Description**: Allocates an offscreen GPU render target with an attached framebuffer object (`fbo`), allowing drawing commands to render into a texture.
- **Parameters**: `width`, `height` (`int`): Framebuffer dimensions in pixels.
- **Returns**: Pointer to the allocated render target bitmap.

#### `begin_render_target(target)` / `end_render_target()` / `clear_render_target(colour)`
```cpp
bool begin_render_target(Bitmap *target);
void end_render_target();
void clear_render_target(Colour colour);
```
- **Description**: Redirects subsequent drawing commands (`rectfill`, `draw_sprite`, `textout`, etc.) to render into `target`. `end_render_target()` restores rendering back to the main window.
- **Parameters**:
  - `target` (`Bitmap*`): Offscreen target created with `create_render_target()`.
  - `colour` (`Colour`): Clear colour for the render target.
- **Example**:
```cpp
sl::Bitmap *rt = sl::create_render_target(256, 256);
sl::begin_render_target(rt);
sl::clear_render_target(sl::Colour{20, 20, 30});
sl::circlefill(rt, 128, 128, 50, sl::Red);
sl::end_render_target();

// Draw offscreen render target as a sprite on screen
sl::draw_sprite(rt, 100, 100);
```

#### `load_bitmap(path)` / `save_bitmap(bitmap, path)` / `destroy_bitmap(bitmap)`
```cpp
Bitmap *load_bitmap(const std::string &path);
bool save_bitmap(Bitmap *bitmap, const std::string &path);
void destroy_bitmap(Bitmap *bitmap);
```
- **Description**: Loads an image file (PNG, JPG, BMP) into a bitmap, exports a bitmap as a PNG image, or destroys a bitmap and frees all associated RAM/GPU memory.

#### `upload_bitmap(bitmap)` / `download_bitmap(bitmap)`
```cpp
bool upload_bitmap(Bitmap *bitmap);
bool download_bitmap(Bitmap *bitmap);
```
- **Description**: Synchronises pixel data from CPU RAM to the GPU texture (`upload_bitmap`) or reads GPU texture pixels back into CPU RAM (`download_bitmap`).

#### `putpixel(bitmap, x, y, colour)` / `getpixel(bitmap, x, y)` / `flood_fill(...)`
```cpp
void putpixel(Bitmap *bitmap, int x, int y, Colour colour);
Colour getpixel(Bitmap *bitmap, int x, int y);
void flood_fill(Bitmap *bitmap, int x, int y, Colour colour);
```
- **Description**: Writes or reads an individual pixel, or performs a 4-way flood fill starting at `(x, y)`.

#### `draw_points(bitmap, points, count)`
```cpp
struct PointVertex { float x, y; Colour colour; };
void draw_points(Bitmap *bitmap, const PointVertex *points, int count);
```
- **Description**: Draws many single-pixel points to the screen in one batched GPU submission (e.g. a CPU-readback GPU particle system). Only supported on `sl::screen`; unlike `putpixel()`, this issues a single draw call for the whole array instead of one per point.

#### `set_blend_mode(mode)`
```cpp
enum class BlendMode { normal, additive, multiply, screen, premultiplied };
void set_blend_mode(BlendMode mode);
```
- **Description**: Selects the colour-blending mode for subsequent draws (e.g. additive glow for particles). `BlendMode::additive` is fully supported on both OpenGL and Vulkan, for points and triangle-based draws (including post-process effects like `sl::Blur`). `multiply`, `screen` and `premultiplied` (source already multiplied by alpha) are also available; on Vulkan and Direct3D 12 the extra modes apply to triangle-based draws (sprites, filled shapes, text), and other primitives fall back to normal blending. See `ex2dfeatures`.

#### `draw_points(bitmap, points, count)` / `set_blend_mode(mode)`
```cpp
struct PointVertex { float x = 0.0f, y = 0.0f; Colour colour; };
void draw_points(Bitmap *bitmap, const PointVertex *points, int count);

enum class BlendMode { normal, additive, multiply, screen, premultiplied };
void set_blend_mode(BlendMode mode);
```
- **Description**: `draw_points` submits many single-pixel points to the screen in one batched GPU call, for use cases like rendering large GPU-simulated particle systems (see `excompute`/`exgpuparticles`) without a per-point draw call. `set_blend_mode` selects additive (`GL_ONE, GL_ONE`) vs. normal alpha blending for subsequent draws, supported on both OpenGL and Vulkan.

#### Sprite Drawing Functions
```cpp
void draw_sprite(Bitmap *bitmap, float x, float y);
void draw_sprite_stretched(Bitmap *bitmap, float x, float y, int width, int height);
void draw_sprite_rotated(Bitmap *bitmap, float centerX, float centerY, float angleDegrees);
void draw_sprite_rotated_stretched(Bitmap *bitmap, float centerX, float centerY, float angleDegrees, int width, int height);
void draw_sprite_h_flip(Bitmap *bitmap, float x, float y);
void draw_sprite_v_flip(Bitmap *bitmap, float x, float y);
void draw_sprite_batch(Bitmap *bitmap, const std::vector<SpriteInstance> &sprites);
```
- **Description**: Hardware-accelerated sprite rendering with positioning, scaling, centre-point rotation (clockwise degrees), and horizontal/vertical flipping. Offscreen render targets are automatically orientation-corrected when drawn as sprites.
- **`draw_sprite_batch`**: draws many sprites from one bitmap in a single call. Each `SpriteInstance` has a centre `x`/`y`, `scale_x`/`scale_y` (negative mirrors), `rotation` in clockwise degrees, an optional source rectangle (zero width or height means the whole bitmap) and a `tint` that multiplies the texel colour and alpha. Vertices are built on the CPU and sent through the normal 2D batcher, so the cost is one draw call per batch rather than per sprite, and blend modes and render targets behave as for `draw_sprite`. It is not GPU instancing, unlike `draw_mesh_instanced()` in 3D. The tint is ignored during a normal-map pass.

### Sprite parameters, clipping, camera, collision and tweens

```cpp
struct SpriteParams { float x, y, origin_x = 0.5f, origin_y = 0.5f, scale_x = 1, scale_y = 1, rotation = 0;
                      bool flip_x, flip_y; int source_x, source_y, source_width, source_height; Colour tint; };
void draw_sprite_ex(Bitmap *bitmap, const SpriteParams &params);
void set_clip_rect(int x, int y, int width, int height);
void clear_clip_rect();
bool get_clip_rect(int *x = nullptr, int *y = nullptr, int *width = nullptr, int *height = nullptr);
```

- **`draw_sprite_ex`**: one sprite with an origin (a fraction of its size, so `(0.5, 0.5)` is the centre and `(0, 0)` the top-left) that rotation and scale pivot about, scale, clockwise rotation, flips, a source rectangle and a tint. It uses the sprite batcher.
- **`set_clip_rect`** / **`clear_clip_rect`**: restrict GPU drawing to a pixel rectangle on the screen or the active render target (a hardware scissor on OpenGL, Vulkan and Direct3D 12). `begin_render_target()` and `end_render_target()` clear it. Software drawing into RAM bitmaps is not clipped.
- **`Camera2D`** (`camera2d.h`): `x`/`y` (the world point at the centre of the viewport), `zoom`, clockwise `rotation`, an optional viewport, `set_bounds()`, frame-rate independent `follow()`, `shake()` plus `update()`, `world_to_screen()`, `screen_to_world()`, `visible_rect()` and `draw_sprite()` for world-space sprites. `top_left()` gives the position for `draw_tilemap()`, which supports panning and shake but not zoom or rotation. See `excamera2d`.
- **Collision helpers** (`collision2d.h`): `Rect2D`, `Circle2D`, `Vec2D`; overlap tests; `collide_rects`, `collide_circles`, `collide_circle_rect` and `collide_polygons` (convex, SAT) returning a push-out normal and depth; `raycast_rect` and `raycast_circle`; and `SpatialHash2D` for broad-phase queries.
- **Tweens and timers** (`tween.h`): `ease(Ease, t)` with 18 curves, `Tween` (delay, loop, ping-pong) and `Scheduler` (`after()`, `every()`, `cancel()`).

- **Pixel-perfect scaling** (`pixel_scale.h`): `compute_scale_fit(game_w, game_h, window_w, window_h, ScaleMode)` with `ScaleMode::{stretch, aspect_fit, integer}` returns a `ScaleFit` (destination rectangle, scale, `window_to_game()`, `game_to_window()`, `contains()`). Render to a `create_render_target()` bitmap at the low resolution, then `draw_scaled_target(target, fit)`. `present_scaled_target(target, mode)` does the fit, the draw and `set_mouse_scale_fit()` in one call each frame, so it follows window resizes and fullscreen toggles and `mouse_x()`/`mouse_y()` return game-image coordinates (`set_mouse_mapping()` / `clear_mouse_mapping()` in `input.h` are the underlying calls). It is a helper rather than a viewport change, so 3D and post-processing are unaffected.
- **Animation clips** (`animation2d.h`): `SpriteSheet` holds frames, named `AnimationClip`s (loop, `PlayDirection::{forward, reverse, ping_pong}`, per-frame durations) and frame events (`add_event`). Build one with `sprite_sheet_from_atlas()` or import Aseprite/TexturePacker JSON with `load_sprite_sheet_json()` / `parse_sprite_sheet_json()` (array or hash frames, `frameTags`). `Animator` plays a clip: `play()`, `update(dt)`, `pause()`/`resume()`, `finished()`, `on_event`, `on_finished`, `draw()`.
- **Text layout** (`font.h`): `TextStyle` (`align`, `wrap_width`, `line_spacing`, `shadow`, `outline`), `wrap_text()`, `text_block_width()`, `text_block_height()` and `draw_text_block()`. Wrapping is UTF-8 aware and breaks over-long words. All text drawing is UTF-8: glyphs are rasterised into the font's atlas the first time they are used, so any code point the font provides (accents, Greek, Cyrillic, symbols) works. Missing glyphs draw as `?`. With the text panel enabled, one panel is drawn behind the whole block.

#### Shape Primitives & Curves
```cpp
void line(Bitmap *bitmap, float x1, float y1, float x2, float y2, Colour colour, float thickness = 1.0f);
void arc(Bitmap *bitmap, float x, float y, float startAngle, float endAngle, float radius, Colour colour, float thickness = 1.0f);
void spline(Bitmap *bitmap, const int points[8], Colour colour, float thickness = 1.0f);
void spline(Bitmap *bitmap, const float points[8], Colour colour, float thickness = 1.0f);
int calc_spline(const int points[8], int npts, int *xout, int *yout);
int calc_spline(const float points[8], int npts, float *xout, float *yout);

void rect(Bitmap *bitmap, float left, float top, float right, float bottom, Colour colour, float thickness = 1.0f);
void rectfill(Bitmap *bitmap, float left, float top, float right, float bottom, Colour colour);
void circle(Bitmap *bitmap, float x, float y, float radius, Colour colour, float thickness = 1.0f);
void circlefill(Bitmap *bitmap, float x, float y, float radius, Colour colour);
void ellipse(Bitmap *bitmap, float x, float y, float radiusX, float radiusY, Colour colour, float thickness = 1.0f);
void ellipsefill(Bitmap *bitmap, float x, float y, float radiusX, float radiusY, Colour colour);
void triangle(Bitmap *bitmap, float x1, float y1, float x2, float y2, float x3, float y3, Colour colour, float thickness = 1.0f);
void trianglefill(Bitmap *bitmap, float x1, float y1, float x2, float y2, float x3, float y3, Colour colour);
```
- **Description**: Immediate-mode 2D shape, arc, and Bézier spline primitives with customisable line thickness. `calc_spline` computes interpolated $(x, y)$ coordinate arrays along a 4-point cubic Bézier curve.

#### Allegro-style Point-Plotter Callbacks (`do_*`)
```cpp
using PixelProc = std::function<void(Bitmap *bitmap, int x, int y, int d)>;

void do_line(Bitmap *bitmap, int x1, int y1, int x2, int y2, int d, const PixelProc &proc);
void do_circle(Bitmap *bitmap, int x, int y, int radius, int d, const PixelProc &proc);
void do_ellipse(Bitmap *bitmap, int x, int y, int radiusX, int radiusY, int d, const PixelProc &proc);
void do_arc(Bitmap *bitmap, int x, int y, float startAngle, float endAngle, float radius, int d, const PixelProc &proc);
void do_spline(Bitmap *bitmap, const int points[8], int d, const PixelProc &proc);
```
- **Description**: Executes rasterisation algorithms (Bresenham line, Midpoint circle/ellipse, Bézier curve) and invokes `proc(bitmap, x, y, d)` for every point along the shape. Enables custom pixel manipulation, collision tracing, and procedural geometry creation.

---

## Typography & Text Rendering ([include/font.h](../include/font.h))

The font subsystem provides TrueType/OpenType font loading, measurement, printf formatting, and high-performance persistent font atlases.

### Functions

#### `open_font(path, pointSize)` / `close_font(font)`
```cpp
Font *open_font(const std::string &path, int pointSize);
void close_font(Font *font);
```
- **Description**: Loads a TrueType/OpenType font file at the specified point size, or closes a previously opened font.
- **Parameters**:
  - `path` (`const std::string&`): File path to TTF/OTF font file.
  - `pointSize` (`int`): Font size in points.
- **Returns**: Opaque `Font*` handle (SDL_ttf font wrapper), or `nullptr` on error.

#### `get_default_monospace_font()` / `open_monospace_font(pointSize)` / `open_sans_font(pointSize)`
```cpp
Font *get_default_monospace_font();
Font *open_monospace_font(int pointSize);
Font *open_sans_font(int pointSize);
```
- **Description**: Returns built-in fallback platform monospace or sans-serif fonts without requiring external asset files.

#### `text_length(font, text)` / `text_height(font)`
```cpp
int text_length(Font *font, const std::string &text);
int text_height(Font *font);
```
- **Description**: Measures the bounding box dimensions (width or line height) of UTF-8 text in pixels.
- **Returns**: Dimension in pixels.

#### `textout(font, x, y, colour, text)`
```cpp
void textout(Font *font, int x, int y, const Colour &colour, const std::string &text);
```
- **Description**: Renders UTF-8 text at screen position `(x, y)` using the font's persistent GPU glyph atlas.
- **Parameters**:
  - `font` (`Font*`): Loaded font handle.
  - `x`, `y` (`int`): Top-left text position in pixels.
  - `colour` (`Colour`): Text colour.
  - `text` (`const std::string&`): UTF-8 text string to render.

#### `gprintf(x, y, colour, format, ...)` / `gprintf_center(y, colour, format, ...)`
```cpp
void gprintf(int x, int y, const Colour &colour, const char *fmt, ...);
void gprintf_center(int y, const Colour &colour, const char *fmt, ...);
```
- **Description**: Convenience functions that format and render text using the default monospace font, positioned at `(x, y)` or horizontally centred on screen.

#### `set_text_panel_style(style)` / `text_panel_style()`
```cpp
struct TextPanelStyle { bool enabled; Colour background; Colour text; int padding_x, padding_y; };
void set_text_panel_style(const TextPanelStyle &style);
```
- **Description**: Optional backing panel so text stays readable over busy scenes. While `enabled`, `gprintf`, `gprintf_center`, `textout`, `textprintf`, `textout_cached` and `draw_renderer_label` draw each string on a translucent dark rectangle in the style's `text` colour (pastel yellow by default), ignoring the colour passed in. Off by default; the examples turn it on with `sl::set_text_panel_style({true});`.

---

## Shaders, Compute & Storage Buffers ([include/graphics_fx.h](../include/graphics_fx.h))

This subsystem encapsulates dynamic GLSL compilation, custom vertex/fragment/compute programs, shader uniform setting, and GPU Storage Buffers (SSBOs) across OpenGL and Vulkan.

### `sl::Shader`

#### Member Functions

```cpp
bool load(const std::string &vertexSource, const std::string &fragmentSource);
bool load_compute(const std::string &computeSource);
bool load_files(const std::string &vertexPath, const std::string &fragmentPath, ...);
bool load_compute_file(const std::string &computePath, const std::string &assetId = {});
void reset();
bool is_valid() const;
const std::string &error() const;

bool use() const;
static void stop();

bool dispatch_compute(unsigned int groupsX, unsigned int groupsY, unsigned int groupsZ) const;

bool set_texture(unsigned int unit, Bitmap *bitmap) const;
bool set_texture(const char *samplerName, unsigned int unit, Bitmap *bitmap) const;
bool set_storage_texture(unsigned int unit, Bitmap *bitmap) const;

bool set_uniform(const char *name, int value) const;
bool set_uniform(const char *name, float value) const;
bool set_uniform(const char *name, float x, float y) const;
bool set_uniform(const char *name, float x, float y, float z) const;
bool set_uniform_mat4(const char *name, const float *matrix4x4) const;
```

### `sl::StorageBuffer`

Represents an RAII GPU Shader Storage Buffer Object (SSBO) for compute shader storage and CPU/GPU data transfer.

```cpp
class StorageBuffer {
public:
    StorageBuffer() = default;
    explicit StorageBuffer(std::size_t sizeBytes);
    ~StorageBuffer();

    bool create(std::size_t sizeBytes);
    void destroy();
    bool is_valid() const;
    std::size_t size_bytes() const;
    std::uint32_t handle() const;

    bool upload(const void *data, std::size_t sizeBytes, std::size_t offsetBytes = 0);
    template <typename T> bool upload(const std::vector<T> &data, std::size_t offsetBytes = 0);

    bool readback(void *outData, std::size_t sizeBytes, std::size_t offsetBytes = 0) const;
    template <typename T> bool readback(std::vector<T> &outData, std::size_t offsetBytes = 0) const;

    bool bind(unsigned int bindingIndex) const;
};
```

### Compute Helpers

#### `compute_barrier()`
```cpp
void compute_barrier();
```
- **Description**: Issues a GPU memory barrier ensuring all compute shader writes are visible to subsequent render or compute passes.

#### `dispatch_compute_for(shader, totalItemsX, totalItemsY, totalItemsZ, localSizeX, localSizeY, localSizeZ)`
```cpp
bool dispatch_compute_for(const Shader &shader,
                         unsigned int totalItemsX, unsigned int totalItemsY = 1, unsigned int totalItemsZ = 1,
                         unsigned int localSizeX = 16, unsigned int localSizeY = 16, unsigned int localSizeZ = 1);
```
- **Description**: Convenience helper that calculates workgroup grid dimensions $\lceil \text{total} / \text{localSize} \rceil$ and dispatches the compute shader.

---

## Post-Processing Graphics Effects ([include/graphics_fx.h](../include/graphics_fx.h))

Post-processing effects process regions or full-screen bitmaps using pre-compiled GPU pipeline passes.

### Classes & Effects

- **`sl::PingPongBuffer`**: Manages double-buffered offscreen render targets for multi-pass post-processing chains (`begin`, `advance`, `source`, `target`).
- **`sl::Bloom`**: Thresholds bright pixels, downsamples, applies separable Gaussian blur, and composites glow (`set_threshold`, `set_intensity`, `set_radius`, `apply`).
- **`sl::Vignette`**: Darkens image edges in a soft-radius circle (`set_radius`, `set_softness`, `set_intensity`, `apply`).
- **`sl::ColourAdjust`**: Adjusts brightness, contrast, saturation, and exposure (`set_brightness`, `set_contrast`, `set_saturation`, `set_exposure`, `apply`).
- **`sl::ScreenShake`**: Applies decaying camera offsets to 2D projections (`trigger(amplitude, duration)`, `update(delta_seconds)`, `active()`).
- Additional effects available: `sl::Blur`, `sl::ChromaticAberration`, `sl::Pixelate`, `sl::RadialBlur`, `sl::HeatHaze`, `sl::Shockwave`, `sl::CRTFilter`, `sl::DitherFilter`, `sl::FilmGrain`.

---

## 2D Radial Lighting & Polygon Shadows ([include/graphics_fx.h](../include/graphics_fx.h))

Provides 2D soft-shadow light culling, spotlights, and polygonal shadow casting over rendered scenes.

### Key Types

#### `Light`
```cpp
struct Light {
    float x = 0.0f;
    float y = 0.0f;
    float radius = 256.0f;
    float intensity = 1.0f;
    float shadow_softness = 0.0f;
    float direction_x = 0.0f;
    float direction_y = -1.0f;
    float inner_angle = 0.0f; // >0 enables spotlight cone
    float outer_angle = 0.0f;
    Colour colour{255, 255, 255};
};
```

#### `ShadowCaster` & `make_rectangle_shadow_caster`
```cpp
struct ShadowCaster { std::vector<ShadowPoint> vertices; };
ShadowCaster make_rectangle_shadow_caster(float left, float top, float right, float bottom);
```

### `sl::LightingPass`
```cpp
class LightingPass {
public:
    bool initialise();
    void shutdown();
    void set_ambient(float ambient); // 0.0 (black) to 1.0 (full ambient)

    void apply(Bitmap *source, const Light &light, int x = 0, int y = 0, int width = 0, int height = 0,
               const std::vector<ShadowCaster> &casters = {}) const;
    void apply(Bitmap *source, const std::vector<Light> &lights, int x = 0, int y = 0, int width = 0, int height = 0,
               const std::vector<ShadowCaster> &casters = {}) const;
};
```

---

## Box2D Physics System ([include/physics.h](../include/physics.h))

`simlib` provides an integrated Box2D rigid-body simulation wrapper operating directly in pixel coordinates (64 pixels = 1 metre).

### Functions

#### `create_physics_world(gravity)` / `destroy_physics_world(world)`
```cpp
PhysicsWorld *create_physics_world(Vec2 gravity = {0.0f, 980.0f});
void destroy_physics_world(PhysicsWorld *world);
```
- **Description**: Creates a 2D physics world with vertical gravity in pixels/sec² (default 980 px/s² $\approx$ 9.8 m/s²), or destroys a world and all owned bodies.

#### `step_physics_world(world, time_step, velocity_iterations = 8, position_iterations = 3)`
```cpp
void step_physics_world(PhysicsWorld *world, float time_step, int velocity_iterations = 8, int position_iterations = 3);
```
- **Description**: Advances simulation state by `time_step` seconds.

#### `create_physics_body(world, type, position)` / `destroy_physics_body(body)`
```cpp
PhysicsBody *create_physics_body(PhysicsWorld *world, BodyType type, Vec2 position = {});
void destroy_physics_body(PhysicsBody *body);
```
- **Description**: Constructs a static, kinematic, or dynamic rigid body at a pixel-space position.

#### Body Manipulators
```cpp
void set_physics_body_transform(PhysicsBody *body, Vec2 position, float angle = 0.0f);
Vec2 physics_body_position(const PhysicsBody *body);
float physics_body_angle(const PhysicsBody *body);
Vec2 physics_body_velocity(const PhysicsBody *body);
void set_physics_body_velocity(PhysicsBody *body, Vec2 velocity);
void apply_physics_force(PhysicsBody *body, Vec2 force);
```

#### Fixtures & Colliders
```cpp
bool add_box_fixture(PhysicsBody *body, float width, float height, float density = 1.0f, float friction = 0.3f, float restitution = 0.0f);
bool add_circle_fixture(PhysicsBody *body, float radius, float density = 1.0f, float friction = 0.3f, float restitution = 0.0f);
bool add_polygon_fixture(PhysicsBody *body, const std::vector<Vec2> &vertices, float density = 1.0f, float friction = 0.3f, float restitution = 0.0f);
```

#### `poll_physics_contacts(world)`
```cpp
std::vector<PhysicsContact> poll_physics_contacts(PhysicsWorld *world);
```
- **Description**: Returns and clears all collision contact events (`begin` / `end` overlap, bodies involved, contact point, and normal) generated since the last step.

---

#### Joints and debug drawing
A *revolute* joint is a hinge: two bodies pinned at one point that can rotate about it (bridge planks, doors, wheels on an axle).
```cpp
PhysicsJoint *create_distance_joint(PhysicsWorld *world, PhysicsBody *a, PhysicsBody *b, Vec2 anchor_a, Vec2 anchor_b, float frequency_hz = 0.0f, float damping_ratio = 0.0f);
PhysicsJoint *create_revolute_joint(PhysicsWorld *world, PhysicsBody *a, PhysicsBody *b, Vec2 anchor);
PhysicsJoint *create_prismatic_joint(PhysicsWorld *world, PhysicsBody *a, PhysicsBody *b, Vec2 anchor, Vec2 axis);
PhysicsJoint *create_weld_joint(PhysicsWorld *world, PhysicsBody *a, PhysicsBody *b, Vec2 anchor, float frequency_hz = 0.0f, float damping_ratio = 0.0f);
PhysicsJoint *create_wheel_joint(PhysicsWorld *world, PhysicsBody *a, PhysicsBody *b, Vec2 anchor, Vec2 axis, float frequency_hz = 4.0f, float damping_ratio = 0.7f);
PhysicsJoint *create_mouse_joint(PhysicsWorld *world, PhysicsBody *body, Vec2 target, float max_force = 10000.0f);
void destroy_physics_joint(PhysicsJoint *joint);
bool physics_joint_active(const PhysicsJoint *joint);
void set_mouse_joint_target(PhysicsJoint *joint, Vec2 target);
void set_joint_motor(PhysicsJoint *joint, float speed, float max_torque);       // revolute, wheel
void set_prismatic_motor(PhysicsJoint *joint, float speed, float max_force);
void set_revolute_limits(PhysicsJoint *joint, float lower, float upper);
void set_prismatic_limits(PhysicsJoint *joint, float lower, float upper);
void debug_draw_physics(PhysicsWorld *world, const PhysicsDebugDraw &options = {});
```
- **Description**: Anchors and axes are pixel-space world coordinates in the bodies' starting pose. A joint handle stays valid until `destroy_physics_joint()`; if a connected body is destroyed first the joint goes inactive (`physics_joint_active()` is false) and is ignored. A non-zero `frequency_hz` turns distance and weld joints into damped springs. The mouse joint drags a dynamic body toward a target for grab-and-throw interaction. `debug_draw_physics()` draws shapes (filled and outlined), joints, optional bounding boxes and centres of mass to the screen with the 2D primitives; `PhysicsDebugDraw::offset` and `scale` follow a camera. See `exphysics`.

#### Ray casts, queries, sensors and filters
```cpp
PhysicsRayHit physics_raycast(PhysicsWorld *world, Vec2 from, Vec2 to, bool include_sensors = false, const PhysicsBody *ignore = nullptr);
std::vector<PhysicsRayHit> physics_raycast_all(PhysicsWorld *world, Vec2 from, Vec2 to, bool include_sensors = false, const PhysicsBody *ignore = nullptr);
std::vector<PhysicsBody *> physics_query_box(PhysicsWorld *world, Vec2 corner_a, Vec2 corner_b, bool include_sensors = false);
std::vector<PhysicsBody *> physics_query_point(PhysicsWorld *world, Vec2 point, bool include_sensors = false);
void set_physics_body_sensor(PhysicsBody *body, bool sensor);
void set_physics_body_filter(PhysicsBody *body, std::uint16_t category_bits, std::uint16_t mask_bits, std::int16_t group_index = 0);
```
- **Description**: `PhysicsRayHit` reports `hit`, the body, point and normal; `physics_raycast_all()` returns hits nearest first. Sensors detect overlaps without colliding; their contacts have `PhysicsContact::sensor` set and a zero point and normal. Filters use Box2D category/mask/group bits and also apply to fixtures added later. Ray casts and queries skip sensors unless `include_sensors` is true.

---

## 3D Surfaces ([include/surface3d.h](../include/surface3d.h))

A `Surface3D` is an offscreen colour+depth target you draw 3D meshes into, then composite into 2D with `draw_sprite(surface_3d_bitmap(surface), x, y)`. It works on the OpenGL, Vulkan and Direct3D 12 backends.

- `create_surface_3d(w, h, samples = 1)` / `destroy_surface_3d(surface)`: `samples` requests MSAA (e.g. 4), clamped to GPU support; `surface_3d_samples(surface)` reports the count in use. Both 3D meshes and 2D elements (HUD, text, sprites) can be drawn into multisampled surfaces on OpenGL, Vulkan and Direct3D 12.
- `begin_surface_3d(surface, camera)` / `end_surface_3d()`: bracket 3D drawing. Contents are undefined until `clear_surface_3d(colour)` is called.
- `set_light_3d({direction, ambient})`: one directional light.
- `create_cube_mesh`, `create_plane_mesh`, `create_sphere_mesh`, `create_mesh(vertices, indices)` / `destroy_mesh`
- `load_obj_mesh(path, colour)` / `load_obj_mesh_from_memory(data, size, colour)`: Wavefront OBJ (v/vt/vn/f, polygons triangulated, negative indices); normals are generated when missing.
- `load_model(path)` / `draw_model(model, transform)` / `destroy_model(model)`: loads `.obj` (with `.mtl`: `Kd` colour, `map_Kd` texture, `norm`/`map_Bump`/`bump` normal map, one part per `usemtl`), `.gltf` and `.glb` (node hierarchy, base-colour factor/texture, `normalTexture`, PBR metallic-roughness and emissive, `doubleSided`, skins, POSITION/NORMAL/TEXCOORD_0/COLOR_0, triangle primitives), and `.ac` (AC3D hierarchy transforms, object names, UVs, diffuse materials and textures). `load_flightgear_model(path)` imports a FlightGear model XML's AC3D model path, supported rotate/translate/spin/blend animations, and conditional visibility; `load_model_animations_json(model, path)` loads simlib's JSON animation format and `set_model_property(model, name, value)` drives either format. See [AC3D Models and FlightGear Animations](ac3d.md) for format coverage and limitations. A `Model` owns its meshes and textures; each `ModelPart` has a `Material` and transform.
- `draw_mesh(mesh, model_matrix, {tint, lit, texture, cull})`: `texture` is an optional `Bitmap*` sampled with the vertex `uv` (v = 0 is the image top) and multiplied into the colour. `cull` is a `CullMode`: `back` (default; clockwise-wound faces are culled), `front`, or `none`.
- `draw_mesh_instanced(mesh, instances, material)`: draw many copies of a mesh in one hardware-instanced call. `instances` is a `std::vector<InstanceData3D>`, each a `model` matrix plus a `tint` that multiplies the vertex colours (rgb and alpha) on top of `Material::tint`. The view-projection is applied once and each model matrix in the vertex shader, with correct lighting for non-uniform scale and mirrored instances. This overload uploads the instances every call, so it suits instances that change each frame; calls in the same frame do not overwrite one another.
- `create_instance_buffer(capacity)`, `update_instance_buffer(buffer, instances)`, `destroy_instance_buffer(buffer)` and `draw_mesh_instanced(mesh, buffer, material)`: keep the instance data on the GPU for groups that rarely change. The capacity is fixed at creation and `update_instance_buffer()` fails if given more instances. Update a buffer at most once per frame, before drawing it that frame. The buffer stays valid as the camera moves, so a static group needs no per-frame upload. `Material::terrain` shading is not supported with instancing. Supported on OpenGL, Vulkan and Direct3D 12 (the Direct3D 12 path is untested on Linux).

glTF 2.0 (`.glb`/`.gltf`) is the recommended format; export from Blender 4.x with File > Export > glTF 2.0 (Y-up). Node keyframe clips (translation/rotation/scale; linear, step, cubic-spline) are loaded into `Model::clips`:

```cpp
sl::play_model_clip(model, "Swing", /*loop=*/true, /*speed=*/1.0f);
sl::update_model_clip(model, dt);          // each frame, before draw_model
sl::sample_model_clip(model, "Slide", t);  // or pose at an explicit time
sl::reset_model_pose(model);
```

Save a model with `sl::set_mesh_cpu_data_retained(true)` before loading and then `sl::save_model_gltf(model, "out.glb")`; see [Blender and glTF workflow](blender.md) for the `simlib_convert` tool and Blender export script.

Also `find_model_clip`, `model_clip_duration`, `stop_model_clip`, `model_clip_playing`. Blender key times start at 1/24 s. Convert old AC3D assets to glTF once.

**Skinned models.** glTF skins (`JOINTS_0`/`WEIGHTS_0`, one set of four influences per vertex) are skinned on the CPU: each time the pose changes (`update_model_clip`, `sample_model_clip`, `reset_model_pose`) the model re-skins every skinned part and uploads the vertices with `sl::update_mesh_vertices(mesh, vertices)`, which you can also use for your own CPU-animated meshes (same vertex count, at most once per frame). Skinned parts have an identity `transform`, their mesh is not shared, and their `bounds_*` come from the load-time pose. Data is in `Model::skins` and `Model::skin_bindings` (`ModelSkinBinding::skinned` is the latest result). Skinned models keep a CPU copy of their bind-pose vertices. GPU skinning and morph targets are not implemented.

**PBR materials.** glTF materials load as `Material::pbr` with `metallic`, `roughness`, `metallic_roughness` (a texture with occlusion in R, roughness in G and metalness in B), `occlusion_strength` and `emissive` (`KHR_materials_emissive_strength` is honoured). Lit PBR materials use Cook-Torrance GGX shading with the scene's directional light, and `Light3D::ambient` as a simple ambient/occlusion term; there is no image-based lighting. Limits: an occlusion texture is used only when it is the same image as the metallic-roughness texture, and an emissive texture is ignored (as is the emissive factor for that material). Supported on OpenGL, Vulkan and Direct3D 12 (the Direct3D 12 path is untested on Linux). Set `Material::pbr = true` yourself to use it on custom meshes. Non-glTF materials are unchanged.

See `src/examples/ex3d_surface`, `src/examples/exmodel3d` and `src/examples/expbr`. glTF cameras/lights and morph targets are not supported.

---

## Voxel Chunks & World ([include/voxel.h](../include/voxel.h))

`simlib` provides basic voxel support designed for building Minecraft-style block worlds. Voxels are managed in 16x16x16 chunks using hidden-face culling to batch visible cube faces into indexed GPU `Mesh` buffers, directional face shading, texture atlas UV mapping, and 3D DDA raycasting for block picking and placement.

### Key Types & Constants

- `CHUNK_SIZE = 16`: Dimensions along X, Y, and Z axes (4,096 voxels per chunk).
- `BlockId = std::uint8_t`: 8-bit block identifier (`BLOCK_AIR = 0`).
- `VoxelFace`: Cube faces `top` (+Y), `bottom` (-Y), `north` (+Z), `south` (-Z), `east` (+X), `west` (-X).
- `BlockDefinition`: Defines block properties (`is_solid`, `is_transparent`, per-face `tiles[6]`, `tint`), emitted RGB block light (`light_emission` in `[0..15]`, `light_colour`), and light attenuation (`light_opacity` in `[0..15]`; 255 infers a default from solidity/transparency).
- `VoxelLight`: Per-voxel sky level and propagated RGB block-light levels, each in `[0..15]`.
- `VoxelAtlas`: Grid texture atlas helper converting tile coordinates `(x, y)` to normalised UV coordinates `[u0, v0, u1, v1]`.
- `VoxelRaycastResult`: Stores raycast intersection results (`hit`, `hit_block`, `place_block`, `hit_face`, `hit_pos`, `distance`).

### `sl::VoxelChunk`
- `VoxelChunk(cx, cy, cz)`: Creates a chunk at chunk grid coordinates.
- `get_block(x, y, z)` / `set_block(x, y, z, id)`: Local block access in `[0..15]`.
- `fill(id)`: Fills entire chunk with a block ID.
- `count_visible_faces(lookup)`: Counts visible faces after hidden-face culling without allocating GPU memory.
- `rebuild_mesh(atlas, lookup)`: Generates an indexed GPU `Mesh*` with culled quads, propagated vertex lighting, directional shading, and UV mapping.
- `mesh()` / `vertex_count()` / `triangle_count()`: Inspect generated mesh.
- `draw(material)`: Renders the chunk mesh at its world origin into the active 3D surface.

### `sl::VoxelWorld`
- Multi-chunk manager coordinating chunk storage, cross-boundary culling, and dirty chunk updating.
- `world_to_chunk(x, y, z)` / `world_to_local(x, y, z)`: Coordinate conversion helpers handling negative coordinates correctly.
- `get_block(x, y, z)` / `set_block(x, y, z, id)`: Query and set blocks at arbitrary world coordinates. Automatically marks affected chunks (and border neighbours) dirty and schedules lighting updates.
- `get_light(x, y, z)`: Query propagated sky and RGB block-light levels.
- `update_lighting()`: Recompute direct sky light and neighbour-propagated sky/block light after block or emitter changes; call this to refresh light levels before querying them.
- `update_dirty_chunks(atlas)`: Recomputes pending lighting and rebuilds meshes for chunks affected by block or lighting changes.
- `raycast(origin, direction, max_distance)`: Fast Voxel Traversal (3D DDA) through world blocks.
- `draw(material)`: Draws all chunk meshes.

By default, air and transparent blocks pass propagated light with one level of attenuation per neighbouring voxel, while opaque blocks stop it. `light_opacity` can customise that behaviour (0 passes without attenuation; 15 blocks light). Emission colour propagates per RGB channel. Direct sky light remains at full strength through open air above the highest opaque voxel in each world column, then spreads into caves with neighbour attenuation. Block-light propagation and cave light spread are limited to allocated chunks; unallocated cells can still report direct sky light when unobstructed.

```cpp
sl::BlockDefinition torch;
torch.light_emission = 15;
torch.light_colour = {255, 170, 90, 255};
sl::register_block_definition(TORCH_BLOCK_ID, torch);

world.set_block(x, y, z, TORCH_BLOCK_ID);
world.update_lighting();
world.update_dirty_chunks();
sl::VoxelLight light = world.get_light(x + 1, y, z);
```

See `src/examples/exvoxel` for an interactive demo with procedural terrain generation, pixel-art texture atlas, first-person camera, and block break/place mechanics.

---

## Audio Subsystem (Samples & Streams) ([include/audio.h](../include/audio.h))

Handles SDL_mixer sample sound effects (0-255 Allegro volume/pan conventions) and music streams.

### Sound Effects (Samples)
```cpp
bool audio_fx_init();
void audio_fx_shutdown();

Sample *load_sample(const std::string &path);
void destroy_sample(Sample *sample);

std::uint64_t play_sample(Sample *sample, int volume = 255, int pan = 128, int frequency = 1000, int loops = 0);
void stop_voice(std::uint64_t voice);
void stop_sample(Sample *sample);
void stop_all_samples();
void audio_fx_set_volume(int volume); // 0-255
```

### Music Streams
```cpp
bool music_init();
void music_shutdown();

Stream *load_stream(const std::string &path);
void destroy_stream(Stream *stream);

void play_stream(Stream *stream, int loops = -1);
void stop_stream();
void pause_stream();
void resume_stream();
void music_set_volume(int volume); // 0-255
```

---

## Cellular Automata Fluid & Granular Simulation ([include/fluid.h](../include/fluid.h))

Multi-material Cellular Automata simulation combining fluids, falling sand, gases, fire, thermodynamics, and Box2D Archimedes buoyancy coupling.

```cpp
FluidSimulation *create_fluid_simulation(int width = 200, int height = 150);
void destroy_fluid_simulation(FluidSimulation *sim);
void fluid_step(FluidSimulation *sim);
bool fluid_set_cell(FluidSimulation *sim, int x, int y, FluidElement type, float mass = 0.0f, float temp = 20.0f,
                    uint8_t life = 0, int8_t variation = 0);
FluidCell fluid_get_cell(const FluidSimulation *sim, int x, int y);
void fluid_clear(FluidSimulation *sim);
void fluid_reset_bounds(FluidSimulation *sim);
```

---

## Particle Systems & Emitters ([include/particles.h](../include/particles.h))

Configurable 2D particle emitters with velocity spread, size scaling, colour fading, and gravity.

```cpp
struct EmitterConfig {
    float emit_rate = 30.0f;
    float particle_lifetime = 1.0f;
    float min_speed = 20.0f, max_speed = 80.0f;
    float direction_degrees = -90.0f, spread_degrees = 30.0f;
    float start_size = 8.0f, end_size = 0.0f;
    Colour start_colour{255, 255, 255, 255}, end_colour{255, 255, 255, 0};
    float gravity = 0.0f;
    int max_particles = 500;
    Bitmap *texture = nullptr;
};

class ParticleEmitter {
public:
    ParticleEmitter(const EmitterConfig &config, float x = 0.0f, float y = 0.0f);
    void set_position(float x, float y);
    void update(float delta_seconds);
    void draw(Bitmap *target);
    void emit_burst(int count);
};
```

---

## Precipitation ([include/precipitation.h](../include/precipitation.h))

Rain, hail and snow in 2D (`Precipitation2D`, drawn on `sl::screen`) and 3D (`Precipitation3D`, drawn around a `Camera3D`). Both share a `PrecipitationController` that fades smoothly between weather states.

```cpp
enum class PrecipitationType { none, rain, hail, snow };
enum class PrecipitationLevel { light, moderate, heavy, storm };

struct PrecipitationConfig { PrecipitationType type; float intensity; glm::vec2 wind; float fall_speed, size; Colour colour; bool splashes; };
PrecipitationConfig precipitation_preset(PrecipitationType type, PrecipitationLevel level);

// weather.transition_to(cfg, seconds) fades the old type out, then the new one in
Precipitation2D p2(width, height);
p2.set_ground([](float x) { return ground_y(x); });
p2.update(dt);  p2.draw();

Precipitation3D p3;
p3.set_ground(height_fn);
p3.update(dt, camera);  p3.draw(camera);  // call p3.release() before sl::shutdown()
float cover = p3.snow_cover();             // 0-1; use snow_tinted(colour, cover) to whiten surfaces
precipitation_apply_environment(env, p3.weather.current());  // darker sky, denser fog
```

`Precipitation3D` releases its GPU resources in its destructor, so a manual `release()` is optional (and a no-op after `sl::shutdown()`).

Also: `Precipitation3D::wind_from_clouds` and `PrecipitationLens` (haze overlay; set `droplets = true` for raindrops and melting flakes on the lens).

**Weather sound.** `PrecipitationAudio` plays a looped bed that is synthesised at run time with the audio synth, so no sound files are needed: a bright hiss for rain, a gated clatter for hail and a faint low wind for snow. Call `sl::audio_fx_init()` first, then `update()` each frame and `release()` before audio shutdown (the destructor also does this).

```cpp
sl::audio_fx_init();
sl::PrecipitationAudio sound;
sound.update(p3.weather.current());       // default master volume 26, about 0.1 of full scale
sound.update(p3.weather.current(), 60);   // master 0-255; louder
sound.update({sl::PrecipitationType::none, 0.0f});  // silence
```

Loudness follows the weather's intensity (snow is quieter than rain) scaled by `master`. The audio layer has no per-voice volume, so the loop restarts when the volume (in steps of 4) or the weather type changes. `PrecipitationAudio::volume_for(weather, master)` returns the chosen 0-255 volume. 3D particles are drawn with `draw_mesh_instanced` as alpha-blended quads (`MaterialBlend::alpha`), after your opaque geometry.

---

## Procedural FastNoise Generation ([include/noise.h](../include/noise.h))

A wrapper around FastNoiseLite for procedural terrain and texture generation.

```cpp
class Generator {
public:
    explicit Generator(int seed = 1337);
    void set_frequency(float frequency);
    void set_type(FastNoiseLite::NoiseType type);
    float get(float x, float y) const;
    float get(float x, float y, float z) const;
};
```

---

## Embedded SQLite Persistence ([src/examples/exdb/rdb.h](../src/examples/exdb/rdb.h))

Provides RAII-managed C++ wrappers around SQLite for database creation, prepared statements, and transactional safety. Located in the `exdb` example module.

```cpp
namespace rdb {
    class Database {
    public:
        Database(const std::string &filename);
        void execute(const std::string &sql);
        std::unique_ptr<Statement> prepare(const std::string &sql);

        class Transaction {
        public:
            explicit Transaction(Database &db);
            void commit();
            void rollback();
        };
    };

    class Statement {
    public:
        void bind(int index, int val);
        void bind(int index, double val);
        void bind(int index, const std::string &val);
        bool step(); // Returns true while rows are available
        void reset();
        int getInt(int col);
        double getDouble(int col);
        std::string getText(int col);
    };
}
```

---

## Resource Archives ([include/resource.h](../include/resource.h))

Provides read-only ZIP asset archive support for loading images, fonts, audio samples, and scripts directly from compressed data packages.

```cpp
class Archive {
public:
    Archive() = default;
    bool open(const std::string &path);
    void close();
    bool contains(const std::string &name) const;
    std::vector<std::string> entries() const;
    std::vector<std::uint8_t> read(const std::string &name) const;
};
```

---

## Isometric Math & Projection ([include/isometric.h](../include/isometric.h))

Provides pure mathematical transformations between 3D isometric world grid coordinates $(x, y, z)$ and 2D screen coordinates $(sx, sy)$, diamond vertex calculations, mouse tile picking, viewport culling, and depth sorting.

```cpp
namespace sl {
    struct IsometricConfig {
        float tile_width = 64.0f;
        float tile_height = 32.0f;
        float elevation_height = 16.0f;
    };

    struct IsometricTransform {
        IsometricConfig config;
        float origin_x = 0.0f;
        float origin_y = 0.0f;
        float zoom = 1.0f;

        // Forward transform: 3D grid -> 2D screen
        IsoPoint world_to_screen(float x, float y, float z = 0.0f) const;
        void world_to_screen(float x, float y, float z, float &sx, float &sy) const;

        // Inverse transform: 2D screen -> 3D grid
        IsoPoint screen_to_world(float sx, float sy, float z = 0.0f) const;
        void screen_to_world(float sx, float sy, float z, float &x, float &y) const;
        IsoGridPoint screen_to_grid(float sx, float sy, float z = 0.0f) const;

        // Geometric helpers
        IsoTileDiamond tile_diamond(float x, float y, float z = 0.0f) const;
        IsoBounds visible_bounds(float left, float top, float right, float bottom,
                                 float min_z = 0.0f, float max_z = 0.0f) const;

        // Depth sorting key
        static float depth_sort_key(float x, float y, float z = 0.0f);
    };
}
```

---

## JSON Configuration ([include/config.h](../include/config.h))

Provides a JSON-backed application configuration document with nested dotted-key path access, typed serialisation, and fallback values.

```cpp
namespace sl {
    class Config {
    public:
        Config() = default;

        // File I/O
        bool load(const std::string &path);
        bool save(const std::string &path, int indentation = 4) const;

        // Key queries
        bool contains(const std::string &key) const;

        // Typed getters and setters (supporting dotted paths like "video.resolution.width")
        template <typename Type>
        Type get(const std::string &key, const Type &fallback) const;

        template <typename Type>
        void set(const std::string &key, const Type &value);

        // Document access
        const nlohmann::json &document() const;
    };
}
```

---

[← Back to README](../README.md)

---

## Tile Maps ([include/tilemap.h](../include/tilemap.h))

Multi-layer orthogonal or isometric tile maps built on `sl::Atlas`, with culled batched drawing, animated tiles, tile metadata and a Tiled JSON loader.

### Key Types
- `sl::TileMap`: map size, tile size, `tilesets`, `layers` (`TileLayer`), `object_layers` (`ObjectLayer`) and an animation clock. Create with `create_tilemap()` or `load_tiled_map()` and free with `destroy_tilemap()` (which also frees bitmaps the loader created).
- `sl::TileLayer`: row-major `cells`, plus `visible`, `offset_x/y` and `parallax_x/y`. A cell is a GID (0 = empty) OR-ed with `tile_flip_horizontal`, `tile_flip_vertical` and/or `tile_flip_diagonal`; mask with `tile_id_mask`.
- `sl::TileSet` / `sl::TileInfo`: an atlas with its `first_gid`, and per-tile `solid`, `properties` and `animation` frames.

### Functions
- `tilemap_add_tileset(map, atlas, first_gid = 0)`, `tilemap_add_layer(map, name)`, `tilemap_layer()`, `tilemap_get()`, `tilemap_set()`.
- `tilemap_set_solid()`, `tilemap_tile_info()`, `tilemap_tile_property()`, `tilemap_is_solid()`, `tilemap_rect_blocked()`: collision queries (out of bounds counts as solid on finite maps, and as empty on infinite maps).
- `tilemap_world_to_tile()`, `tilemap_tile_to_world()`: coordinate conversion.
- `create_hex_tilemap(width, height, tile_w, tile_h, side_length, stagger_y, stagger_odd)`, `create_infinite_tilemap(tile_w, tile_h)`, `tilemap_layer_bounds()`: hexagonal maps and sparse, chunked infinite maps (negative coordinates allowed, grown on `tilemap_set`). `load_tiled_map()` reads Tiled hexagonal maps and infinite maps (`chunks`); infinite isometric maps are rejected. Hex `tilemap_world_to_tile()` picks the nearest hex centre.
- `update_tilemap(map, dt_seconds)`: advances tile animations.
- `draw_tilemap(map, camera_x, camera_y, view_width = 0, view_height = 0, first_layer = 0, last_layer = -1)`: draws visible cells only. For isometric maps the camera is the screen offset of cell (0, 0).
- `load_tiled_map(path)`: loads Tiled JSON (`.tmj`/`.json`) or native XML (`.tmx`) maps with embedded or external `.tsj`/`.tsx` tilesets (including image-collection tilesets, kept per tile in `TileSet::images`), CSV or base64 layer data (uncompressed, zlib or gzip, also inside infinite-map chunks), group layers, flips, tile properties (a boolean `solid` property sets `TileInfo::solid`), animations and object layers. Zstd-compressed layers and infinite isometric maps are rejected with an error.
- `tilemap_find_object(map, name)`: look up a spawn point or trigger from an object layer.
- `draw_sprite_region_flipped()` (in `draw.h`) draws a bitmap region with horizontal, vertical and diagonal flips.

See [`extilemap`](../src/examples/extilemap/extilemap.cpp) for a scrolling map with collision and animation.

---

## 3D Fluid Simulation ([include/fluid3d.h](../include/fluid3d.h))

A 3D version of the cellular automata simulation that works hand in hand with the voxel engine. It is a separate type from the 2D `FluidSimulation`; the 2D API is unchanged. It simulates a bounded active region, reads solids from a `VoxelWorld` and writes fluid cells back as fluid blocks.

```cpp
FluidSimulation3D *create_fluid_simulation_3d(int width = 48, int height = 48, int depth = 48);
void destroy_fluid_simulation_3d(FluidSimulation3D *sim);
void fluid3d_step(FluidSimulation3D *sim);
bool fluid3d_set_cell(FluidSimulation3D *sim, int x, int y, int z, FluidElement type, float mass = 1.0f);
FluidCell fluid3d_get_cell(const FluidSimulation3D *sim, int x, int y, int z);

// Voxel bridge
void fluid3d_register_blocks(const FluidVoxelBlocks &blocks = {});   // block ids 200-212, one per element
void fluid3d_import_from_voxels(FluidSimulation3D *sim, const VoxelWorld &world, const FluidVoxelBlocks &blocks = {});
size_t fluid3d_export_to_voxels(FluidSimulation3D *sim, VoxelWorld &world, const FluidVoxelBlocks &blocks = {});
void fluid3d_move_region(FluidSimulation3D *sim, VoxelWorld &world, int ox, int oy, int oz, const FluidVoxelBlocks &blocks = {});
```

Typical loop: `fluid3d_import_from_voxels` once, then each frame `fluid3d_step`, `fluid3d_export_to_voxels` (only changed voxels are written) and `world.update_dirty_chunks()`.

Notes:
- The outermost layer of cells is read-only context (solid boundary); keep the area of interest inside it.
- Simulation is performed in 8x8x8 blocks that sleep when nothing changes. `max_blocks_per_step` caps the work per step; blocks are visited with a rotating cursor.
- Use `fluid3d_move_region` to make the region follow the player: it exports, moves the origin and re-imports. Fluid outside the region stays frozen as blocks.
- Differences from 2D: no mass-based pressure model (liquids level by sliding sideways), and only water quenches fire.
- See the `exfluid3d` example.

---

## 3D Flight & Orbital Dynamics ([include/flight.h](../include/flight.h))

`FlightWorld` is a double-precision 3D rigid-body simulation for Orbiter-style spaceflight and aircraft flight. See [Flight & Orbital Dynamics](flight.md) for the full guide.

```cpp
sl::FlightWorld world;
world.integrator = sl::FlightIntegrator::rk4;   // euler, semi_implicit, verlet, rk4, rk6
int earth = world.create_body("earth");
sl::FlightBody &e = *world.body(earth);
e.mode = sl::FlightBodyMode::kinematic;
e.gm = 3.986004418e14;
e.radius = 6.371e6;
int ship = world.create_body("ship");
// ... configure mass, thrusters, surfaces, gear ...
world.step(dt);
```

Key types: `FlightWorld`, `FlightBody`, `Thruster`, `ThrusterGroup`, `PropellantTank`, `LiftingSurface`, `GearContact`, `Atmosphere`, `OrbitElements`.
Free functions: `orbit_from_state`, `state_from_orbit`, `orbit_path`, `thruster_output`.
Notes:
- Coordinates are right-handed, Y up; the body frame is +X right, +Y up, -Z forward. Positions and velocities are `double` (`dvec3`); use `FlightWorld::render_matrix(id, origin)` to render with a floating origin.
- `step` splits `dt` into substeps no larger than `max_substep` (capped by `max_substeps_per_step`), so large time warps stay stable.
- Bodies in `rails` mode follow an analytic Kepler orbit and do not drift.
- See the `exorbit` and `exflight` examples.

---

## Chunked-LOD Terrain ([include/terrain.h](../include/terrain.h))

`Terrain` streams a quadtree of mesh chunks for a flat arena or a cube-sphere planet. See [Terrain](terrain.md) for the full guide.

```cpp
sl::TerrainConfig config;
config.size = 16384.0;                       // flat; or config.shape = TerrainShape::sphere with config.radius
sl::NoiseParameters noise;
noise.amplitude = 180.0;
sl::Terrain terrain(config, std::make_shared<sl::NoiseSource>(noise));
terrain.set_layer_textures();                // optional: textures config.layers with GPU texture arrays (OpenGL, Vulkan)
terrain.update(view);                        // every frame: select chunks, stream, upload
terrain.draw(origin);                        // inside begin_surface_3d / end_surface_3d
double h = terrain.height_at(x, z);
```

- Sources: `NoiseSource`, `HeightmapSource`, `GridSource` (editable), or your own `TerrainSource`.
- Queries: `height_at`, `normal_at`, `height_at_direction`, `normal_at_direction` and `raycast`.
- Chunks are built on worker threads and uploaded a few per frame; `wait_until_ready(view)` blocks until the requested chunks are built, for loading screens.
- Geomorphing hides LOD pops. Terrain meshes must be drawn through `Terrain::draw`, because they use the vertex tangent slot for the morph offset.
- `FlightWorld` can use a terrain as ground. Lua scripts get a `terrain` table.

---

## Atmosphere & Clutter ([include/atmosphere.h](../include/atmosphere.h), [include/clutter.h](../include/clutter.h))

Sky, fog, scattering, clouds and terrain-aware scattered objects. See [World Flavour](world_flavour.md) for the full guide.

```cpp
sl::set_environment_3d(env);                 // after begin_surface_3d(): fog, haze, clouds
sl::draw_sky_3d();
sl::ClutterLayer clutter(terrain);
clutter.add_species(trees);                  // mesh, density, slope/height rules, LOD distances
clutter.update(view);                        // outside begin_surface_3d
clutter.draw(origin);
```

- `Environment3D`: gradient sky and exponential fog for flat worlds, Rayleigh/Mie scattering for planets, up to four cloud layers (`clouds` plus `more_clouds[3]`), each a flat layer or ray-marched volumetric clouds (`volumetric`) of a `CloudType` (cumulus, stratocumulus, stratus, altocumulus, cirrus, cumulonimbus; `cloud_layer(type)` gives typical settings). See `src/examples/exclouds`.
- `atmosphere_sun_colour`, `atmosphere_sky_colour`, `atmosphere_transmittance`: CPU mirrors of the shader maths.
- `bake_instances`, `bake_impostor`, `bake_billboards`: merge copies of a mesh, or render view-dependent billboards.
- `Material::alpha_cutoff` and `Material::fog` control cut-outs and fog per material.
- `Material::blend` (`MaterialBlend::opaque`, `alpha`, `additive`) makes a material translucent, with the opacity taken from `tint.alpha`, vertex alpha and texture alpha. Blended draws test depth but do not write it and are not sorted: draw opaque geometry first, then blended meshes farthest first.

---

## Atlases & Animated Sprites ([include/atlas.h](../include/atlas.h), [include/animated_sprite.h](../include/animated_sprite.h))

- `create_atlas(bitmap, tile_w, tile_h, spacing = 0, margin = 0)` / `load_atlas_from_file(path, tile_w, tile_h)`: slice a sprite sheet into numbered tiles. `atlas_tile_rect` returns a tile's pixel rectangle.
- `atlas_blit`, `atlas_stretch_blit`, `atlas_rotate_sprite` and `atlas_rotate_stretch_sprite` draw tiles to a bitmap, with optional pivots.
- `AnimatedSprite` plays a run of atlas tiles with a per-frame duration, optional one-shot playback and rotation. Create one with `create_animated_sprite(atlas, start_tile, frames, x, y, w, h, duration)` and draw it with `draw_animated_sprite` or `draw_rotated_animated_sprite`.
- A sprite manager owns many sprites: `add_animated_sprite`, `update`, `draw_sprites`, `set_sprite_position`, `set_sprite_rotation`, `set_sprite_tiles`, `toggle_pause` and `remove_animated_sprite`. See `src/examples/exanimsprite`.

---

## Procedural Audio Synthesis ([include/audio_synth.h](../include/audio_synth.h))

`generate_sample(SynthParams)` renders signed 16-bit PCM into a `Sample` you can pass to `play_sample()`.

- `SynthParams`: `sample_rate`, `channels`, `duration`, a list of `SynthOscillator`, an `Adsr` envelope and `noise_seed`.
- `SynthOscillator`: `Waveform` (`sine`, `square`, `saw`, `triangle`, `noise`), `frequency`, `amplitude` and a `Pitch` sweep (`start` to `end` multipliers).
- See `src/examples/exsynth`.

---

## Video Playback ([include/video.h](../include/video.h))

Video playback into a bitmap. The backend is set at build time with `SIMLIB_VIDEO` (`plmpeg`: MPEG1/MP2 files; `ffmpeg`: most formats plus URLs; `none`: stubs).

- `seek_video(video, seconds)`, `video_position(video)`, `video_has_audio(video)`, `video_backend_name()`.
- Failures set `last_error()`; with `plmpeg`, `open_video` rejects URLs.

- `open_video(path, audio = true)` / `open_video_from_memory(data, size, audio = true)` / `destroy_video(video)`.
- `update_video(video, dt_seconds)` decodes the next frames; draw `video_bitmap(video)` like any other sprite.
- `video_width`, `video_height`, `video_duration`, `video_has_ended`, `set_video_loop` and `set_video_volume`.

---

## Dear ImGui Integration ([include/gui.h](../include/gui.h))

`gui_init()`, `gui_handle_event(event)`, `gui_new_frame()`, `gui_render()` and `gui_shutdown()` wrap Dear ImGui for all three backends. `gui_show_demo_window()` and `gui_show_metrics_window(show)` are quick diagnostics. See `src/examples/exgui`.

The GUI backend is chosen at configure time with `-DSIMLIB_GUI=imgui|none` (default `imgui`). With `none`, ImGui is not fetched, built, linked or installed, `<imgui.h>` is not included by `sl.h`, the `gui_*` functions are no-ops, and the ImGui examples (`exgui`, `exspriteeditor`, `exlobby`) are skipped. `sl::gui_available()` reports which mode the library was built in; `SIMLIB_GUI_IMGUI` is defined for consumers when ImGui is enabled.

---

## Tile-Grid Pathfinding ([include/pathfinding.h](../include/pathfinding.h))

A header-only, global-namespace flow-field helper. Define `PATHFINDING_IMPLEMENTATION` in exactly one source file before including it. See `src/examples/expathfinding`.

- `Pathfinder::resize(columns, rows, tile_w, tile_h, blocked_fn)` sets up the grid; `update(player_col, player_row, player_moving)` rebuilds the chase field and updates scent and sound each frame.
- `next_step(PathLayer, col, row, step)` returns the best neighbouring step: downhill on `PathLayer::chase`, uphill on `scent` or `sound`. Enemies read the field rather than searching.
- `open(col, row)` and `value(layer, col, row)` inspect the grid.

---

## Networking ([include/net.h](../include/net.h))

ENet-based UDP messaging plus non-blocking TCP. Everything is polled from your frame loop and nothing blocks. The objects initialise the library while open, so `net_initialise()` / `net_shutdown()` (reference counted) are optional. See the [networking guide](networking.md) and `src/examples/exlobby`.

```cpp
enum class NetEventType { connected, disconnected, received };
struct NetEvent { NetEventType type; std::uint32_t peer; std::uint8_t channel; std::vector<std::uint8_t> data; };

class NetServer {
public:
    bool start(std::uint16_t port, std::size_t max_clients = 32, std::size_t channels = 2); // port 0 = pick one
    void stop();
    bool running() const;
    std::uint16_t port() const;
    std::size_t client_count() const;
    bool poll(NetEvent &event);                      // call until false, every frame
    bool send(std::uint32_t peer, const std::vector<std::uint8_t> &data, std::uint8_t channel = 0, bool reliable = true);
    void broadcast(const std::vector<std::uint8_t> &data, std::uint8_t channel = 0, bool reliable = true);
    void disconnect(std::uint32_t peer);
};

class NetClient {
public:
    bool connect(const std::string &host, std::uint16_t port, std::size_t channels = 2); // result arrives via poll()
    void disconnect();
    bool connected() const;
    bool active() const;                             // connecting or connected
    bool poll(NetEvent &event);
    bool send(const std::vector<std::uint8_t> &data, std::uint8_t channel = 0, bool reliable = true);
    std::uint32_t ping_ms() const;
};

class PacketWriter {
public:
    template <typename T> void write(T value);       // arithmetic/enum, little-endian
    void write_string(std::string_view text);        // 16-bit length prefix
    void write_bytes(const void *data, std::size_t size);
    const std::vector<std::uint8_t> &bytes() const;
};

class PacketReader {                                 // reads never overrun; check every result
public:
    PacketReader(const std::vector<std::uint8_t> &data);
    template <typename T> bool read(T &value);
    bool read_string(std::string &text);
    bool read_bytes(void *out, std::size_t size);
    std::size_t remaining() const;
    bool ok() const;
};

enum class TcpState { closed, connecting, connected };

class TcpSocket {                                    // move-only, plain text (no TLS)
public:
    bool connect(const std::string &host, std::uint16_t port, int timeout_ms = 15000); // DNS on a worker thread
    TcpState update();                               // call every frame
    TcpState state() const;
    bool connected() const;
    bool send(std::string_view data);                // queued, 1 MiB backlog cap
    std::size_t receive(std::string &out);           // appends what has arrived
    bool wait_readable(int timeout_ms);              // for tools and tests
    void close();
    const std::string &error() const;
};

class TcpListener {
public:
    bool listen(std::uint16_t port, int backlog = 16);
    bool accept(TcpSocket &out);
    void close();
    bool listening() const;
    std::uint16_t port() const;
};
```

Packets from the network are untrusted: check every `read`, and clamp or range-check values before use. Peer ids are never 0. UDP and TCP do not interoperate (a `NetClient` cannot talk to an IRC server).

---

## Sandboxed Lua Scripting ([include/lua_runtime.h](../include/lua_runtime.h), [include/lua_canvas.h](../include/lua_canvas.h))

See [Sandboxed Lua Scripting & Canvas](scripting_lua.md) for the full script API.

- `LuaRuntime`: a hardened sol2 sandbox. `set_limits(LuaLimits)`, `initialise(output_handler)`, `execute(script)`, `call(function, argument)`, `emit(event, ...)` and `reset()`. Each returns a `LuaScriptResult`.
- `LuaCanvas`: scripted drawing on top of the runtime. `run_text`, `run_file`, `dispatch_keypress`, `set_asset_root`, `render(target)`, `clear` and `reset`.

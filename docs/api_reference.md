# Core API Reference

[← Back to README](../README.md)

This reference details the public C++ API provided by `simlib`. Including `sl.h` pulls in the entire public engine interface. Individual subsystem headers (located under [include/](include/)) can also be included independently when required.

---

## Table of Contents
1. [Error Handling](#error-handling-errorh)
2. [Application Lifecycle & Display](#application-lifecycle--display-displayh)
3. [Events, Input & Timing](#events-input--timing-eventh-inputh-systemh-gamepadh)
4. [Bitmaps, Offscreen Render Targets & Drawing](#bitmaps-offscreen-render-targets--drawing-drawh)
5. [Typography & Text Rendering](#typography--text-rendering-fonth)
6. [Shaders, Compute & Storage Buffers](#shaders-compute--storage-buffers-graphics_fxh)
7. [Post-Processing Graphics Effects](#post-processing-graphics-effects-graphics_fxh)
8. [2D Radial Lighting & Polygon Shadows](#2d-radial-lighting--polygon-shadows-graphics_fxh)
9. [Box2D Physics System](#box2d-physics-system-physicsh)
10. [Audio Subsystem (Samples & Streams)](#audio-subsystem-samples--streams-audioh)
11. [Cellular Automata Fluid & Granular Simulation](#cellular-automata-fluid--granular-simulation-fluidh)
12. [Particle Systems & Emitters](#particle-systems--emitters-particlesh)
13. [Procedural FastNoise Generation](#procedural-fastnoise-generation-noiseh)
14. [Embedded SQLite Persistence](#embedded-sqlite-persistence-rdbh)
15. [Resource Archives](#resource-archives-resourceh)

---

## Error Handling ([include/error.h](include/error.h))

`simlib` records thread-local error messages when subsystem initialization, file loading, or GPU resource allocation fails.

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

## Application Lifecycle & Display ([include/display.h](include/display.h))

The display subsystem handles window creation, graphics backend selection (OpenGL 4.3, Vulkan, Direct3D), resolution configuration, vsync, and viewport management.

### Types & Enumerations

- `enum class GraphicsBackend { opengl, vulkan, d3d11, d3d12 }`: Selects the hardware rendering backend.
- `constexpr int GFX_AUTODETECT_WINDOWED = 1`: Default windowed driver mode.

### Functions

#### `set_graphics_backend(backend)`
```cpp
bool set_graphics_backend(GraphicsBackend backend);
```
- **Description**: Explicitly selects the graphics rendering backend prior to window creation.
- **Parameters**:
  - `backend` (`GraphicsBackend`): The desired rendering backend (`opengl`, `vulkan`, `d3d11`, or `d3d12`).
- **Returns**: `true` if the specified backend is available in the current build; `false` otherwise.

#### `configure_graphics_backend_from_args(argc, argv)`
```cpp
bool configure_graphics_backend_from_args(int argc, char *argv[]);
```
- **Description**: Parses command-line arguments for backend flags (`--gl`, `--vulkan`, `--d3d11`, `--d3d12`) and configures the active backend accordingly. Defaults to OpenGL if no flag is supplied.
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
bool set_gfx_mode(int driver, int width, int height, int virtualWidth = 0, int virtualHeight = 0);
```
- **Description**: Creates the main application window, initializes the selected graphics context, and constructs the primary display framebuffer (`sl::screen`).
- **Parameters**:
  - `driver` (`int`): Driver selection (typically `sl::GFX_AUTODETECT_WINDOWED`).
  - `width` (`int`): Window width in pixels.
  - `height` (`int`): Window height in pixels.
  - `virtualWidth` (`int`, optional): Logical drawing canvas width (0 defaults to `width`).
  - `virtualHeight` (`int`, optional): Logical drawing canvas height (0 defaults to `height`).
- **Returns**: `true` on successful initialization; `false` on failure.

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
- **Description**: Configures vertical presentation synchronization.
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

## Events, Input & Timing ([include/event.h](include/event.h), [include/input.h](include/input.h), [include/system.h](include/system.h), [include/gamepad.h](include/gamepad.h))

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

#### `key_down(scancode)`
```cpp
bool key_down(SDL_Scancode key);
```
- **Description**: Queries whether a specific keyboard scancode is currently physically held down.
- **Parameters**:
  - `key` (`SDL_Scancode`): SDL scancode to test (e.g., `SDL_SCANCODE_SPACE`, `SDL_SCANCODE_W`).
- **Returns**: `true` if the key is held; `false` otherwise.

#### `mouse_x()` / `mouse_y()` / `mouse_buttons()`
```cpp
int mouse_x();
int mouse_y();
std::uint32_t mouse_buttons();
```
- **Description**: Queries the current cursor position (in window pixel coordinates) and held mouse button bitmask.
- **Returns**: Integer coordinates or bitfield (`SDL_BUTTON_LMASK`, `SDL_BUTTON_RMASK`).

#### `gamepad_init()` / `gamepad_count()` / `gamepad_connected(index)`
```cpp
bool gamepad_init();
int gamepad_count();
bool gamepad_connected(int index);
```
- **Description**: Initializes controller subsystems, counts active gamepads, and tests connection status by index.
- **Parameters**: `index` (`int`): 0-based controller device index.

#### `time_ms()`
```cpp
std::uint64_t time_ms();
```
- **Description**: Returns elapsed time in milliseconds since system initialization using SDL's monotonic timer.
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

---

## Bitmaps, Offscreen Render Targets & Drawing ([include/draw.h](include/draw.h))

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
};
```

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
  - `colour` (`Colour`): Clear color for the render target.
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
- **Description**: Synchronizes pixel data from CPU RAM to the GPU texture (`upload_bitmap`) or reads GPU texture pixels back into CPU RAM (`download_bitmap`).

#### `putpixel(bitmap, x, y, colour)` / `getpixel(bitmap, x, y)` / `flood_fill(...)`
```cpp
void putpixel(Bitmap *bitmap, int x, int y, Colour colour);
Colour getpixel(Bitmap *bitmap, int x, int y);
void flood_fill(Bitmap *bitmap, int x, int y, Colour colour);
```
- **Description**: Writes or reads an individual pixel, or performs a 4-way flood fill starting at `(x, y)`.

#### Sprite Drawing Functions
```cpp
void draw_sprite(Bitmap *bitmap, float x, float y);
void draw_sprite_stretched(Bitmap *bitmap, float x, float y, int width, int height);
void draw_sprite_rotated(Bitmap *bitmap, float centerX, float centerY, float angleDegrees);
void draw_sprite_rotated_stretched(Bitmap *bitmap, float centerX, float centerY, float angleDegrees, int width, int height);
void draw_sprite_h_flip(Bitmap *bitmap, float x, float y);
void draw_sprite_v_flip(Bitmap *bitmap, float x, float y);
```
- **Description**: Hardware-accelerated sprite rendering with positioning, scaling, center-point rotation (clockwise degrees), and horizontal/vertical flipping. Offscreen render targets are automatically orientation-corrected when drawn as sprites.

#### Shape Primitives
```cpp
void line(Bitmap *bitmap, float x1, float y1, float x2, float y2, Colour colour, float thickness = 1.0f);
void rect(Bitmap *bitmap, float left, float top, float right, float bottom, Colour colour, float thickness = 1.0f);
void rectfill(Bitmap *bitmap, float left, float top, float right, float bottom, Colour colour);
void circle(Bitmap *bitmap, float x, float y, float radius, Colour colour, float thickness = 1.0f);
void circlefill(Bitmap *bitmap, float x, float y, float radius, Colour colour);
void ellipse(Bitmap *bitmap, float x, float y, float radiusX, float radiusY, Colour colour, float thickness = 1.0f);
void ellipsefill(Bitmap *bitmap, float x, float y, float radiusX, float radiusY, Colour colour);
void triangle(Bitmap *bitmap, float x1, float y1, float x2, float y2, float x3, float y3, Colour colour, float thickness = 1.0f);
void trianglefill(Bitmap *bitmap, float x1, float y1, float x2, float y2, float x3, float y3, Colour colour);
```
- **Description**: Immediate-mode 2D outline and filled shape primitives with customizable line thickness. Pass `sl::screen` to render directly to the window.

---

## Typography & Text Rendering ([include/font.h](include/font.h))

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
  - `colour` (`Colour`): Text color.
  - `text` (`const std::string&`): UTF-8 text string to render.

#### `gprintf(x, y, colour, format, ...)` / `gprintf_center(y, colour, format, ...)`
```cpp
void gprintf(int x, int y, const Colour &colour, const char *fmt, ...);
void gprintf_center(int y, const Colour &colour, const char *fmt, ...);
```
- **Description**: Convenience functions that format and render text using the default monospace font, positioned at `(x, y)` or horizontally centered on screen.

---

## Shaders, Compute & Storage Buffers ([include/graphics_fx.h](include/graphics_fx.h))

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

## Post-Processing Graphics Effects ([include/graphics_fx.h](include/graphics_fx.h))

Post-processing effects process regions or full-screen bitmaps using pre-compiled GPU pipeline passes.

### Classes & Effects

- **`sl::PingPongBuffer`**: Manages double-buffered offscreen render targets for multi-pass post-processing chains (`begin`, `advance`, `source`, `target`).
- **`sl::Bloom`**: Thresholds bright pixels, downsamples, applies separable Gaussian blur, and composites glow (`set_threshold`, `set_intensity`, `set_radius`, `apply`).
- **`sl::Vignette`**: Darkens image edges in a soft-radius circle (`set_radius`, `set_softness`, `set_intensity`, `apply`).
- **`sl::ColourAdjust`**: Adjusts brightness, contrast, saturation, and exposure (`set_brightness`, `set_contrast`, `set_saturation`, `set_exposure`, `apply`).
- **`sl::ScreenShake`**: Applies decaying camera offsets to 2D projections (`trigger(amplitude, duration)`, `update(delta_seconds)`, `active()`).
- Additional effects available: `sl::Blur`, `sl::ChromaticAberration`, `sl::Pixelate`, `sl::RadialBlur`, `sl::HeatHaze`, `sl::Shockwave`, `sl::CRTFilter`, `sl::DitherFilter`, `sl::FilmGrain`.

---

## 2D Radial Lighting & Polygon Shadows ([include/graphics_fx.h](include/graphics_fx.h))

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

## Box2D Physics System ([include/physics.h](include/physics.h))

`simlib` provides an integrated Box2D rigid-body simulation wrapper operating directly in pixel coordinates (64 pixels = 1 meter).

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

## Audio Subsystem (Samples & Streams) ([include/audio.h](include/audio.h))

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

## Cellular Automata Fluid & Granular Simulation ([include/fluid.h](include/fluid.h))

Multi-material Cellular Automata simulation combining fluids, falling sand, gases, fire, thermodynamics, and Box2D Archimedes buoyancy coupling.

```cpp
CAFluidGrid *create_fluid_simulation(int width, int height);
void destroy_fluid_simulation(CAFluidGrid *sim);
void fluid_step(CAFluidGrid *sim);
void fluid_set_cell(CAFluidGrid *sim, int x, int y, uint8_t element, float mass, float temp, uint8_t life, uint8_t variation);
FluidCell fluid_get_cell(CAFluidGrid *sim, int x, int y);
void fluid_clear(CAFluidGrid *sim);
void fluid_reset_bounds(CAFluidGrid *sim);
```

---

## Particle Systems & Emitters ([include/particles.h](include/particles.h))

Configurable 2D particle emitters with velocity spread, size scaling, color fading, and gravity.

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

## Procedural FastNoise Generation ([include/noise.h](include/noise.h))

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

## Embedded SQLite Persistence ([include/rdb.h](include/rdb.h))

Provides RAII-managed C++ wrappers around SQLite for database creation, prepared statements, and transactional safety.

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

## Resource Archives ([include/resource.h](include/resource.h))

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

[← Back to README](../README.md)


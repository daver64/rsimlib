# Sandboxed Lua Scripting & Canvas

[← Back to README](../README.md)

`simlib` provides an embedded, hardened Lua runtime using [sol2](https://github.com/ThePhD/sol2), along with a high-level `LuaCanvas` drawing and event pipeline.

---

## Sandbox Security Model

The runtime disables dangerous operations to prevent filesystem, command execution, or memory escapes:
- `io`, `package`, `debug`, and unsafe `os` libraries are excluded.
- `dofile`, `loadfile`, and `load` are removed.
- `os.time()` and `os.clock()` are provided as safe time measurement primitives.

```cpp
sol::state lua;
lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::math, sol::lib::table);
lua["dofile"] = sol::nil;
lua["loadfile"] = sol::nil;
lua["load"] = sol::nil;

sol::table app = lua.create_named_table("app");
app.set_function("quit", []() { /* ... */ });
```

---

## Lua Drawing Commands (`app`)

`LuaCanvas` exposes persistent drawing commands under `app`. Commands persist and re-render every frame until `app.clear_drawings()` or `app.clear_sprites()` is called:

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

### Sprites, Render Targets & Sound in Lua
```lua
-- Sprites & Offscreen Render Targets
ok, message = app.load_sprite("balloon", "textures/balloon_red.png")
app.sprite("balloon", 100, 150)
app.sprite_stretched("balloon", 300, 150, 96, 128)
app.sprite_rotated("balloon", 500, 200, 45)

app.create_render_target("minimap", 128, 128)
app.sprite("minimap", 650, 20) -- Offscreen render targets can be drawn directly as sprites!

-- Audio
ok = app.load_sound("jump", "sfx/jump.wav")
voice = app.play_sound("jump")
app.stop_sound(voice)

app.load_music("theme", "music/theme.ogg")
app.play_music("theme")
```

---

## Compute Shaders & SSBOs (`compute`)

Compute shader loading, StorageBuffer allocation, float array upload/readback, and workgroup grid dispatches are available directly in Lua:

```lua
local compute_src = [[
#version 430 core
layout(local_size_x = 16) in;
layout(std430, binding = 0) buffer DataBuffer {
    float values[];
};
void main() {
    uint id = gl_GlobalInvocationID.x;
    if (id < values.length()) values[id] *= 2.0;
}
]]

compute.load_shader("doubler", compute_src)
compute.create_buffer("buf1", 64) -- 16 floats = 64 bytes
compute.upload_floats("buf1", {1.0, 2.0, 3.0, 4.0})
compute.bind_buffer("buf1", 0)

compute.dispatch_for("doubler", 4, 1, 1, 16, 1, 1)
compute.barrier()

local results = compute.readback_floats("buf1", 4)
-- results[1] == 2.0, results[2] == 4.0, results[3] == 6.0, results[4] == 8.0
```

---

## Database Operations (`rdb`)

Lua scripts can query SQLite, PostgreSQL, or MySQL databases:

```lua
rdb.connect("db", "sqlite", ":memory:")
rdb.execute("db", "CREATE TABLE stats (score INT, player TEXT);")
rdb.execute("db", "INSERT INTO stats VALUES (1500, 'Hero');")

local rows = rdb.query("db", "SELECT * FROM stats;")
for i, row in ipairs(rows) do
    print(row.player, "Score:", row.score)
end
rdb.disconnect("db")
```

---

## Screen Shake & Post-Processing (`fx`)

```lua
-- Camera Shake
fx.shake(8.0, 0.3) -- amplitude, duration in seconds
fx.update_shake(delta_time)

-- Bloom & Vignette
fx.bloom_init()
fx.bloom_config(0.7, 1.2, 2.0)
fx.apply_bloom("screen_rt")

fx.vignette_init()
fx.vignette_config(0.8, 0.4, 0.7)
fx.apply_vignette("screen_rt")
```

---

## Display & System Metrics (`display`, `system`)

```lua
local w = display.width()
local h = display.height()
display.set_title("My Lua Game")

local fps = system.get_fps()
local dt = system.get_frame_time()
```

---

## Application Event Callbacks

Lua scripts can define event callbacks invoked from C++:

```lua
function on_keypress(key)
    if key == "space" then
        app.clear_drawings()
        app.circlefill(400, 300, 80, 255, 190, 70)
    end
end

function on_player_scored(points)
    print("Score:", points)
end
```

### Dispatching from C++
```cpp
sl::LuaCanvas canvas;
if (canvas.run_file("assets/scripts/game.lua").success)
{
    canvas.dispatch_keypress("space");
    canvas.runtime().emit("on_player_scored", 100);
}
```

---

[← Back to README](../README.md)

# Sandboxed Lua Scripting & Canvas

[← Back to README](../README.md)

`simlib` provides an embedded, hardened Lua runtime using [sol2](https://github.com/ThePhD/sol2), along with a high-level `LuaCanvas` drawing and event pipeline.

---

## Sandbox Security Model

The runtime disables dangerous operations to prevent filesystem, command execution, or memory escapes:
- `io`, `package`, `debug`, and unsafe `os` libraries are excluded.
- `dofile`, `loadfile`, and `load` are removed.
- `os.time()` and `os.clock()` are provided as safe time measurement primitives.
- Scripts are resource limited. Each `execute()` or callback call may run at most 100 million Lua VM instructions, and the state may allocate at most 256 MiB. Exceeding either limit raises an ordinary Lua error, which is returned in `LuaScriptResult` and leaves the runtime usable. Adjust or disable (0) the limits from C++:

```cpp
sl::LuaLimits limits;
limits.max_instructions = 20'000'000;
limits.max_memory_bytes = 64u * 1024u * 1024u;
canvas.runtime().set_limits(limits);
```

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

Resource management and the remaining sprite and audio calls:
```lua
app.sprite_rotated_stretched("balloon", 400, 300, 45, 96, 128) -- centre x/y, degrees, width, height
app.unload_sprite("balloon")
app.destroy_render_target("minimap")

app.unload_sound("jump")
app.pause_music(); app.resume_music(); app.stop_music()
app.set_music_volume(64)        -- 0-128
app.unload_music("theme")
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

Other compute calls: `compute.set_uniform_float(shader, name, value)`, `compute.destroy_shader(id)` and `compute.destroy_buffer(id)`.

**Backend note:** on Vulkan, GLSL compute shaders cannot declare loose `uniform` variables, so `compute.set_uniform_float` has no effect there. Keep parameters in a storage buffer, or write the shader with a `push_constant` block (see [`exgpuparticles`](exgpuparticles_guide.md)). `readback_floats` returns up-to-date results on every backend.

---

## Tile Maps (`tilemap`)

Load a Tiled JSON map (`.tmj`/`.json`) relative to the asset root, query it, and draw it. Columns, rows and layers are 0-based.

```lua
tilemap.load("level", "maps/level1.tmj")
local info = tilemap.info("level")          -- width, height, tile_width, tile_height, layers, isometric

tilemap.draw("level", cam_x, cam_y)         -- retained; call again to move the camera (optional view width, height)
tilemap.update("level", dt)                 -- advances animated tiles

local gid = tilemap.get("level", 0, col, row)
tilemap.set("level", 0, col, row, gid)
if tilemap.rect_blocked("level", x, y, w, h) then end   -- collision against solid tiles
local col, row = tilemap.world_to_tile("level", x, y)
local x, y = tilemap.tile_to_world("level", col, row)
local spawn = tilemap.find_object("level", "spawn")  -- {name, type, x, y, width, height, properties} or nil
local kind = tilemap.property("level", gid, "kind")
tilemap.unload("level")
```

Also `tilemap.is_solid(id, col, row [, layer])` and `tilemap.set_solid(id, gid, solid)`.

---

## Fluid Simulations (`fluid`, `fluid3d`)

Elements are named by string: `empty`, `solid`, `wood`, `sand`, `water`, `oil`, `acid`, `lava`, `gunpowder`, `plant`, `pump`, `fire`, `smoke`, `steam`. 2D grids are limited to 1024x1024 cells and 3D grids to 128 cells per axis.

```lua
fluid.create("pool", 200, 150)
fluid.set_cell("pool", 100, 10, "water", 1.0)     -- optional mass and temperature
fluid.step("pool", 1)                             -- optional step count
fluid.draw("pool", 20, 20, 3)                     -- retained; x, y, pixels per cell
local cell = fluid.get_cell("pool", 100, 10)      -- {element, mass, temp}, or nil when out of bounds
fluid.destroy("pool")
```

`fluid3d` works with a voxel world: solids are read from the world and fluid is written back as fluid blocks.

```lua
voxel.create_world("w")
fluid3d.create("sim", 48, 36, 48, -24, 0, -24)    -- size, then world-space origin
fluid3d.import("sim", "w")                        -- read solids from the world
fluid3d.set_cell("sim", 10, 20, 10, "water")
fluid3d.step("sim", 1)
local changed = fluid3d.export("sim", "w")        -- write fluid blocks back; returns voxels changed
fluid3d.move_region("sim", "w", px - 24, 0, pz - 24)  -- follow the player
fluid3d.set_budget("sim", 64)                     -- cap awake 8x8x8 blocks stepped per step (0 = unlimited)
```

See [3D Fluid Simulation](api_reference.md#3d-fluid-simulation-includefluid3dh) for the rules.

---

## Voxel Worlds (`voxel`)

```lua
voxel.create_world("w")
voxel.define_block(1, {90, 160, 90}, {solid = true, transparent = false, emission = 0})  -- block ids 1-255
voxel.set_block("w", x, y, z, 1)
voxel.fill("w", 0, 0, 0, 31, 0, 31, 1)            -- inclusive box, up to 4 million blocks
local id = voxel.get_block("w", x, y, z)

-- Cast a ray; returns nil on a miss
local hit = voxel.raycast("w", ex, ey, ez, dx, dy, dz, 16)
if hit then voxel.set_block("w", hit.place_x, hit.place_y, hit.place_z, 1) end  -- also x, y, z, face, distance

-- Retained 3D view drawn at (x, y) on the canvas; call again each frame to move the camera
voxel.view("w", 0, 0, 640, 400, {40, 30, 40}, {12, 2, 12}, 60, {18, 22, 36})  -- eye, target, fov, sky colour
local stats = voxel.stats("w")                    -- {chunks, triangles}
voxel.destroy_world("w")
```

Blocks use flat tint colours (no texture atlas) in Lua. Fluid blocks 200-212 are registered by `fluid3d.create`.

---

## Database Operations (`rdb`)

Database operations can be registered into Lua via the `rdb::register_lua(lua)` helper in `src/examples/exdb/rdb_lua.h`:

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

if fx.shake_active() then fx.clear_shake() end
```

---

## Display & System Metrics (`display`, `system`)

```lua
local w = display.width()
local h = display.height()
display.set_title("My Lua Game")

local fps = system.get_fps()
local dt = system.get_frame_time()
system.set_fps(60)

local vw, vh = display.virtual_width(), display.virtual_height()
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

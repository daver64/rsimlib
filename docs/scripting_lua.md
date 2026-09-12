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

### Sprites & Sound in Lua
```lua
-- Sprites
ok, message = app.load_sprite("balloon", "textures/balloon_red.png")
app.sprite("balloon", 100, 150)
app.sprite_stretched("balloon", 300, 150, 96, 128)
app.sprite_rotated("balloon", 500, 200, 45)

-- Audio
ok = app.load_sound("jump", "sfx/jump.wav")
voice = app.play_sound("jump")
app.stop_sound(voice)

app.load_music("theme", "music/theme.ogg")
app.play_music("theme")
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

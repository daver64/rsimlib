# Core API Reference

[← Back to README](../README.md)

Include `sl.h` to access the complete public API, or include an individual header when you only need one subsystem.

---

## Application Lifecycle & Display (`display.h`)

| Function | Description |
| --- | --- |
| `configure_graphics_backend_from_args(argc, argv)` | Select `--gl` or `--vulkan`; OpenGL is selected when neither flag is present. Call before `set_gfx_mode()`. |
| `set_gfx_mode(driver, width, height, virtualWidth, virtualHeight)` | Create the SDL window and selected graphics backend, then initialize the display bitmap. Use `GFX_AUTODETECT_WINDOWED` for the default windowed driver. |
| `display_handle_event(event)` | Apply display-related events, including window resizing. Call for every polled event. |
| `set_window_title(title)` | Set the native window title. |
| `set_fullscreen(enabled)` / `toggle_fullscreen()` | Enter, leave, or toggle desktop fullscreen mode. |
| `set_vsync(enabled)` | Enable or disable presentation synchronization for the selected backend. |
| `screen_width()` / `screen_height()` | Return the current drawable dimensions in pixels. |
| `virtual_screen_width()` / `virtual_screen_height()` | Return the configured logical drawing dimensions. |
| `clear_to_colour(red, green, blue, alpha)` | Clear the current display framebuffer. |
| `show_video_bitmap()` | Present the current framebuffer to the window. |
| `wait_for_graphics()` | Wait for GPU command queues to finish before releasing resources. |
| `display_shutdown()` | Release the window, graphics context, and display resources. |

---

## Events, Input & Timing (`event.h`, `input.h`, `system.h`)

| Function | Description |
| --- | --- |
| `poll_event(&event)` | Remove the next event from the queue; returns `false` when empty. |
| `key_down(scancode)` | Test whether a keyboard scancode is currently held. |
| `mouse_x()` / `mouse_y()` | Return the current mouse position in window pixels. |
| `mouse_buttons()` | Return the current mouse-button bitmask. |
| `time_ms()` | Return elapsed milliseconds from SDL's monotonic timer. |
| `set_fps(fps)` / `get_fps()` | Set or read the target frame rate; `0` means uncapped. |
| `get_frame_time()` | Return the duration of the last completed frame in milliseconds. |
| `end_frame()` | Finish a frame, maintain the configured frame rate, and record frame timing. |
| `rest(milliseconds)` | Delay execution for approximately the requested duration. |

---

## Bitmaps, Render Targets & Drawing (`draw.h`)

| Function | Description |
| --- | --- |
| `create_bitmap(width, height)` | Create a RAM-backed RGBA bitmap. |
| `create_video_bitmap(width, height)` | Create a GPU-backed bitmap. |
| `create_render_target(width, height)` | Create a GPU bitmap that can receive drawing commands as an offscreen framebuffer. |
| `begin_render_target(target)` / `end_render_target()` | Redirect drawing to an offscreen target, then restore drawing to the window. |
| `clear_render_target(colour)` | Clear the currently bound render target. |
| `load_bitmap(path)` | Load an image file into a bitmap (also supports memory and `Archive` sources). |
| `save_bitmap(bitmap, path)` | Save a bitmap as an uncompressed PNG. |
| `destroy_bitmap(bitmap)` | Release a bitmap and its GPU resources. |
| `upload_bitmap(bitmap)` / `download_bitmap(bitmap)` | Synchronize pixel data between RAM and GPU memory. |
| `bind_texture(unit, bitmap)` | Bind a bitmap's GPU texture to an active texture unit (`0..N-1`). |
| `clear_to_colour(bitmap, colour)` | Fill a bitmap with one colour. |
| `putpixel(bitmap, x, y, colour)` / `getpixel(bitmap, x, y)` | Write or read one pixel. |
| `flood_fill(bitmap, x, y, colour)` | Flood-fill an enclosed area of a bitmap starting from (x, y) with a replacement colour. |
| `draw_sprite(bitmap, x, y)` | Draw a bitmap at a top-left position. |
| `draw_sprite_stretched(bitmap, x, y, width, height)` | Draw a bitmap scaled to a destination rectangle. |
| `draw_sprite_rotated(bitmap, centerX, centerY, angleDegrees)` | Draw a bitmap around its centre with clockwise rotation. |
| `draw_sprite_h_flip(bitmap, x, y)` / `draw_sprite_v_flip(bitmap, x, y)` | Draw a horizontally or vertically flipped bitmap. |
| `blit(source, destination, ...)` / `stretch_blit(...)` | Copy bitmap regions with or without scaling. |
| `create_sub_bitmap(parent, x, y, width, height)` | Create a bitmap view containing a copied rectangular region. |
| `create_atlas(bitmap, tile_w, tile_h, spacing, margin)` | Slices a bitmap into a uniform grid atlas (`Atlas`). |
| `atlas_blit(atlas, dest, tile_index, x, y)` / `atlas_stretch_blit(...)` | Blit or stretch-blit a tile from an atlas to a destination bitmap. |

### Shape Primitives
- `line(bitmap, x1, y1, x2, y2, colour, thickness = 1.0f)`
- `rect(bitmap, left, top, right, bottom, colour, thickness = 1.0f)` / `rectfill(...)`
- `circle(bitmap, x, y, radius, colour, thickness = 1.0f)` / `circlefill(...)`
- `ellipse(bitmap, x, y, radiusX, radiusY, colour, thickness = 1.0f)` / `ellipsefill(...)`
- `triangle(bitmap, x1, y1, x2, y2, x3, y3, colour, thickness = 1.0f)` / `trianglefill(...)`

---

## Typography & Text (`font.h`)

| Function | Description |
| --- | --- |
| `get_default_monospace_font()` | Return the default monospace font. |
| `open_font(path, pointSize)` | Open a TrueType or OpenType font from a file, memory, or `Archive`. |
| `open_monospace_font(pointSize)` / `open_sans_font(pointSize)` | Open built-in platform font fallbacks. |
| `close_font(font)` | Release a font returned by an `open_*` function. |
| `text_length(font, text)` / `text_height(font)` | Measure rendered text in pixels. |
| `textout(font, x, y, colour, text)` | Draw UTF-8 text using the font's persistent glyph atlas. |
| `textprintf(font, x, y, colour, format, ...)` | Format and draw text using `printf`-style arguments. |
| `gprintf(x, y, colour, format, ...)` | Format and draw text using the default monospace font. |
| `gprintf_center(y, colour, format, ...)` | Draw default-font text horizontally centred on the screen. |

---

## Audio Subsystems (`audio.h`)

| Function | Description |
| --- | --- |
| `audio_fx_init()` / `audio_fx_shutdown()` | Initialize or release sound-effect playback. |
| `load_sample(path)` / `destroy_sample(sample)` | Load or release a sound effect. |
| `play_sample(sample, volume, pan, frequency, loops)` | Play a sample (volume & pan range `0`-`255`). |
| `stop_voice(voice)` / `stop_sample(sample)` / `stop_all_samples()` | Stop active sound effect playback. |
| `audio_fx_set_volume(volume)` | Set global sound-effect volume. |
| `music_init()` / `music_shutdown()` | Initialize or release music stream playback. |
| `load_stream(path)` / `destroy_stream(stream)` | Load or release a music stream. |
| `play_stream(stream, loops)` | Start music playback (default loops forever). |
| `stop_stream()` / `pause_stream()` / `resume_stream()` | Control active music playback. |
| `music_set_volume(volume)` | Set global music stream volume. |

---

## Cellular Automata Fluid & Granular Simulation (`fluid.h`)

| Function | Description |
| --- | --- |
| `create_fluid_simulation(width, height)` | Create a CA fluid simulation instance. |
| `destroy_fluid_simulation(sim)` | Release a simulation instance. |
| `fluid_step(sim)` | Advance fluid, thermodynamics, sand, gas, fire, and chemistry by one tick. |
| `fluid_set_cell(sim, x, y, element, mass, temp, life, variation)` | Set state for a grid cell. |
| `fluid_get_cell(sim, x, y)` | Query state of a grid cell. |
| `fluid_clear(sim)` | Reset all cells to empty. |
| `fluid_reset_bounds(sim)` | Re-initialize solid barrier boundary walls. |

---

## Resource Archives (`resource.h`)

| Member | Description |
| --- | --- |
| `archive.open(path)` | Open a ZIP archive from disk. |
| `archive.close()` | Close the archive and release file handles. |
| `archive.contains(name)` | Test whether an entry exists. |
| `archive.entries()` | List file and directory entry names in archive order. |
| `archive.read(name)` | Read an entry into a byte vector. |

---

[← Back to README](../README.md)

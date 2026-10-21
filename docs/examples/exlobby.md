# exlobby

A game lobby that finds games through an IRC channel and connects players directly over UDP with `NetServer` / `NetClient`.

![exlobby screenshot](img/exlobby.png)

## What it shows
- IRC channel discovery (`!host` announcements)
- Hosting and joining games
- Readying up and moving a dot in a shared arena
- ImGui lobby UI

## Running
```bash
./build/exlobby [--irc-host HOST] [--irc-port PORT] [--channel #CHANNEL] [--nick NAME] [--advertise ADDRESS] [--game-port PORT]
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Only built when the library is configured with `SIMLIB_GUI=imgui` (the default). Hosting needs a reachable address and an open UDP port. IRC is unencrypted, so do not type anything private.

## Controls
| Input | Action |
|-------|--------|
| Mouse | Use the lobby UI |
| W A S D / arrows | Move in the arena |
| Escape | Exit |

## Key APIs
- `NetServer`, `NetClient` — [Networking](../api_reference.md#networking-includeneth) · [`net.h`](../../include/net.h)
- GUI functions — [Dear ImGui](../api_reference.md#dear-imgui-integration-includeguih) · [`gui.h`](../../include/gui.h)

## Source
[src/examples/exlobby/exlobby.cpp](../../src/examples/exlobby/exlobby.cpp)

## See also
- [exgui](exgui.md)

See also the [Networking guide](../networking.md).

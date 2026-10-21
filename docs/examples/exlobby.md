# exlobby

A game lobby that finds games through an IRC channel and connects players directly over UDP with `NetServer` / `NetClient`.

![exlobby screenshot](img/exlobby.png)

## What it shows
- IRC channel discovery (`!host` announcements)
- Hosting and joining games
- Readying up and moving a dot in a shared arena
- ImGui lobby UI

## Key code

In `exlobby.cpp`, a hosting player announces the game in the IRC channel with a `!host` command, re-announcing periodically:

```cpp
irc.update();
// ...
if (hosting_open && online && (now >= next_announce || (announce_now && now - last_announce > 5000)))
{
    irc.say(lobby::make_host_command(advertise, session.port(), max_players, title));
    announced = true;
    last_announce = now;
    next_announce = now + 30000;
}
```

The ImGui lobby UI hosts or joins a game and toggles readiness:

```cpp
if (ImGui::Button("Join"))
{
    if (session.join(game.address, game.port, nick))
        notice.clear();
}
// ...
if (ImGui::Button("Host game"))
{
    if (session.host(static_cast<std::uint16_t>(port), max_players, nick))
// ...
if (ImGui::Checkbox("Ready", &ready))
    session.set_ready(ready);
```

Once playing, arrow/WASD input moves the player's dot in the shared arena:

```cpp
for (const lobby::Player &p : session.players())
    if (p.id == session.my_id())
        session.set_position(p.x + dx * 0.4f * dt, p.y + dy * 0.4f * dt);
```

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

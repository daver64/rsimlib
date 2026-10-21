# Networking ([include/net.h](../include/net.h))

simlib networking is polled from your frame loop. Nothing blocks, and there are no callbacks from other threads.
The UDP part is built on [ENet](https://github.com/zpl-c/enet) (MIT licence, bundled in
`src/engine/third_party/enet.h`); ENet itself is an implementation detail and is not part of the public headers.

```cpp
#include "sl.h"   // or "net.h"
```

There is no `net_initialise()` call to make for normal use: the objects below initialise the library while they are
open. `net_initialise()` / `net_shutdown()` exist for code that wants to hold it open explicitly (calls are counted).

## UDP: `NetServer` and `NetClient`

Use UDP for game state. Every packet is sent on a numbered channel, either reliably (resent until it arrives, in
order) or unreliably (may be lost; best for frequently repeated data such as positions).

```cpp
sl::NetServer server;
server.start(7777, /*max_clients*/ 8, /*channels*/ 2);

sl::NetEvent event;
while (server.poll(event))
{
    switch (event.type)
    {
    case sl::NetEventType::connected:    /* event.peer is the new client */ break;
    case sl::NetEventType::disconnected: break;
    case sl::NetEventType::received:     /* event.channel, event.data */ break;
    }
}
server.broadcast(bytes, /*channel*/ 1, /*reliable*/ false);
```

```cpp
sl::NetClient client;
client.connect("example.org", 7777);      // host names are fine; the result arrives from poll()
while (client.poll(event)) { /* connected, disconnected (also on failure), received */ }
client.send(bytes, 0, true);
```

Port `0` for `start()` picks a free port (see `port()`). `peer` ids are never 0 and are unique among the connected
peers of a server. `NetClient::ping_ms()` reports round trip time.

## Packets: `PacketWriter` and `PacketReader`

```cpp
sl::PacketWriter out;
out.write<std::uint8_t>(1);
out.write(3.5f);
out.write_string("hello");            // 16-bit length prefix
server.send(peer, out.bytes());

sl::PacketReader in(event.data);
std::uint8_t type; float value; std::string text;
if (in.read(type) && in.read(value) && in.read_string(text)) { /* ... */ }
```

Numbers are little-endian on the wire on every platform. Reads never run past the end of the packet: a short or
malformed packet makes the read return `false` (and `ok()` stay false), so always check the results. **Treat every
packet as untrusted** and validate values (clamp, range check, limit counts) before using them.

## TCP: `TcpSocket` and `TcpListener`

Use TCP to talk to services that speak it, such as IRC or HTTP.

```cpp
sl::TcpSocket socket;
socket.connect("irc.libera.chat", 6667);       // returns at once; DNS runs on a worker thread

// every frame:
socket.update();                               // advances connecting, flushes queued data
if (socket.connected())
{
    socket.send("NICK demo\r\n");              // queued (1 MiB cap), never blocks
    std::string text;
    socket.receive(text);                      // whatever has arrived, possibly nothing
}
if (socket.state() == sl::TcpState::closed && !socket.error().empty()) { /* failed or lost */ }
```

`TcpSocket` is a byte stream: a `receive()` can return half a line or several lines, so buffer and split yourself
(`src/examples/exlobby/irc_client.h` shows one way). It is move-only. `TcpListener` (`listen`, `accept`) accepts
incoming connections and is handy for tests and tools.

## Notes

- **No encryption.** `TcpSocket` is plain text; the lobby's IRC connection is not private. Do not send passwords
  or personal data over it.
- UDP and TCP are separate protocols: a `NetClient` cannot talk to an IRC server, and a `TcpSocket` cannot talk to a
  `NetServer`.
- IPv4 and IPv6 are both supported (dual-stack sockets), and host names are resolved without blocking the frame loop.
- Hosting a game behind a home router needs the UDP port forwarded; simlib does not do NAT traversal.
- The Windows build links `ws2_32` and `winmm`; the Windows and D3D12 paths have not been compiled by the Linux
  development build.

## Example: `exlobby` ([src/examples/exlobby](../src/examples/exlobby))

A game lobby that uses an IRC channel to find games:

```
./build/exlobby                                   # irc.libera.chat, #simlib-lobby
./build/exlobby --irc-host irc.example.org --irc-port 6667 --channel "#mygames" --nick me
./build/exlobby --advertise my.public.address --game-port 7777
```

| Option | Meaning |
|---|---|
| `--irc-host HOST` | IRC server to use; connects at start when given (otherwise press **Connect**) |
| `--irc-port PORT` | IRC port, plain text (default 6667) |
| `--channel #NAME` | Channel to meet in |
| `--nick NAME` | Your nick |
| `--advertise ADDRESS` | Address other players use to reach you when hosting |
| `--game-port PORT` | UDP port for hosted games (default 7777) |
| `--gl`, `--vulkan`, `--d3d12` | Graphics backend |

The host, port, nick and channel can also be edited in the window.

Hosts announce games in the channel with plain text commands, so a normal IRC client can see them too:

```
!host <address>:<port> <max players 2-16> <title up to 40 characters>
!close
!games        (asks hosts to announce again)
```

Announcements are validated before they are listed, expire after 90 seconds, and hosts re-announce every 30 seconds.
Players then connect directly to the host over UDP, ready up, and the host starts the game; everyone moves a dot
around a shared arena (arrow keys or WASD). The example is polite to IRC networks: it rate limits what it sends,
ignores CTCP requests and uses a plain, clearly named client.

`tests/lobby_test.cpp` covers the IRC parser, the game board, the IRC client against an in-process fake IRC server,
and the host/guest session over loopback, all offline.

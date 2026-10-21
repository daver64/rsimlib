# exdb

A small embedded database: opens a file, reads rows and lists them on screen.

![exdb screenshot](img/exdb.png)

## What it shows
- Opening a database file
- Reading rows
- Showing the row count

## Running
```bash
./build/exdb
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows.

## Controls
| Input | Action |
|-------|--------|
| Escape | Exit |

## Key APIs
- Database — [Embedded SQLite](../api_reference.md#embedded-sqlite-persistence-srcexamplesexdbrdbh)

## Source
[src/examples/exdb/exdb.cpp](../../src/examples/exdb/exdb.cpp)

## See also
- [exconfig](exconfig.md)
- [exresources](exresources.md)

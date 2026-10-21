# exdb

A small embedded database: opens a file, reads rows and lists them on screen.

![exdb screenshot](img/exdb.png)

## What it shows
- Opening a database file
- Reading rows
- Showing the row count

## Key code

In `exdb.cpp`, a database file is opened with `rdb::Database` and a table is created.

```cpp
rdb::Database database(filename);
database.execute("DROP TABLE IF EXISTS products;");
database.execute(
    "CREATE TABLE products ("
    "id INTEGER PRIMARY KEY, "
    "name TEXT NOT NULL, "
    // ...
    ");");
```

Rows are then read back with a prepared `SELECT` and `forEachRow`.

```cpp
auto select = database.prepare(
    "SELECT id, name, category, price, stock FROM products ORDER BY id;");
select->forEachRow([&rows](rdb::Statement &row)
                   { rows.push_back({row.getInt(0), row.getText(1), row.getText(2),
                                     row.getDouble(3), row.getInt(4)}); });
```

The row count is shown as live text in `draw_table`.

```cpp
sl::gprintf(32, 72, muted, "Rows: %d    Press Escape to exit", static_cast<int>(products.size()));
```

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

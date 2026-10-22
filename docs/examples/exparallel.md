# exparallel

Draws a field of thousands of cubes with one `draw_mesh()` call each, in three ways: recorded on one thread, recorded on worker threads by the backend, and built by your own threads with `RenderQueue3D`.

![exparallel screenshot](img/exparallel.png)

## What it shows
- Vulkan and Direct3D 12 recording large batches of draws on several threads, with no change to your drawing code (`set_parallel_recording()`)
- `RenderQueue3D` and `submit_render_queue()`: worker threads build the draws, the main thread submits them in order
- `FrameStats::recording_threads` and `parallel_draws` as a live readout, next to the frame time

On OpenGL all three modes behave the same (the readout shows 1 recording thread), because a GL context belongs to one thread. Run with `--vulkan` to see the difference.

## Key code

Mode 0 turns the feature off; modes 1 and 2 leave it on:

```cpp
sl::set_parallel_recording(mode != 0);
```

Modes 0 and 1 issue ordinary draws; the backend decides how to record them:

```cpp
for (int i = 0; i < total; ++i)
{
    glm::mat4 model;
    sl::Material material;
    describe_cube(i, side, time, model, material);
    sl::draw_mesh(cube, model, material);
}
```

Mode 2 fills one `RenderQueue3D` per thread, then submits them in order on the main thread:

```cpp
queue.draw_mesh(cube, model, material);       // on a worker thread
// ...
for (const sl::RenderQueue3D &queue : queues)
    sl::submit_render_queue(queue);           // on the main thread
```

## Running
```bash
./build/exparallel --vulkan
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend.

## Controls
| Input | Action |
|-------|--------|
| Space | Switch recording mode |
| Up / Down | More / fewer cubes |
| Escape | Exit |

See the [tutorial lesson](../tutorial/3d-04-many-objects-and-threads.md) and [Parallel command recording](../renderers.md#parallel-command-recording).

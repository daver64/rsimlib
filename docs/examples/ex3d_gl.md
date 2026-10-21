# ex3d_gl

A minimal raw OpenGL 3D example: a spinning model drawn with a custom shader and the simlib `Camera`. OpenGL only.

![ex3d_gl screenshot](img/ex3d_gl.png)

## What it shows
- Using the OpenGL backend directly alongside simlib
- Compiling a custom 3D shader
- `Camera` and `model_matrix` transforms

## Key code

In `ex3d_gl.cpp`, the example requires the OpenGL backend and then compiles its own shader program through raw GL calls alongside simlib.

```cpp
if (!sl::set_graphics_backend(sl::GraphicsBackend::opengl))
{
    std::fprintf(stderr, "ex3d_gl requires the OpenGL backend; use --gl.\n");
    return -1;
}
// ...
const GLuint program = create_program();
```

```cpp
const GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, load_file("shaders/glsl/ex3d.vert"));
const GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, load_file("shaders/glsl/ex3d.frag"));
```

Each frame in `ex3d_gl.cpp`, `sl::model_matrix` and `sl::Camera` produce the matrices that are combined into the MVP uniform for the custom shader.

```cpp
const glm::mat4 model = sl::model_matrix({0.0f, 0.0f, 0.0f}, {time * 0.7f, time, time * 0.35f});
const glm::mat4 view = camera.view_matrix();
const glm::mat4 projection = camera.projection_matrix(
    static_cast<float>(sl::screen_width()) / std::max(1, sl::screen_height()));
glUseProgram(program);
glUniformMatrix4fv(glGetUniformLocation(program, "uMvp"), 1, GL_FALSE,
                   glm::value_ptr(projection * view * model));
```

## Running
```bash
./build/ex3d_gl
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Requires the OpenGL backend; run with `--gl`.

## Controls
| Input | Action |
|-------|--------|
| Escape | Exit |

## Key APIs
- `Camera`, `model_matrix` — [3D Surfaces](../api_reference.md#3d-surfaces-includesurface3dh) · [`surface3d.h`](../../include/surface3d.h)
- `set_graphics_backend`, `graphics_backend` — [Application Lifecycle](../api_reference.md#application-lifecycle--display-includedisplayh) · [`display.h`](../../include/display.h)

## Source
[src/examples/ex3d_gl/ex3d_gl.cpp](../../src/examples/ex3d_gl/ex3d_gl.cpp)

## See also
- [ex3d_surface](ex3d_surface.md)
- [ex3d_vulkan](ex3d_vulkan.md)

#include "sl.h"
#include "scene3d.h"

#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include <glm/gtc/type_ptr.hpp>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

namespace
{
    struct Vertex
    {
        float position[3];
        float colour[3];
    };

    std::string load_file(const char *path)
    {
        std::ifstream file(path, std::ios::binary);
        return file ? std::string(std::istreambuf_iterator<char>(file), {}) : std::string{};
    }

    GLuint compile_shader(GLenum type, const std::string &source)
    {
        const GLuint shader = glCreateShader(type);
        const char *text = source.c_str();
        glShaderSource(shader, 1, &text, nullptr);
        glCompileShader(shader);
        GLint status = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (status == GL_TRUE) return shader;
        glDeleteShader(shader);
        return 0;
    }

    GLuint create_program()
    {
        const GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, load_file("shaders/glsl/ex3d.vert"));
        const GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, load_file("shaders/glsl/ex3d.frag"));
        if (!vertex_shader || !fragment_shader)
        {
            if (vertex_shader) glDeleteShader(vertex_shader);
            if (fragment_shader) glDeleteShader(fragment_shader);
            return 0;
        }
        const GLuint program = glCreateProgram();
        glAttachShader(program, vertex_shader);
        glAttachShader(program, fragment_shader);
        glLinkProgram(program);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        GLint status = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &status);
        if (status == GL_TRUE) return program;
        glDeleteProgram(program);
        return 0;
    }

    void append_face(Vertex *vertices, int &offset, const float corners[8][3],
                     int a, int b, int c, int d, const float colour[3])
    {
        const int indices[] = {a, b, c, a, c, d};
        for (int index : indices)
        {
            for (int component = 0; component < 3; ++component)
            {
                vertices[offset].position[component] = corners[index][component];
                vertices[offset].colour[component] = colour[component];
            }
            ++offset;
        }
    }
}

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        sl::graphics_backend() != sl::GraphicsBackend::opengl)
    {
        std::fprintf(stderr, "ex3d currently requires the OpenGL backend; use --gl.\n");
        return -1;
    }
    if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600)) return -1;

    const GLuint program = create_program();
    if (!program)
    {
        std::fprintf(stderr, "Unable to compile the ex3d shaders.\n");
        sl::shutdown();
        return -1;
    }

    const float corners[8][3] = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}};
    const float colours[6][3] = {
        {0.95f, 0.25f, 0.25f}, {0.25f, 0.75f, 1.0f}, {0.35f, 0.95f, 0.45f},
        {1.0f, 0.75f, 0.2f}, {0.75f, 0.35f, 1.0f}, {0.2f, 0.9f, 0.85f}};
    Vertex vertices[36];
    int offset = 0;
    append_face(vertices, offset, corners, 0, 1, 2, 3, colours[0]);
    append_face(vertices, offset, corners, 5, 4, 7, 6, colours[1]);
    append_face(vertices, offset, corners, 4, 0, 3, 7, colours[2]);
    append_face(vertices, offset, corners, 1, 5, 6, 2, colours[3]);
    append_face(vertices, offset, corners, 3, 2, 6, 7, colours[4]);
    append_face(vertices, offset, corners, 4, 5, 1, 0, colours[5]);

    GLuint vao = 0;
    GLuint vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, colour)));
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    sl::Camera camera;
    bool running = true;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            sl::display_handle_event(event);
        }

        const float time = static_cast<float>(sl::time_ms()) * 0.001f;
        const glm::mat4 model = sl::model_matrix({0.0f, 0.0f, 0.0f}, {time * 0.7f, time, time * 0.35f});
        const glm::mat4 view = camera.view_matrix();
        const glm::mat4 projection = camera.projection_matrix(
            static_cast<float>(sl::screen_width()) / std::max(1, sl::screen_height()));
        glViewport(0, 0, sl::screen_width(), sl::screen_height());
        glClearColor(0.035f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(program);
        glUniformMatrix4fv(glGetUniformLocation(program, "uMvp"), 1, GL_FALSE,
            glm::value_ptr(projection * view * model));
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        sl::gprintf_center(32, {232, 236, 244}, "OpenGL 3D example - Escape to exit");
        sl::show_video_bitmap();
        sl::end_frame();
    }

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);
    sl::shutdown();
    return 0;
}
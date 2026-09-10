#define GL_GLEXT_PROTOTYPES

#include "renderer.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#include <cstdint>
#include <vector>
#include <cstddef>

namespace sl::detail
{
    namespace
    {
        std::string shader_log(GLuint shader)
        {
            GLint length = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
            if (length <= 1) return "Shader compilation failed.";
            std::vector<GLchar> log(static_cast<std::size_t>(length));
            glGetShaderInfoLog(shader, length, nullptr, log.data());
            return log.data();
        }

        std::string program_log(GLuint program)
        {
            GLint length = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
            if (length <= 1) return "Shader linking failed.";
            std::vector<GLchar> log(static_cast<std::size_t>(length));
            glGetProgramInfoLog(program, length, nullptr, log.data());
            return log.data();
        }

        GLuint compile_shader(GLenum type, const std::string &source, std::string &error)
        {
            const GLuint shader = glCreateShader(type);
            if (shader == 0)
            {
                error = "Unable to create shader.";
                return 0;
            }
            const char *source_text = source.c_str();
            glShaderSource(shader, 1, &source_text, nullptr);
            glCompileShader(shader);
            GLint compiled = GL_FALSE;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
            if (compiled == GL_TRUE) return shader;
            error = shader_log(shader);
            glDeleteShader(shader);
            return 0;
        }

        GLuint link_program(GLuint first, GLuint second, std::string &error)
        {
            const GLuint program = glCreateProgram();
            if (program == 0)
            {
                error = "Unable to create shader program.";
                return 0;
            }
            glAttachShader(program, first);
            if (second != 0) glAttachShader(program, second);
            glLinkProgram(program);
            GLint linked = GL_FALSE;
            glGetProgramiv(program, GL_LINK_STATUS, &linked);
            if (linked != GL_TRUE)
            {
                error = program_log(program);
                glDeleteProgram(program);
                return 0;
            }
            return program;
        }

        class OpenGLRenderer final : public Renderer
        {
        public:
            void configure_window() override
            {
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
            }

            bool initialise(SDL_Window *window, std::string &error) override
            {
                window_ = window;
                context_ = SDL_GL_CreateContext(window);
                if (!context_)
                {
                    error = SDL_GetError();
                    window_ = nullptr;
                    return false;
                }
                return true;
            }

            void shutdown() override
            {
                shutdown_2d();
                if (context_)
                {
                    SDL_GL_DeleteContext(context_);
                    context_ = nullptr;
                }
                window_ = nullptr;
            }

            void resize(int width, int height) override
            {
                glViewport(0, 0, width, height);
            }

            bool set_vsync(bool enabled) override
            {
                return context_ && SDL_GL_SetSwapInterval(enabled ? 1 : 0) == 0;
            }

            void present() override
            {
                if (window_)
                {
                    SDL_GL_SwapWindow(window_);
                }
            }

            SDL_GLContext native_context() const override
            {
                return context_;
            }

            bool create_texture(int width, int height, bool linear, std::uint32_t &texture) override
            {
                GLuint handle = 0;
                glGenTextures(1, &handle);
                if (handle == 0)
                {
                    return false;
                }
                glBindTexture(GL_TEXTURE_2D, handle);
                const GLint filter = linear ? GL_LINEAR : GL_NEAREST;
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                texture = handle;
                return true;
            }

            bool upload_texture(std::uint32_t texture, int width, int height,
                                const std::uint8_t *pixels) override
            {
                if (texture == 0 || !pixels)
                {
                    return false;
                }
                glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                                GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                return true;
            }

            void destroy_texture(std::uint32_t texture) override
            {
                if (texture != 0)
                {
                    const GLuint handle = static_cast<GLuint>(texture);
                    glDeleteTextures(1, &handle);
                }
            }

            bool create_render_target(int width, int height, std::uint32_t &texture,
                                      std::uint32_t &framebuffer) override
            {
                texture = 0;
                framebuffer = 0;
                if (!create_texture(width, height, true, texture))
                {
                    return false;
                }
                GLuint fbo = 0;
                glGenFramebuffers(1, &fbo);
                glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                       static_cast<GLuint>(texture), 0);
                const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                if (!complete)
                {
                    destroy_texture(texture);
                    texture = 0;
                    if (fbo != 0)
                    {
                        glDeleteFramebuffers(1, &fbo);
                    }
                    return false;
                }
                framebuffer = fbo;
                return true;
            }

            void destroy_render_target(std::uint32_t texture, std::uint32_t framebuffer) override
            {
                if (framebuffer != 0)
                {
                    const GLuint handle = static_cast<GLuint>(framebuffer);
                    glDeleteFramebuffers(1, &handle);
                }
                destroy_texture(texture);
            }

            bool initialise_2d() override
            {
                if (vao_ != 0) return true;
                static const std::string vertex = R"(
#version 430 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;
uniform mat4 uProjection;
out vec2 vTexCoord;
out vec4 vColor;
void main() { gl_Position = uProjection * vec4(aPos, 0.0, 1.0); vTexCoord = aTexCoord; vColor = aColor; }
)";
                static const std::string fragment = R"(
#version 430 core
in vec2 vTexCoord;
in vec4 vColor;
uniform sampler2D uTexture;
out vec4 fragColor;
void main() { fragColor = texture(uTexture, vTexCoord) * vColor; }
)";
                if (!create_shader(vertex, fragment, default_2d_shader_, shader_error_)) return false;
                glGenVertexArrays(1, &vao_);
                glGenBuffers(1, &vbo_);
                glBindVertexArray(vao_);
                glBindBuffer(GL_ARRAY_BUFFER, vbo_);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLVertex), reinterpret_cast<void *>(offsetof(GLVertex, x)));
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLVertex), reinterpret_cast<void *>(offsetof(GLVertex, u)));
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GLVertex), reinterpret_cast<void *>(offsetof(GLVertex, r)));
                glBindVertexArray(0);
                const unsigned char white_pixel[4] = {255, 255, 255, 255};
                if (!create_texture(1, 1, false, white_texture_) ||
                    !upload_texture(white_texture_, 1, 1, white_pixel)) return false;
                return true;
            }

            void shutdown_2d() override
            {
                if (vbo_ != 0) glDeleteBuffers(1, &vbo_);
                if (vao_ != 0) glDeleteVertexArrays(1, &vao_);
                vbo_ = 0; vao_ = 0;
                destroy_shader(default_2d_shader_);
                default_2d_shader_ = 0;
                destroy_texture(white_texture_);
                white_texture_ = 0;
            }

            bool begin_2d(int width, int height) override
            {
                if (!initialise_2d()) return false;
                float projection[16] = {};
                projection[0] = width > 0 ? 2.0f / width : 0.0f;
                projection[5] = height > 0 ? -2.0f / height : 0.0f;
                projection[10] = -1.0f; projection[12] = -1.0f; projection[13] = 1.0f; projection[15] = 1.0f;
                set_shader_mat4(default_2d_shader_, "uProjection", projection);
                set_shader_int(default_2d_shader_, "uTexture", 0);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glActiveTexture(GL_TEXTURE0);
                return true;
            }

            void submit_2d(std::uint32_t primitive_mode, const GLVertex *vertices, int count, std::uint32_t texture) override
            {
                if (!vertices || count <= 0 || !initialise_2d()) return;
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture != 0 ? texture : white_texture_));
                glBindVertexArray(vao_);
                glBindBuffer(GL_ARRAY_BUFFER, vbo_);
                glBufferData(GL_ARRAY_BUFFER, sizeof(GLVertex) * count, vertices, GL_DYNAMIC_DRAW);
                glDrawArrays(static_cast<GLenum>(primitive_mode), 0, count);
                glBindVertexArray(0);
            }

            bool create_shader(const std::string &vertex_source, const std::string &fragment_source,
                               std::uint32_t &program, std::string &error) override
            {
                program = 0;
                const GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_source, error);
                if (vertex == 0) return false;
                const GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_source, error);
                if (fragment == 0)
                {
                    glDeleteShader(vertex);
                    return false;
                }
                const GLuint linked = link_program(vertex, fragment, error);
                glDeleteShader(vertex);
                glDeleteShader(fragment);
                program = linked;
                return program != 0;
            }

            bool create_compute_shader(const std::string &source, std::uint32_t &program,
                                       std::string &error) override
            {
                program = 0;
                const GLuint compute = compile_shader(GL_COMPUTE_SHADER, source, error);
                if (compute == 0) return false;
                const GLuint linked = link_program(compute, 0, error);
                glDeleteShader(compute);
                program = linked;
                return program != 0;
            }

            void destroy_shader(std::uint32_t program) override
            {
                if (program != 0) glDeleteProgram(static_cast<GLuint>(program));
            }

            bool use_shader(std::uint32_t program) override
            {
                if (program == 0) return false;
                glUseProgram(static_cast<GLuint>(program));
                return true;
            }

            void stop_shader() override { glUseProgram(0); }

            bool dispatch_compute(std::uint32_t program, unsigned int groups_x,
                                  unsigned int groups_y, unsigned int groups_z) override
            {
                if (!use_shader(program)) return false;
                glDispatchCompute(groups_x, groups_y, groups_z);
                return true;
            }

            bool set_shader_int(std::uint32_t program, const char *name, int value) override
            {
                return set_location(program, name, [value](GLint location) { glUniform1i(location, value); });
            }

            bool set_shader_float(std::uint32_t program, const char *name, float value) override
            {
                return set_location(program, name, [value](GLint location) { glUniform1f(location, value); });
            }

            bool set_shader_float2(std::uint32_t program, const char *name, float x, float y) override
            {
                return set_location(program, name, [x, y](GLint location) { glUniform2f(location, x, y); });
            }

            bool set_shader_int2(std::uint32_t program, const char *name, int x, int y) override
            {
                return set_location(program, name, [x, y](GLint location) { glUniform2i(location, x, y); });
            }

            bool set_shader_float3(std::uint32_t program, const char *name, float x, float y, float z) override
            {
                return set_location(program, name, [x, y, z](GLint location) { glUniform3f(location, x, y, z); });
            }

            bool set_shader_mat4(std::uint32_t program, const char *name, const float *matrix) override
            {
                return matrix && set_location(program, name,
                    [matrix](GLint location) { glUniformMatrix4fv(location, 1, GL_FALSE, matrix); });
            }

        private:
            template <typename Setter>
            bool set_location(std::uint32_t program, const char *name, Setter setter)
            {
                if (!use_shader(program)) return false;
                const GLint location = glGetUniformLocation(static_cast<GLuint>(program), name);
                if (location < 0) return false;
                setter(location);
                return true;
            }

            SDL_Window *window_ = nullptr;
            SDL_GLContext context_ = nullptr;
            GLuint vao_ = 0;
            GLuint vbo_ = 0;
            std::uint32_t default_2d_shader_ = 0;
            std::uint32_t white_texture_ = 0;
            std::string shader_error_;
        };
    }

    std::unique_ptr<Renderer> create_renderer()
    {
        return std::make_unique<OpenGLRenderer>();
    }
}

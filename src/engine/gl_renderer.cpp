/** @file
 * @brief Implements the OpenGL 4.3 Renderer backend.
 */
#define GL_GLEXT_PROTOTYPES

#include "gl_renderer.h"
#include "gl_context.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#include <cstdint>
#include <vector>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <unordered_map>

namespace sl::detail
{
    namespace
    {
        std::string load_glsl_shader(const char *name)
        {
            std::ifstream file(std::filesystem::path(SIMLIB_GLSL_SHADER_DIR) / name);
            return file ? std::string(std::istreambuf_iterator<char>(file), {}) : std::string{};
        }

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

        class GLRenderer final : public Renderer
        {
        public:
            void configure_window() override
            {
                GLContext::configure_window_attributes();
            }
            std::uint32_t window_flags() const override { return SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE; }

            bool initialise(SDL_Window *window, std::string &error) override
            {
                return context_.initialise(window, error);
            }

            void shutdown() override
            {
                shutdown_2d();
                texture_sizes_.clear();
                render_target_stack_.clear();
                context_.shutdown();
            }

            bool resize(int width, int height, std::string &error) override
            {
                return context_.resize(width, height, error);
            }

            bool set_vsync(bool enabled) override
            {
                return context_.set_vsync(enabled);
            }

            bool vsync_active() const override
            {
                return context_.vsync_active();
            }

            void present() override
            {
                context_.present();
            }

            void wait_idle() override { glFinish(); }

            bool begin_frame(std::string &) override { return true; }
            bool end_frame(std::string &) override { present(); return true; }

            SDL_GLContext native_context() const override
            {
                return context_.native_context();
            }

            bool create_texture(const TextureDesc &description, std::uint32_t &texture) override
            {
                texture = 0;
                if (description.width <= 0 || description.height <= 0)
                {
                    return false;
                }
                GLuint handle = 0;
                glGenTextures(1, &handle);
                if (handle == 0)
                {
                    return false;
                }
                glBindTexture(GL_TEXTURE_2D, handle);
                const GLint filter = description.filter == TextureFilter::linear ? GL_LINEAR : GL_NEAREST;
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, description.width, description.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                if (glGetError() != GL_NO_ERROR)
                {
                    glDeleteTextures(1, &handle);
                    return false;
                }
                texture_sizes_[handle] = {description.width, description.height};
                texture = handle;
                return true;
            }

            bool upload_texture(std::uint32_t texture, int width, int height,
                                const std::uint8_t *pixels) override
            {
                if (texture == 0 || !pixels || width <= 0 || height <= 0)
                {
                    return false;
                }
                const GLuint handle = static_cast<GLuint>(texture);
                const auto size = texture_sizes_.find(handle);
                if (size == texture_sizes_.end() || width > size->second.first || height > size->second.second)
                {
                    return false;
                }
                glBindTexture(GL_TEXTURE_2D, handle);
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
                    texture_sizes_.erase(handle);
                }
            }

            bool create_render_target(int width, int height, std::uint32_t &texture,
                                      std::uint32_t &framebuffer) override
            {
                texture = 0;
                framebuffer = 0;
                GLint previous_framebuffer = 0;
                GLint previous_viewport[4] = {};
                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_framebuffer);
                glGetIntegerv(GL_VIEWPORT, previous_viewport);
                const auto restore_state = [&]()
                {
                    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_framebuffer));
                    glViewport(previous_viewport[0], previous_viewport[1],
                        previous_viewport[2], previous_viewport[3]);
                };
                if (width <= 0 || height <= 0)
                {
                    return false;
                }
                if (!create_texture({width, height, TextureFilter::linear}, texture))
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
                    restore_state();
                    return false;
                }
                restore_state();
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
            bool begin_render_target(std::uint32_t framebuffer, int width, int height, std::string &) override
            {
                if (framebuffer == 0 || width <= 0 || height <= 0)
                {
                    return false;
                }
                RenderTargetState previous{};
                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous.framebuffer);
                glGetIntegerv(GL_VIEWPORT, previous.viewport);
                render_target_stack_.push_back(previous);
                glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebuffer));
                glViewport(0, 0, width, height);
                return true;
            }
            bool end_render_target(std::string &) override
            {
                if (render_target_stack_.empty())
                {
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    return true;
                }
                const RenderTargetState previous = render_target_stack_.back();
                render_target_stack_.pop_back();
                glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous.framebuffer));
                glViewport(previous.viewport[0], previous.viewport[1], previous.viewport[2], previous.viewport[3]);
                return true;
            }

            bool initialise_2d() override
            {
                if (vao_ != 0) return true;
                const std::string vertex = load_glsl_shader("default_2d.vert");
                const std::string fragment = load_glsl_shader("default_2d.frag");
                if (vertex.empty() || fragment.empty())
                {
                    shader_error_ = "Unable to load default OpenGL shader assets.";
                    return false;
                }
                if (!create_shader({ShaderLanguage::glsl, vertex}, {ShaderLanguage::glsl, fragment},
                                   default_2d_shader_, shader_error_)) return false;
                glGenVertexArrays(1, &vao_);
                glGenBuffers(1, &vbo_);
                if (vao_ == 0 || vbo_ == 0)
                {
                    shutdown_2d();
                    shader_error_ = "Unable to create OpenGL 2D vertex resources.";
                    return false;
                }
                glBindVertexArray(vao_);
                glBindBuffer(GL_ARRAY_BUFFER, vbo_);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), reinterpret_cast<void *>(offsetof(Vertex2D, x)));
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), reinterpret_cast<void *>(offsetof(Vertex2D, u)));
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), reinterpret_cast<void *>(offsetof(Vertex2D, r)));
                glBindVertexArray(0);
                const unsigned char white_pixel[4] = {255, 255, 255, 255};
                if (!create_texture({1, 1, TextureFilter::nearest}, white_texture_) ||
                    !upload_texture(white_texture_, 1, 1, white_pixel))
                {
                    shutdown_2d();
                    shader_error_ = "Unable to create the OpenGL default texture.";
                    return false;
                }
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
                projection[10] = -1.0f;
                projection[12] = -1.0f + 2.0f * detail::screen_offset_x() / width;
                projection[13] = 1.0f - 2.0f * detail::screen_offset_y() / height;
                projection[15] = 1.0f;
                set_shader_mat4(default_2d_shader_, "uProjection", projection);
                set_shader_int(default_2d_shader_, "uTexture", 0);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glActiveTexture(GL_TEXTURE0);
                return true;
            }
            bool begin_shader_2d(std::uint32_t program, int width, int height) override
            {
                if (!initialise_2d() || !use_shader(program) || width <= 0 || height <= 0) return false;
                float projection[16] = {};
                projection[0] = 2.0f / width;
                projection[5] = -2.0f / height;
                projection[10] = -1.0f;
                projection[12] = -1.0f + 2.0f * detail::screen_offset_x() / width;
                projection[13] = 1.0f - 2.0f * detail::screen_offset_y() / height;
                projection[15] = 1.0f;
                if (!set_shader_mat4(program, "uProjection", projection) ||
                    !set_shader_int(program, "uTexture", 0)) return false;
                glEnable(GL_BLEND);
                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                glActiveTexture(GL_TEXTURE0);
                return true;
            }
            void set_premultiplied_alpha(bool enabled) override
            {
                glBlendFunc(enabled ? GL_ONE : GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            bool clear_frame(float red, float green, float blue, float alpha) override
            {
                context_.clear(red, green, blue, alpha);
                return true;
            }

            void submit_2d(PrimitiveType primitive_mode, const Vertex2D *vertices, int count, std::uint32_t texture) override
            {
                if (!vertices || count <= 0 || !initialise_2d()) return;
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture != 0 ? texture : white_texture_));
                glBindVertexArray(vao_);
                glBindBuffer(GL_ARRAY_BUFFER, vbo_);
                glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2D) * count, vertices, GL_DYNAMIC_DRAW);
                GLenum mode = GL_TRIANGLE_FAN;
                switch (primitive_mode)
                {
                case PrimitiveType::points: mode = GL_POINTS; break;
                case PrimitiveType::lines: mode = GL_LINES; break;
                case PrimitiveType::line_loop: mode = GL_LINE_LOOP; break;
                case PrimitiveType::triangles: mode = GL_TRIANGLES; break;
                case PrimitiveType::triangle_fan: mode = GL_TRIANGLE_FAN; break;
                }
                glDrawArrays(mode, 0, count);
                glBindVertexArray(0);
            }

            bool create_storage_buffer(std::size_t size, std::uint32_t &buffer) override
            {
                GLuint handle = 0; glGenBuffers(1, &handle); buffer = handle;
                if (handle != 0) { glBindBuffer(GL_SHADER_STORAGE_BUFFER, handle); glBufferData(GL_SHADER_STORAGE_BUFFER, size, nullptr, GL_DYNAMIC_DRAW); }
                return handle != 0;
            }
            void destroy_storage_buffer(std::uint32_t buffer) override
            {
                if (buffer != 0) { const GLuint handle = static_cast<GLuint>(buffer); glDeleteBuffers(1, &handle); }
            }
            bool upload_storage_buffer(std::uint32_t buffer, std::size_t size, const void *data, bool preserve_storage) override
            {
                if (buffer == 0) return false;
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(buffer));
                if (!preserve_storage || data) glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(size), data, GL_DYNAMIC_DRAW);
                return true;
            }
            void bind_storage_buffer(unsigned int binding, std::uint32_t buffer) override
            {
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, static_cast<GLuint>(buffer));
            }
            void bind_texture_unit(unsigned int unit, std::uint32_t texture) override
            {
                glActiveTexture(GL_TEXTURE0 + unit); glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
            }
            void storage_barrier() override { glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); }

            bool create_shader(const ShaderSource &vertex_source, const ShaderSource &fragment_source,
                               std::uint32_t &program, std::string &error) override
            {
                program = 0;
                if (vertex_source.language != ShaderLanguage::glsl || fragment_source.language != ShaderLanguage::glsl)
                {
                    error = "OpenGL backend accepts GLSL shader sources only.";
                    return false;
                }
                const GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_source.text, error);
                if (vertex == 0) return false;
                const GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_source.text, error);
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

            bool create_compute_shader(const ShaderSource &source, std::uint32_t &program,
                                       std::string &error) override
            {
                program = 0;
                if (source.language != ShaderLanguage::glsl)
                {
                    error = "OpenGL backend accepts GLSL shader sources only.";
                    return false;
                }
                const GLuint compute = compile_shader(GL_COMPUTE_SHADER, source.text, error);
                if (compute == 0) return false;
                const GLuint linked = link_program(compute, 0, error);
                glDeleteShader(compute);
                program = linked;
                return program != 0;
            }
            bool create_shader_module(ShaderStage, const ShaderSource &, std::uint32_t &, std::string &error) override
            {
                error = "OpenGL renderer uses linked GLSL programs rather than standalone SPIR-V modules.";
                return false;
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
                        struct RenderTargetState
                        {
                            GLint framebuffer = 0;
                            GLint viewport[4] = {};
                        };

            template <typename Setter>
            bool set_location(std::uint32_t program, const char *name, Setter setter)
            {
                if (!use_shader(program)) return false;
                const GLint location = glGetUniformLocation(static_cast<GLuint>(program), name);
                if (location < 0) return false;
                setter(location);
                return true;
            }

            GLContext context_;
            GLuint vao_ = 0;
            GLuint vbo_ = 0;
            std::uint32_t default_2d_shader_ = 0;
            std::uint32_t white_texture_ = 0;
            std::string shader_error_;
            std::unordered_map<GLuint, std::pair<int, int>> texture_sizes_;
            std::vector<RenderTargetState> render_target_stack_;
        };
    }

    std::unique_ptr<Renderer> create_gl_renderer()
    {
        return std::make_unique<GLRenderer>();
    }
}

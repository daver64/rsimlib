#define GL_GLEXT_PROTOTYPES

#include "renderer.h"
#include "vulkan_context.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>
#include <cstddef>
#include <unordered_map>
#include <filesystem>
#include <array>

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
            std::uint32_t window_flags() const override { return SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE; }

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

            bool begin_frame(std::string &) override { return true; }
            bool end_frame(std::string &) override { present(); return true; }

            SDL_GLContext native_context() const override
            {
                return context_;
            }

            bool create_texture(const TextureDesc &description, std::uint32_t &texture) override
            {
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
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, description.width, description.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
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
            bool begin_render_target(std::uint32_t framebuffer, int, int, std::string &) override
            {
                glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebuffer));
                return true;
            }
            bool end_render_target(std::string &) override
            {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                return true;
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
                if (!create_shader({ShaderLanguage::glsl, vertex}, {ShaderLanguage::glsl, fragment},
                                   default_2d_shader_, shader_error_)) return false;
                glGenVertexArrays(1, &vao_);
                glGenBuffers(1, &vbo_);
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
            bool clear_frame(float red, float green, float blue, float alpha) override
            {
                glClearColor(red, green, blue, alpha);
                glClear(GL_COLOR_BUFFER_BIT);
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

        class VulkanRenderer final : public Renderer
        {
        public:
            void configure_window() override
            {
                window_flags_ = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
            }
            std::uint32_t window_flags() const override { return window_flags_; }
            bool initialise(SDL_Window *window, std::string &error) override
            {
                window_ = window;
                if (!context_.initialise(window, error)) return false;
                if (!context_.create_texture_descriptor_layout(descriptor_layout_, error) ||
                    !context_.create_storage_descriptor_layout(storage_layout_, error) ||
                    !context_.create_descriptor_pool(descriptor_pool_, error)) return false;
                const std::filesystem::path shader_dir = SIMLIB_SHADER_BINARY_DIR;
                if (!context_.load_shader_module(shader_dir / "vulkan_2d.vert.spv", vertex_module_, error) ||
                    !context_.load_shader_module(shader_dir / "vulkan_2d.frag.spv", fragment_module_, error) ||
                    !create_pipelines(error)) return false;
                if (!context_.load_shader_module(shader_dir / "vulkan_light_cull.comp.spv", cull_module_, error) ||
                    !context_.create_compute_pipeline(cull_module_, storage_layout_, sizeof(int) * 5,
                                                      cull_pipeline_, error)) return false;
                const unsigned char white_pixel[4] = {255, 255, 255, 255};
                if (!create_texture({1, 1, TextureFilter::nearest}, white_texture_) ||
                    !upload_texture(white_texture_, 1, 1, white_pixel))
                {
                    error = last_error_;
                    return false;
                }
                return true;
            }
            void shutdown() override
            {
                for (VulkanBuffer &buffer : vertex_buffers_) context_.destroy_buffer(buffer);
                vertex_buffers_.clear();
                for (auto &[handle, texture] : textures_)
                {
                    context_.destroy_sampler(texture.sampler);
                    context_.destroy_image(texture.image);
                }
                textures_.clear();
                for (auto &[handle, buffer] : storage_buffers_)
                    context_.destroy_buffer(buffer.buffer);
                storage_buffers_.clear();
                for (auto &[handle, target] : render_targets_)
                {
                    context_.destroy_sampler(target.sampler);
                    context_.destroy_framebuffer(target.framebuffer);
                    context_.destroy_image(target.image);
                }
                render_targets_.clear();
                for (VulkanGraphicsPipeline &pipeline : pipelines_)
                    context_.destroy_graphics_pipeline(pipeline);
                    context_.destroy_compute_pipeline(cull_pipeline_);
                context_.destroy_shader_module(vertex_module_);
                context_.destroy_shader_module(fragment_module_);
                    context_.destroy_shader_module(cull_module_);
                context_.destroy_descriptor_pool(descriptor_pool_);
                context_.destroy_descriptor_set_layout(descriptor_layout_);
                    context_.destroy_storage_descriptor_layout(storage_layout_);
                context_.shutdown();
                window_ = nullptr;
            }
            void resize(int width, int height) override
            {
                for (auto &[handle, target] : render_targets_)
                {
                    context_.destroy_sampler(target.sampler);
                    context_.destroy_framebuffer(target.framebuffer);
                    context_.destroy_image(target.image);
                }
                render_targets_.clear();
                for (VulkanGraphicsPipeline &pipeline : pipelines_)
                    context_.destroy_graphics_pipeline(pipeline);
                if (context_.recreate_swapchain(width, height, last_error_))
                    create_pipelines(last_error_);
            }
            bool set_vsync(bool) override { return true; }
            void present() override
            {
                if (frame_active_)
                {
                    context_.end_frame(last_error_);
                    frame_active_ = false;
                }
            }
            bool begin_frame(std::string &error) override
            {
                if (frame_active_) return true;
                frame_active_ = context_.begin_frame(error);
                return frame_active_;
            }
            bool end_frame(std::string &error) override
            {
                if (!frame_active_) return true;
                frame_active_ = false;
                return context_.end_frame(error);
            }
            SDL_GLContext native_context() const override { return nullptr; }

            bool create_texture(const TextureDesc &description, std::uint32_t &texture) override
            {
                VulkanTexture resource;
                if (!context_.create_image(description.width, description.height, VK_FORMAT_R8G8B8A8_UNORM,
                                           VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                           resource.image, last_error_) ||
                    !context_.create_sampler(resource.sampler, last_error_))
                {
                    context_.destroy_sampler(resource.sampler);
                    context_.destroy_image(resource.image);
                    return false;
                }
                texture = next_texture_++;
                if (!context_.allocate_texture_descriptor(descriptor_pool_, descriptor_layout_, resource.image,
                                                          resource.sampler, resource.descriptor, last_error_))
                {
                    context_.destroy_sampler(resource.sampler);
                    context_.destroy_image(resource.image);
                    return false;
                }
                textures_.emplace(texture, std::move(resource));
                return true;
            }
            bool upload_texture(std::uint32_t texture, int width, int height, const std::uint8_t *pixels) override
            {
                const auto iterator = textures_.find(texture);
                return iterator != textures_.end() &&
                    context_.upload_image_rgba(iterator->second.image, width, height, pixels, last_error_);
            }
            void destroy_texture(std::uint32_t texture) override
            {
                const auto iterator = textures_.find(texture);
                if (iterator == textures_.end()) return;
                context_.destroy_sampler(iterator->second.sampler);
                context_.destroy_image(iterator->second.image);
                textures_.erase(iterator);
            }
            bool create_render_target(int width, int height, std::uint32_t &texture, std::uint32_t &framebuffer) override
            {
                VulkanRenderTarget resource;
                if (!context_.create_image(width, height, VK_FORMAT_R8G8B8A8_UNORM,
                                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                           resource.image, last_error_) ||
                    !context_.create_sampler(resource.sampler, last_error_) ||
                    !context_.create_render_target_framebuffer(resource.image, width, height,
                                                               resource.framebuffer, last_error_))
                {
                    context_.destroy_framebuffer(resource.framebuffer);
                    context_.destroy_sampler(resource.sampler);
                    context_.destroy_image(resource.image);
                    return false;
                }
                texture = next_texture_++;
                framebuffer = next_render_target_++;
                resource.texture_handle = texture;
                if (!context_.allocate_texture_descriptor(descriptor_pool_, descriptor_layout_, resource.image,
                                                          resource.sampler, resource.descriptor, last_error_))
                {
                    context_.destroy_framebuffer(resource.framebuffer);
                    context_.destroy_sampler(resource.sampler);
                    context_.destroy_image(resource.image);
                    return false;
                }
                render_targets_.emplace(framebuffer, std::move(resource));
                return true;
            }
            void destroy_render_target(std::uint32_t texture, std::uint32_t framebuffer) override
            {
                const auto iterator = render_targets_.find(framebuffer);
                if (iterator == render_targets_.end()) return;
                context_.destroy_sampler(iterator->second.sampler);
                context_.destroy_framebuffer(iterator->second.framebuffer);
                context_.destroy_image(iterator->second.image);
                render_targets_.erase(iterator);
            }
            bool begin_render_target(std::uint32_t framebuffer, int width, int height, std::string &error) override
            {
                const auto iterator = render_targets_.find(framebuffer);
                return iterator != render_targets_.end() &&
                    context_.begin_offscreen_render_pass(iterator->second.framebuffer, width, height, error);
            }
            bool end_render_target(std::string &error) override
            { return context_.end_offscreen_render_pass(error); }
            bool create_shader(const ShaderSource &, const ShaderSource &, std::uint32_t &, std::string &error) override
            { error = "Vulkan renderer shader resources are not connected yet."; return false; }
            bool create_compute_shader(const ShaderSource &, std::uint32_t &, std::string &error) override
            { error = "Vulkan renderer compute resources are not connected yet."; return false; }
            void destroy_shader(std::uint32_t) override {}
            bool use_shader(std::uint32_t) override { return unsupported(); }
            void stop_shader() override {}
            bool dispatch_compute(std::uint32_t, unsigned int, unsigned int, unsigned int) override { return unsupported(); }
            bool set_shader_int(std::uint32_t, const char *, int) override { return unsupported(); }
            bool set_shader_float(std::uint32_t, const char *, float) override { return unsupported(); }
            bool set_shader_float2(std::uint32_t, const char *, float, float) override { return unsupported(); }
            bool set_shader_int2(std::uint32_t, const char *, int, int) override { return unsupported(); }
            bool set_shader_float3(std::uint32_t, const char *, float, float, float) override { return unsupported(); }
            bool set_shader_mat4(std::uint32_t, const char *, const float *) override { return unsupported(); }
            bool initialise_2d() override { return pipelines_[4].pipeline != VK_NULL_HANDLE; }
            void shutdown_2d() override {}
            bool begin_2d(int width, int height) override
            {
                if (!begin_frame(last_error_)) return false;
                projection_.fill(0.0f);
                projection_[0] = width > 0 ? 2.0f / width : 0.0f;
                projection_[5] = height > 0 ? -2.0f / height : 0.0f;
                projection_[10] = -1.0f; projection_[12] = -1.0f; projection_[13] = 1.0f; projection_[15] = 1.0f;
                return true;
            }
            bool clear_frame(float red, float green, float blue, float alpha) override
            {
                if (!frame_active_ && !begin_frame(last_error_)) return false;
                return context_.clear_active_frame(red, green, blue, alpha, last_error_);
            }
            void submit_2d(PrimitiveType primitive, const Vertex2D *vertices, int count, std::uint32_t texture) override
            {
                if (!vertices || count <= 0 || !frame_active_) return;
                std::vector<Vertex2D> line_loop_vertices;
                const Vertex2D *upload_vertices = vertices;
                int upload_count = count;
                if (primitive == PrimitiveType::line_loop && count > 1)
                {
                    line_loop_vertices.assign(vertices, vertices + count);
                    line_loop_vertices.push_back(vertices[0]);
                    upload_vertices = line_loop_vertices.data();
                    upload_count = count + 1;
                }
                VulkanBuffer buffer;
                if (!context_.create_buffer(sizeof(Vertex2D) * upload_count, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buffer, last_error_) ||
                    !context_.upload_buffer(buffer, upload_vertices, sizeof(Vertex2D) * upload_count, last_error_))
                {
                    context_.destroy_buffer(buffer); return;
                }
                vertex_buffers_.push_back(buffer);
                const std::uint32_t texture_handle = texture == 0 ? white_texture_ : texture;
                VkDescriptorSet descriptor = VK_NULL_HANDLE;
                const auto texture_iterator = textures_.find(texture_handle);
                if (texture_iterator != textures_.end())
                    descriptor = texture_iterator->second.descriptor;
                else
                {
                    for (const auto &[handle, target] : render_targets_)
                        if (target.texture_handle == texture_handle) descriptor = target.descriptor;
                }
                const std::size_t pipeline_index = static_cast<std::size_t>(primitive);
                if (pipeline_index >= pipelines_.size())
                {
                    context_.destroy_buffer(buffer);
                    return;
                }
                context_.record_vertex_draw(pipelines_[pipeline_index].pipeline, pipelines_[pipeline_index].layout, buffer.buffer, descriptor,
                    static_cast<std::uint32_t>(upload_count), primitive, projection_.data(), last_error_);
            }
            bool create_storage_buffer(std::size_t size, std::uint32_t &buffer) override
            {
                VulkanStorageBuffer resource;
                if (!context_.create_buffer(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    resource.buffer, last_error_)) return false;
                buffer = next_storage_buffer_++;
                storage_buffers_.emplace(buffer, std::move(resource));
                for (std::uint32_t &slot : storage_handles_)
                {
                    if (slot == 0) { slot = buffer; break; }
                }
                if (storage_handles_[0] != 0 && storage_handles_[1] != 0 && storage_handles_[2] != 0)
                {
                    VulkanBuffer buffers[3] = {
                        storage_buffers_[storage_handles_[0]].buffer,
                        storage_buffers_[storage_handles_[1]].buffer,
                        storage_buffers_[storage_handles_[2]].buffer};
                    context_.allocate_storage_descriptor(descriptor_pool_, storage_layout_, buffers, 3,
                                                          storage_descriptor_, last_error_);
                }
                return true;
            }
            void destroy_storage_buffer(std::uint32_t buffer) override
            {
                const auto iterator = storage_buffers_.find(buffer);
                if (iterator == storage_buffers_.end()) return;
                context_.destroy_buffer(iterator->second.buffer);
                storage_buffers_.erase(iterator);
            }
            bool upload_storage_buffer(std::uint32_t buffer, std::size_t size, const void *data, bool) override
            {
                auto iterator = storage_buffers_.find(buffer);
                if (iterator == storage_buffers_.end()) return false;
                if (size > iterator->second.buffer.size)
                {
                    context_.destroy_buffer(iterator->second.buffer);
                    if (!context_.create_buffer(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        iterator->second.buffer, last_error_)) return false;
                }
                return data ? context_.upload_buffer(iterator->second.buffer, data, size, last_error_) : true;
            }
            void bind_storage_buffer(unsigned int, std::uint32_t) override {}
            void bind_texture_unit(unsigned int, std::uint32_t) override {}
            void storage_barrier() override {}

        private:
            bool create_pipelines(std::string &error)
            {
                return context_.create_graphics_pipeline(vertex_module_, fragment_module_, descriptor_layout_, PrimitiveType::points, pipelines_[0], error) &&
                    context_.create_graphics_pipeline(vertex_module_, fragment_module_, descriptor_layout_, PrimitiveType::lines, pipelines_[1], error) &&
                    context_.create_graphics_pipeline(vertex_module_, fragment_module_, descriptor_layout_, PrimitiveType::line_loop, pipelines_[2], error) &&
                    context_.create_graphics_pipeline(vertex_module_, fragment_module_, descriptor_layout_, PrimitiveType::triangles, pipelines_[3], error) &&
                    context_.create_graphics_pipeline(vertex_module_, fragment_module_, descriptor_layout_, PrimitiveType::triangle_fan, pipelines_[4], error);
            }

            bool unsupported()
            {
                last_error_ = "Vulkan renderer resources are not implemented yet.";
                return false;
            }

            SDL_Window *window_ = nullptr;
            unsigned int window_flags_ = 0;
            VulkanContext context_;
            std::string last_error_;
            VulkanDescriptorSetLayout descriptor_layout_;
            VulkanDescriptorPool descriptor_pool_;
            VulkanShaderModule vertex_module_;
            VulkanShaderModule fragment_module_;
            VulkanShaderModule cull_module_;
            std::array<VulkanGraphicsPipeline, 5> pipelines_{};
            VulkanComputePipeline cull_pipeline_;
            VulkanStorageDescriptorLayout storage_layout_;
            std::vector<VulkanBuffer> vertex_buffers_;
            std::array<float, 16> projection_{};
            std::uint32_t white_texture_ = 0;
            bool frame_active_ = false;
            struct VulkanStorageBuffer { VulkanBuffer buffer; };
            std::unordered_map<std::uint32_t, VulkanStorageBuffer> storage_buffers_;
            std::uint32_t next_storage_buffer_ = 1;
            std::array<std::uint32_t, 3> storage_handles_{};
            VkDescriptorSet storage_descriptor_ = VK_NULL_HANDLE;
            struct VulkanTexture
            {
                VulkanImage image;
                VulkanSampler sampler;
                VkDescriptorSet descriptor = VK_NULL_HANDLE;
            };
            struct VulkanRenderTarget
            {
                VulkanImage image;
                VulkanSampler sampler;
                VkDescriptorSet descriptor = VK_NULL_HANDLE;
                VkFramebuffer framebuffer = VK_NULL_HANDLE;
                std::uint32_t texture_handle = 0;
            };
            std::unordered_map<std::uint32_t, VulkanTexture> textures_;
            std::unordered_map<std::uint32_t, VulkanRenderTarget> render_targets_;
            std::uint32_t next_texture_ = 1;
            std::uint32_t next_render_target_ = 1;
        };
    }

    std::unique_ptr<Renderer> create_renderer(GraphicsBackend backend)
    {
        if (backend != GraphicsBackend::opengl)
        {
            if (backend == GraphicsBackend::vulkan)
            {
                return std::make_unique<VulkanRenderer>();
            }
            return nullptr;
        }
        return std::make_unique<OpenGLRenderer>();
    }
}

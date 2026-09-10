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
#include <fstream>
#include <iterator>
#include <array>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <system_error>

namespace sl::detail
{
    namespace
    {
        std::string load_glsl_shader(const char *name)
        {
            std::ifstream file(std::filesystem::path(SIMLIB_GLSL_SHADER_DIR) / name);
            return file ? std::string(std::istreambuf_iterator<char>(file), {}) : std::string{};
        }

        /** Compile GLSL source to SPIR-V at runtime by shelling out to glslangValidator. */
        bool compile_glsl_to_spirv(ShaderStage stage, const std::string &source,
                                   std::vector<std::uint32_t> &spirv, std::string &error)
        {
            static const char *validator = SIMLIB_GLSLANG_VALIDATOR;
            if (!validator || validator[0] == '\0')
            {
                error = "glslangValidator is not available for runtime Vulkan shader compilation.";
                return false;
            }
            namespace fs = std::filesystem;
            static std::uint64_t counter = 0;
            const std::string unique = std::to_string(++counter);
            const char *stage_extension = stage == ShaderStage::vertex ? "vert"
                : stage == ShaderStage::fragment ? "frag" : "comp";
            const fs::path directory = fs::temp_directory_path();
            const fs::path input = directory / ("simlib_shader_" + unique + "." + stage_extension);
            const fs::path output = directory / ("simlib_shader_" + unique + "." + stage_extension + ".spv");
            {
                std::ofstream file(input, std::ios::binary);
                if (!file)
                {
                    error = "Unable to write a temporary Vulkan shader source file.";
                    return false;
                }
                file << source;
            }
            const std::string command = std::string("\"") + validator + "\" -V --auto-map-locations --auto-map-bindings -S " +
                stage_extension + " \"" + input.string() + "\" -o \"" + output.string() + "\" > /dev/null 2>&1";
            const int result = std::system(command.c_str());
            std::error_code remove_error;
            fs::remove(input, remove_error);
            if (result != 0)
            {
                fs::remove(output, remove_error);
                error = "glslangValidator failed to compile the supplied Vulkan shader.";
                return false;
            }
            std::ifstream file(output, std::ios::binary | std::ios::ate);
            if (!file)
            {
                error = "Unable to read the compiled Vulkan shader output.";
                return false;
            }
            const std::streamsize size = file.tellg();
            if (size <= 0 || size % 4 != 0)
            {
                fs::remove(output, remove_error);
                error = "The compiled Vulkan shader output has an invalid size.";
                return false;
            }
            spirv.resize(static_cast<std::size_t>(size) / 4);
            file.seekg(0);
            file.read(reinterpret_cast<char *>(spirv.data()), size);
            fs::remove(output, remove_error);
            return true;
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
            bool begin_render_target(std::uint32_t framebuffer, int width, int height, std::string &) override
            {
                glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebuffer));
                glViewport(0, 0, width, height);
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
                    !context_.create_lighting_descriptor_layout(lighting_descriptor_layout_, error) ||
                    !context_.create_storage_descriptor_layout(storage_layout_, error) ||
                    !context_.create_descriptor_pool(descriptor_pool_, error)) return false;
                const std::filesystem::path shader_dir = SIMLIB_SHADER_BINARY_DIR;
                if (!context_.load_shader_module(shader_dir / "vulkan/vulkan_2d.vert.spv", vertex_module_, error) ||
                    !context_.load_shader_module(shader_dir / "vulkan/vulkan_2d.frag.spv", fragment_module_, error) ||
                    !create_pipelines(error)) return false;
                if (!context_.load_shader_module(shader_dir / "vulkan/vulkan_light_cull.comp.spv", cull_module_, error) ||
                    !context_.create_compute_pipeline(cull_module_, storage_layout_, sizeof(int) * 5,
                                                      cull_pipeline_, error)) return false;
                if (!context_.load_shader_module(shader_dir / "vulkan/vulkan_fullscreen.vert.spv", lighting_vertex_module_, error) ||
                    !context_.load_shader_module(shader_dir / "vulkan/vulkan_lighting.frag.spv", lighting_fragment_module_, error) ||
                    !context_.create_graphics_pipeline(lighting_vertex_module_, lighting_fragment_module_,
                        lighting_descriptor_layout_, PrimitiveType::triangle_fan, lighting_pipeline_, error,
                        &storage_layout_, sizeof(int) * 4 + sizeof(float) + sizeof(int))) return false;
                if (!context_.load_shader_module(shader_dir / "vulkan/vulkan_vignette.frag.spv", vignette_fragment_module_, error) ||
                    !context_.create_graphics_pipeline(lighting_vertex_module_, vignette_fragment_module_,
                        descriptor_layout_, PrimitiveType::triangle_fan, vignette_pipeline_, error,
                        nullptr, sizeof(VignetteConstants))) return false;
                if (!context_.load_shader_module(shader_dir / "vulkan/vulkan_bright_pass.frag.spv", bright_fragment_module_, error) ||
                    !context_.create_graphics_pipeline(lighting_vertex_module_, bright_fragment_module_,
                        descriptor_layout_, PrimitiveType::triangle_fan, bright_pipeline_, error,
                        nullptr, sizeof(BrightConstants))) return false;
                if (!context_.load_shader_module(shader_dir / "vulkan/vulkan_blur.frag.spv", blur_fragment_module_, error) ||
                    !context_.create_graphics_pipeline(lighting_vertex_module_, blur_fragment_module_,
                        descriptor_layout_, PrimitiveType::triangle_fan, blur_pipeline_, error,
                        nullptr, sizeof(BlurConstants))) return false;
                if (!context_.create_composite_descriptor_layout(composite_descriptor_layout_, error) ||
                    !context_.load_shader_module(shader_dir / "vulkan/vulkan_composite.frag.spv", composite_fragment_module_, error) ||
                    !context_.create_graphics_pipeline(lighting_vertex_module_, composite_fragment_module_,
                        composite_descriptor_layout_, PrimitiveType::triangle_fan, composite_pipeline_, error,
                        nullptr, sizeof(CompositeConstants))) return false;
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
                if (context_.device() != VK_NULL_HANDLE) vkDeviceWaitIdle(context_.device());
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
                context_.destroy_graphics_pipeline(lighting_pipeline_);
                context_.destroy_graphics_pipeline(vignette_pipeline_);
                context_.destroy_graphics_pipeline(bright_pipeline_);
                context_.destroy_graphics_pipeline(blur_pipeline_);
                context_.destroy_graphics_pipeline(composite_pipeline_);
                    context_.destroy_compute_pipeline(cull_pipeline_);
                context_.destroy_shader_module(vertex_module_);
                context_.destroy_shader_module(fragment_module_);
                    context_.destroy_shader_module(cull_module_);
                context_.destroy_shader_module(lighting_vertex_module_);
                context_.destroy_shader_module(lighting_fragment_module_);
                context_.destroy_shader_module(vignette_fragment_module_);
                context_.destroy_shader_module(bright_fragment_module_);
                context_.destroy_shader_module(blur_fragment_module_);
                context_.destroy_shader_module(composite_fragment_module_);
                for (auto &[handle, module] : shader_modules_)
                    context_.destroy_shader_module(module);
                shader_modules_.clear();
                for (auto &[handle, dynamic] : dynamic_programs_)
                {
                    for (auto &pipeline : dynamic.pipelines) context_.destroy_graphics_pipeline(pipeline);
                    if (dynamic.descriptor_set != VK_NULL_HANDLE) context_.free_descriptor_set(descriptor_pool_, dynamic.descriptor_set);
                    context_.destroy_descriptor_set_layout(dynamic.descriptor_layout);
                    context_.destroy_shader_module(dynamic.vertex_module);
                    context_.destroy_shader_module(dynamic.fragment_module);
                }
                dynamic_programs_.clear();
                context_.destroy_descriptor_pool(descriptor_pool_);
                context_.destroy_descriptor_set_layout(descriptor_layout_);
                context_.destroy_descriptor_set_layout(lighting_descriptor_layout_);
                context_.destroy_descriptor_set_layout(composite_descriptor_layout_);
                    context_.destroy_storage_descriptor_layout(storage_layout_);
                context_.shutdown();
                window_ = nullptr;
            }
            void resize(int width, int height) override
            {
                drawable_width_ = width;
                drawable_height_ = height;
                for (auto &[handle, target] : render_targets_)
                {
                    context_.destroy_sampler(target.sampler);
                    context_.destroy_framebuffer(target.framebuffer);
                    context_.destroy_image(target.image);
                }
                render_targets_.clear();
                for (VulkanGraphicsPipeline &pipeline : pipelines_)
                    context_.destroy_graphics_pipeline(pipeline);
                context_.destroy_graphics_pipeline(lighting_pipeline_);
                context_.destroy_graphics_pipeline(vignette_pipeline_);
                context_.destroy_graphics_pipeline(bright_pipeline_);
                context_.destroy_graphics_pipeline(blur_pipeline_);
                context_.destroy_graphics_pipeline(composite_pipeline_);
                if (context_.recreate_swapchain(width, height, last_error_))
                {
                    create_pipelines(last_error_);
                    context_.create_graphics_pipeline(lighting_vertex_module_, lighting_fragment_module_,
                        lighting_descriptor_layout_, PrimitiveType::triangle_fan, lighting_pipeline_, last_error_,
                        &storage_layout_, sizeof(int) * 4 + sizeof(float) + sizeof(int));
                    context_.create_graphics_pipeline(lighting_vertex_module_, vignette_fragment_module_,
                        descriptor_layout_, PrimitiveType::triangle_fan, vignette_pipeline_, last_error_,
                        nullptr, sizeof(VignetteConstants));
                    context_.create_graphics_pipeline(lighting_vertex_module_, bright_fragment_module_,
                        descriptor_layout_, PrimitiveType::triangle_fan, bright_pipeline_, last_error_,
                        nullptr, sizeof(BrightConstants));
                    context_.create_graphics_pipeline(lighting_vertex_module_, blur_fragment_module_,
                        descriptor_layout_, PrimitiveType::triangle_fan, blur_pipeline_, last_error_,
                        nullptr, sizeof(BlurConstants));
                    context_.create_graphics_pipeline(lighting_vertex_module_, composite_fragment_module_,
                        composite_descriptor_layout_, PrimitiveType::triangle_fan, composite_pipeline_, last_error_,
                        nullptr, sizeof(CompositeConstants));
                }
            }
            bool set_vsync(bool enabled) override
            {
                if (frame_active_ || (enabled == vsync_enabled_)) return !frame_active_;
                vsync_enabled_ = enabled;
                context_.set_vsync_enabled(enabled);
                resize(drawable_width_, drawable_height_);
                return context_.is_valid() && pipelines_[0].pipeline != VK_NULL_HANDLE;
            }
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
                if (frame_active_)
                {
                    vertex_buffer_cursor_ = 0;
                    for (VulkanTexture &texture : retired_textures_)
                    {
                        context_.free_descriptor_set(descriptor_pool_, texture.descriptor);
                        context_.destroy_sampler(texture.sampler);
                        context_.destroy_image(texture.image);
                    }
                    retired_textures_.clear();
                }
                return frame_active_;
            }
            bool end_frame(std::string &error) override
            {
                if (!frame_active_) return true;
                frame_active_ = false;
                return context_.end_frame(error);
            }
            SDL_GLContext native_context() const override { return nullptr; }
            VulkanContext *vulkan_context() override { return &context_; }

            bool create_texture(const TextureDesc &description, std::uint32_t &texture) override
            {
                VulkanTexture resource;
                if (!context_.create_image(description.width, description.height, VK_FORMAT_R8G8B8A8_UNORM,
                                           VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                           resource.image, last_error_) ||
                    !context_.create_sampler(description.filter, resource.sampler, last_error_))
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
                if (frame_active_)
                {
                    retired_textures_.push_back(std::move(iterator->second));
                    textures_.erase(iterator);
                    return;
                }
                context_.free_descriptor_set(descriptor_pool_, iterator->second.descriptor);
                context_.destroy_sampler(iterator->second.sampler);
                context_.destroy_image(iterator->second.image);
                textures_.erase(iterator);
            }
            bool create_render_target(int width, int height, std::uint32_t &texture, std::uint32_t &framebuffer) override
            {
                VulkanRenderTarget resource;
                if (!context_.create_image(width, height, context_.render_target_format(),
                                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                           resource.image, last_error_) ||
                    !context_.create_sampler(TextureFilter::linear, resource.sampler, last_error_) ||
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
                return iterator != render_targets_.end() && begin_frame(error) &&
                    context_.begin_offscreen_render_pass(iterator->second.framebuffer, width, height, error);
            }
            bool end_render_target(std::string &error) override
            { return context_.end_offscreen_render_pass(error); }
            bool create_shader(const ShaderSource &vertex_source, const ShaderSource &fragment_source,
                               std::uint32_t &program, std::string &error) override
            {
                if (vertex_source.asset_id == "lighting" && fragment_source.asset_id == "lighting" &&
                    lighting_pipeline_.pipeline != VK_NULL_HANDLE)
                {
                    program = lighting_program_;
                    return true;
                }
                if (vertex_source.asset_id == "vignette" && fragment_source.asset_id == "vignette" &&
                    vignette_pipeline_.pipeline != VK_NULL_HANDLE)
                {
                    program = vignette_program_;
                    return true;
                }
                if (vertex_source.asset_id == "bloom-bright" && fragment_source.asset_id == "bloom-bright" &&
                    bright_pipeline_.pipeline != VK_NULL_HANDLE)
                {
                    program = bright_program_;
                    return true;
                }
                if (vertex_source.asset_id == "bloom-blur" && fragment_source.asset_id == "bloom-blur" &&
                    blur_pipeline_.pipeline != VK_NULL_HANDLE)
                {
                    program = blur_program_;
                    return true;
                }
                if (vertex_source.asset_id == "bloom-composite" && fragment_source.asset_id == "bloom-composite" &&
                    composite_pipeline_.pipeline != VK_NULL_HANDLE)
                {
                    program = composite_program_;
                    return true;
                }
                if (vertex_source.language == ShaderLanguage::glsl && fragment_source.language == ShaderLanguage::glsl)
                {
                    return create_dynamic_shader(vertex_source, fragment_source, program, error);
                }
                error = "Vulkan renderer shader asset is unavailable.";
                return false;
            }
            bool create_compute_shader(const ShaderSource &source, std::uint32_t &program, std::string &error) override
            {
                if (source.asset_id != "light-cull" || cull_pipeline_.pipeline == VK_NULL_HANDLE)
                {
                    error = "Vulkan tiled-lighting compute pipeline is unavailable.";
                    return false;
                }
                program = cull_program_;
                return true;
            }
            bool create_shader_module(ShaderStage, const ShaderSource &source, std::uint32_t &module, std::string &error) override
            {
                if (source.language != ShaderLanguage::spirv)
                {
                    error = "Vulkan shader modules require SPIR-V payloads.";
                    return false;
                }
                VulkanShaderModule shader;
                if (!context_.create_shader_module(source.spirv, shader, error)) return false;
                module = next_shader_module_++;
                shader_modules_.emplace(module, std::move(shader));
                return true;
            }
            void destroy_shader(std::uint32_t module) override
            {
                if (module == cull_program_) return;
                const auto dynamic_iterator = dynamic_programs_.find(module);
                if (dynamic_iterator != dynamic_programs_.end())
                {
                    if (context_.device() != VK_NULL_HANDLE) vkDeviceWaitIdle(context_.device());
                    DynamicVulkanProgram &dynamic = dynamic_iterator->second;
                    for (auto &pipeline : dynamic.pipelines) context_.destroy_graphics_pipeline(pipeline);
                    if (dynamic.descriptor_set != VK_NULL_HANDLE) context_.free_descriptor_set(descriptor_pool_, dynamic.descriptor_set);
                    context_.destroy_descriptor_set_layout(dynamic.descriptor_layout);
                    context_.destroy_shader_module(dynamic.vertex_module);
                    context_.destroy_shader_module(dynamic.fragment_module);
                    dynamic_programs_.erase(dynamic_iterator);
                    return;
                }
                const auto iterator = shader_modules_.find(module);
                if (iterator == shader_modules_.end()) return;
                context_.destroy_shader_module(iterator->second);
                shader_modules_.erase(iterator);
            }
            bool use_shader(std::uint32_t program) override
            {
                if (dynamic_programs_.count(program))
                {
                    active_program_ = program;
                    return true;
                }
                if (program != cull_program_ && program != lighting_program_ && program != vignette_program_ &&
                    program != bright_program_ && program != blur_program_ && program != composite_program_) return false;
                active_program_ = program;
                return true;
            }
            void stop_shader() override { active_program_ = 0; }
            bool dispatch_compute(std::uint32_t program, unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) override
            {
                if (program != cull_program_ || storage_descriptor_ == VK_NULL_HANDLE) return unsupported();
                return context_.record_compute_dispatch(cull_pipeline_.pipeline, cull_pipeline_.layout,
                    storage_descriptor_, cull_constants_.data(), sizeof(cull_constants_),
                    groups_x, groups_y, groups_z, last_error_);
            }
            bool set_shader_int(std::uint32_t program, const char *name, int value) override
            {
                if (dynamic_programs_.count(program)) return set_dynamic_uniform(program, name, &value, sizeof(value));
                if (program == cull_program_ && name && std::strcmp(name, "lightCount") == 0)
                {
                    cull_constants_[4] = value;
                    return true;
                }
                if (program == lighting_program_ && name)
                {
                    active_program_ = program;
                    if (std::strcmp(name, "lightCount") == 0) lighting_constants_.light_count = value;
                    else if (std::strcmp(name, "shadowLightCount") == 0) lighting_constants_.shadow_light_count = value;
                    else if (std::strcmp(name, "flipVertical") == 0) lighting_constants_.flip_vertical = value;
                    else return true;
                    return true;
                }
                return unsupported();
            }
            bool set_shader_float(std::uint32_t program, const char *name, float value) override
            {
                if (dynamic_programs_.count(program)) return set_dynamic_uniform(program, name, &value, sizeof(value));
                if (program == lighting_program_ && name && std::strcmp(name, "ambient") == 0)
                {
                    active_program_ = program;
                    lighting_constants_.ambient = value;
                    return true;
                }
                if (program == vignette_program_ && name)
                {
                    active_program_ = program;
                    if (std::strcmp(name, "radius") == 0) vignette_constants_.radius = value;
                    else if (std::strcmp(name, "softness") == 0) vignette_constants_.softness = value;
                    else if (std::strcmp(name, "intensity") == 0) vignette_constants_.intensity = value;
                    else return true;
                    return true;
                }
                if (program == bright_program_ && name && std::strcmp(name, "threshold") == 0)
                {
                    active_program_ = program;
                    bright_constants_.threshold = value;
                    return true;
                }
                if (program == blur_program_ && name && std::strcmp(name, "radius") == 0)
                {
                    active_program_ = program;
                    blur_constants_.radius = value;
                    return true;
                }
                if (program == composite_program_ && name && std::strcmp(name, "intensity") == 0)
                {
                    active_program_ = program;
                    composite_constants_.intensity = value;
                    return true;
                }
                return unsupported();
            }
            bool set_shader_float2(std::uint32_t program, const char *name, float x, float y) override
            {
                if (dynamic_programs_.count(program))
                {
                    const float values[2] = {x, y};
                    return set_dynamic_uniform(program, name, values, sizeof(values));
                }
                if (program == blur_program_ && name)
                {
                    active_program_ = program;
                    if (std::strcmp(name, "texel") == 0) { blur_constants_.texel[0] = x; blur_constants_.texel[1] = y; return true; }
                    if (std::strcmp(name, "direction") == 0) { blur_constants_.direction[0] = x; blur_constants_.direction[1] = y; return true; }
                }
                return unsupported();
            }
            bool set_shader_int2(std::uint32_t program, const char *name, int x, int y) override
            {
                if (dynamic_programs_.count(program))
                {
                    const int values[2] = {x, y};
                    return set_dynamic_uniform(program, name, values, sizeof(values));
                }
                if (!name) return unsupported();
                if (program == lighting_program_ && std::strcmp(name, "tileCount") == 0)
                {
                    active_program_ = program;
                    lighting_constants_.tile_count_x = x;
                    lighting_constants_.tile_count_y = y;
                    return true;
                }
                if (program != cull_program_) return unsupported();
                if (std::strcmp(name, "screenSize") == 0)
                {
                    cull_constants_[0] = x;
                    cull_constants_[1] = y;
                    return true;
                }
                if (std::strcmp(name, "tileCount") == 0)
                {
                    cull_constants_[2] = x;
                    cull_constants_[3] = y;
                    return true;
                }
                return unsupported();
            }
            bool set_shader_float3(std::uint32_t program, const char *name, float x, float y, float z) override
            {
                if (dynamic_programs_.count(program))
                {
                    const float values[3] = {x, y, z};
                    return set_dynamic_uniform(program, name, values, sizeof(values));
                }
                return unsupported();
            }
            bool set_shader_mat4(std::uint32_t program, const char *name, const float *matrix) override
            {
                if (dynamic_programs_.count(program) && name && matrix)
                {
                    return set_dynamic_uniform(program, name, matrix, sizeof(float) * 16);
                }
                if (program == lighting_program_ && name && matrix && std::strcmp(name, "uProjection") == 0)
                {
                    active_program_ = program;
                    std::copy_n(matrix, lighting_projection_.size(), lighting_projection_.begin());
                    return true;
                }
                if (program == vignette_program_ && name && matrix && std::strcmp(name, "uProjection") == 0)
                {
                    active_program_ = program;
                    std::copy_n(matrix, vignette_projection_.size(), vignette_projection_.begin());
                    return true;
                }
                if ((program == bright_program_ || program == blur_program_ || program == composite_program_) &&
                    name && matrix && std::strcmp(name, "uProjection") == 0)
                {
                    active_program_ = program;
                    std::copy_n(matrix, postprocess_projection_.size(), postprocess_projection_.begin());
                    return true;
                }
                return unsupported();
            }
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
                const VkDeviceSize required_size = sizeof(Vertex2D) * static_cast<VkDeviceSize>(upload_count);
                if (vertex_buffer_cursor_ == vertex_buffers_.size()) vertex_buffers_.emplace_back();
                VulkanBuffer &buffer = vertex_buffers_[vertex_buffer_cursor_++];
                if (buffer.size < required_size)
                {
                    context_.destroy_buffer(buffer);
                    if (!context_.create_buffer(required_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buffer, last_error_)) return;
                }
                if (!context_.upload_buffer(buffer, upload_vertices, required_size, last_error_)) return;
                const std::uint32_t texture_handle = texture == 0 ? white_texture_ : texture;
                const auto dynamic_iterator = dynamic_programs_.find(active_program_);
                if (dynamic_iterator != dynamic_programs_.end())
                {
                    DynamicVulkanProgram &dynamic = dynamic_iterator->second;
                    auto resolve_texture = [this](std::uint32_t handle, VulkanImage &image, VulkanSampler &sampler)
                    {
                        const auto texture_iterator = textures_.find(handle);
                        if (texture_iterator != textures_.end())
                        {
                            image = texture_iterator->second.image;
                            sampler = texture_iterator->second.sampler;
                            return true;
                        }
                        for (const auto &[framebuffer, target] : render_targets_)
                        {
                            if (target.texture_handle == handle)
                            {
                                image = target.image;
                                sampler = target.sampler;
                                return true;
                            }
                        }
                        return false;
                    };
                    std::vector<VulkanImage> images(dynamic.sampler_names.size());
                    std::vector<VulkanSampler> samplers(dynamic.sampler_names.size());
                    for (std::size_t index = 0; index < images.size(); ++index)
                    {
                        const std::uint32_t handle = index < dynamic.bound_textures.size() && dynamic.bound_textures[index] != 0
                            ? dynamic.bound_textures[index]
                            : (index == 0 ? texture_handle : white_texture_);
                        if (!resolve_texture(handle, images[index], samplers[index]) &&
                            !resolve_texture(white_texture_, images[index], samplers[index]))
                        {
                            return;
                        }
                    }
                    if (dynamic.descriptor_set == VK_NULL_HANDLE)
                    {
                        if (!context_.allocate_descriptor_set(descriptor_pool_, dynamic.descriptor_layout,
                            dynamic.descriptor_set, last_error_)) return;
                    }
                    if (!images.empty() && !context_.update_dynamic_descriptor_set(dynamic.descriptor_set, images, samplers, last_error_))
                        return;
                    const std::size_t pipeline_index = static_cast<std::size_t>(primitive);
                    if (pipeline_index >= dynamic.pipelines.size()) return;
                    if (dynamic.pipelines[pipeline_index].pipeline == VK_NULL_HANDLE)
                    {
                        if (!context_.create_graphics_pipeline(dynamic.vertex_module, dynamic.fragment_module,
                            dynamic.descriptor_layout, primitive, dynamic.pipelines[pipeline_index], last_error_,
                            nullptr, dynamic.push_constant_size))
                        {
                            return;
                        }
                    }
                    context_.record_postprocess_draw(dynamic.pipelines[pipeline_index].pipeline,
                        dynamic.pipelines[pipeline_index].layout, buffer.buffer, dynamic.descriptor_set,
                        static_cast<std::uint32_t>(upload_count), dynamic.projection.data(),
                        dynamic.push_constants.empty() ? nullptr : dynamic.push_constants.data(),
                        static_cast<std::uint32_t>(dynamic.push_constants.size()), last_error_);
                    return;
                }
                if (active_program_ == lighting_program_)
                {
                    lighting_textures_[0] = texture_handle;
                    auto find_texture = [this](std::uint32_t handle, VulkanImage &image, VulkanSampler &sampler)
                    {
                        const auto texture_iterator = textures_.find(handle);
                        if (texture_iterator != textures_.end())
                        {
                            image = texture_iterator->second.image;
                            sampler = texture_iterator->second.sampler;
                            return true;
                        }
                        for (const auto &[framebuffer, target] : render_targets_)
                        {
                            if (target.texture_handle == handle)
                            {
                                image = target.image;
                                sampler = target.sampler;
                                return true;
                            }
                        }
                        return false;
                    };
                    std::array<VulkanImage, 9> images{};
                    std::array<VulkanSampler, 9> samplers{};
                    for (std::size_t index = 0; index < lighting_textures_.size(); ++index)
                    {
                        if (!find_texture(lighting_textures_[index], images[index], samplers[index]) &&
                            !find_texture(white_texture_, images[index], samplers[index]))
                        {
                            return;
                        }
                    }
                    const bool descriptor_ready = lighting_descriptor_ == VK_NULL_HANDLE
                        ? context_.allocate_lighting_descriptor(descriptor_pool_, lighting_descriptor_layout_,
                            images.data(), samplers.data(), lighting_descriptor_, last_error_)
                        : context_.update_lighting_descriptor(lighting_descriptor_, images.data(), samplers.data(), last_error_);
                    if (!descriptor_ready || storage_descriptor_ == VK_NULL_HANDLE)
                    {
                        return;
                    }
                    context_.record_lighting_draw(lighting_pipeline_.pipeline, lighting_pipeline_.layout,
                        buffer.buffer, lighting_descriptor_, storage_descriptor_, static_cast<std::uint32_t>(upload_count),
                        lighting_projection_.data(), &lighting_constants_, sizeof(lighting_constants_), last_error_);
                    return;
                }
                if (active_program_ == composite_program_)
                {
                    VulkanImage images[2]{};
                    VulkanSampler samplers[2]{};
                    const std::uint32_t bloom_handle = composite_bloom_texture_ != 0 ? composite_bloom_texture_ : white_texture_;
                    auto resolve_texture = [this](std::uint32_t handle, VulkanImage &image, VulkanSampler &sampler)
                    {
                        const auto texture_iterator = textures_.find(handle);
                        if (texture_iterator != textures_.end())
                        {
                            image = texture_iterator->second.image;
                            sampler = texture_iterator->second.sampler;
                            return true;
                        }
                        for (const auto &[framebuffer, target] : render_targets_)
                        {
                            if (target.texture_handle == handle)
                            {
                                image = target.image;
                                sampler = target.sampler;
                                return true;
                            }
                        }
                        return false;
                    };
                    if (!resolve_texture(texture_handle, images[0], samplers[0]) ||
                        !resolve_texture(bloom_handle, images[1], samplers[1]))
                    {
                        return;
                    }
                    const bool descriptor_ready = composite_descriptor_ == VK_NULL_HANDLE
                        ? context_.allocate_composite_descriptor(descriptor_pool_, composite_descriptor_layout_,
                            images, samplers, composite_descriptor_, last_error_)
                        : context_.update_composite_descriptor(composite_descriptor_, images, samplers, last_error_);
                    if (!descriptor_ready) return;
                    context_.record_postprocess_draw(composite_pipeline_.pipeline, composite_pipeline_.layout,
                        buffer.buffer, composite_descriptor_, static_cast<std::uint32_t>(upload_count),
                        postprocess_projection_.data(), &composite_constants_, sizeof(composite_constants_), last_error_);
                    return;
                }
                VkDescriptorSet descriptor = VK_NULL_HANDLE;
                const auto texture_iterator = textures_.find(texture_handle);
                if (texture_iterator != textures_.end())
                    descriptor = texture_iterator->second.descriptor;
                else
                {
                    for (const auto &[handle, target] : render_targets_)
                        if (target.texture_handle == texture_handle) descriptor = target.descriptor;
                }
                if (active_program_ == vignette_program_)
                {
                    context_.record_postprocess_draw(vignette_pipeline_.pipeline, vignette_pipeline_.layout,
                        buffer.buffer, descriptor, static_cast<std::uint32_t>(upload_count), vignette_projection_.data(),
                        &vignette_constants_, sizeof(vignette_constants_), last_error_);
                    return;
                }
                if (active_program_ == bright_program_)
                {
                    context_.record_postprocess_draw(bright_pipeline_.pipeline, bright_pipeline_.layout,
                        buffer.buffer, descriptor, static_cast<std::uint32_t>(upload_count), postprocess_projection_.data(),
                        &bright_constants_, sizeof(bright_constants_), last_error_);
                    return;
                }
                if (active_program_ == blur_program_)
                {
                    context_.record_postprocess_draw(blur_pipeline_.pipeline, blur_pipeline_.layout,
                        buffer.buffer, descriptor, static_cast<std::uint32_t>(upload_count), postprocess_projection_.data(),
                        &blur_constants_, sizeof(blur_constants_), last_error_);
                    return;
                }
                const std::size_t pipeline_index = static_cast<std::size_t>(primitive);
                if (pipeline_index >= pipelines_.size())
                {
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
            void bind_storage_buffer(unsigned int binding, std::uint32_t buffer) override
            {
                if (binding < 2 || binding > 4) return;
                storage_handles_[binding - 2] = buffer;
                update_storage_descriptor();
            }
            void bind_texture_unit(unsigned int unit, std::uint32_t texture) override
            {
                const auto dynamic_iterator = dynamic_programs_.find(active_program_);
                if (dynamic_iterator != dynamic_programs_.end())
                {
                    if (unit < dynamic_iterator->second.bound_textures.size())
                        dynamic_iterator->second.bound_textures[unit] = texture;
                    return;
                }
                if (active_program_ == composite_program_ && unit == 1)
                {
                    composite_bloom_texture_ = texture;
                    return;
                }
                if (unit < lighting_textures_.size()) lighting_textures_[unit] = texture;
            }
            void storage_barrier() override
            {
                // record_compute_dispatch already emits the required compute-to-fragment barrier.
            }

        private:
            bool update_storage_descriptor()
            {
                VulkanBuffer buffers[3]{};
                for (std::size_t index = 0; index < storage_handles_.size(); ++index)
                {
                    const auto iterator = storage_buffers_.find(storage_handles_[index]);
                    if (iterator == storage_buffers_.end()) return false;
                    buffers[index] = iterator->second.buffer;
                }
                if (storage_descriptor_ == VK_NULL_HANDLE)
                    return context_.allocate_storage_descriptor(descriptor_pool_, storage_layout_, buffers,
                        std::size(buffers), storage_descriptor_, last_error_);
                return context_.update_storage_descriptor(storage_descriptor_, buffers, std::size(buffers), last_error_);
            }

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

            bool create_dynamic_shader(const ShaderSource &vertex_source, const ShaderSource &fragment_source,
                                       std::uint32_t &program, std::string &error)
            {
                std::vector<std::uint32_t> vertex_spirv;
                std::vector<std::uint32_t> fragment_spirv;
                if (!compile_glsl_to_spirv(ShaderStage::vertex, vertex_source.text, vertex_spirv, error)) return false;
                if (!compile_glsl_to_spirv(ShaderStage::fragment, fragment_source.text, fragment_spirv, error)) return false;

                DynamicVulkanProgram dynamic;
                if (!context_.create_shader_module(vertex_spirv, dynamic.vertex_module, error)) return false;
                if (!context_.create_shader_module(fragment_spirv, dynamic.fragment_module, error))
                {
                    context_.destroy_shader_module(dynamic.vertex_module);
                    return false;
                }

                dynamic.sampler_names = fragment_source.vulkan_sampler_names;
                dynamic.bound_textures.assign(dynamic.sampler_names.size(), 0);

                std::vector<VkDescriptorSetLayoutBinding> bindings(dynamic.sampler_names.size());
                for (std::size_t index = 0; index < bindings.size(); ++index)
                {
                    bindings[index].binding = static_cast<std::uint32_t>(index);
                    bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    bindings[index].descriptorCount = 1;
                    bindings[index].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
                }
                if (!context_.create_dynamic_descriptor_layout(bindings, dynamic.descriptor_layout, error))
                {
                    context_.destroy_shader_module(dynamic.vertex_module);
                    context_.destroy_shader_module(dynamic.fragment_module);
                    return false;
                }

                std::uint32_t push_constant_size = 0;
                dynamic.uniform_layout = fragment_source.vulkan_uniforms;
                for (const ShaderUniformLayout &uniform : dynamic.uniform_layout)
                    push_constant_size = std::max(push_constant_size, uniform.offset + uniform.size);
                dynamic.push_constants.assign(push_constant_size, std::uint8_t{0});
                dynamic.push_constant_size = push_constant_size;
                // Pipeline variants (one per PrimitiveType) are created lazily on first use of
                // that topology, since a vertex shader supplied for triangle/line rendering
                // does not necessarily write gl_PointSize, which the point-list topology requires.

                program = next_dynamic_program_++;
                dynamic_programs_.emplace(program, std::move(dynamic));
                return true;
            }

            bool set_dynamic_uniform(std::uint32_t program, const char *name, const void *data, std::size_t size)
            {
                if (!name || !data) return unsupported();
                const auto iterator = dynamic_programs_.find(program);
                if (iterator == dynamic_programs_.end()) return unsupported();
                DynamicVulkanProgram &dynamic = iterator->second;
                if (std::strcmp(name, "uProjection") == 0)
                {
                    if (size != sizeof(float) * 16) return unsupported();
                    std::memcpy(dynamic.projection.data(), data, size);
                    return true;
                }
                for (const ShaderUniformLayout &uniform : dynamic.uniform_layout)
                {
                    if (uniform.name == name)
                    {
                        if (uniform.offset + uniform.size > dynamic.push_constants.size()) return unsupported();
                        std::memcpy(dynamic.push_constants.data() + uniform.offset, data, std::min<std::size_t>(size, uniform.size));
                        return true;
                    }
                }
                return unsupported();
            }

            SDL_Window *window_ = nullptr;
            unsigned int window_flags_ = 0;
            VulkanContext context_;
            std::string last_error_;
            VulkanDescriptorSetLayout descriptor_layout_;
            VulkanDescriptorSetLayout lighting_descriptor_layout_;
            VulkanDescriptorPool descriptor_pool_;
            VulkanShaderModule vertex_module_;
            VulkanShaderModule fragment_module_;
            VulkanShaderModule cull_module_;
            VulkanShaderModule lighting_vertex_module_;
            VulkanShaderModule lighting_fragment_module_;
            VulkanGraphicsPipeline lighting_pipeline_;
            VulkanShaderModule vignette_fragment_module_;
            VulkanGraphicsPipeline vignette_pipeline_;
            VulkanShaderModule bright_fragment_module_;
            VulkanGraphicsPipeline bright_pipeline_;
            VulkanShaderModule blur_fragment_module_;
            VulkanGraphicsPipeline blur_pipeline_;
            VulkanShaderModule composite_fragment_module_;
            VulkanGraphicsPipeline composite_pipeline_;
            VulkanDescriptorSetLayout composite_descriptor_layout_;
            VkDescriptorSet composite_descriptor_ = VK_NULL_HANDLE;
            std::uint32_t composite_bloom_texture_ = 0;
            static constexpr std::uint32_t cull_program_ = 0x80000000u;
            static constexpr std::uint32_t lighting_program_ = 0x80000001u;
            static constexpr std::uint32_t vignette_program_ = 0x80000002u;
            static constexpr std::uint32_t bright_program_ = 0x80000003u;
            static constexpr std::uint32_t blur_program_ = 0x80000004u;
            static constexpr std::uint32_t composite_program_ = 0x80000005u;
            std::uint32_t active_program_ = 0;
            std::unordered_map<std::uint32_t, VulkanShaderModule> shader_modules_;
            std::uint32_t next_shader_module_ = 1;
            std::array<VulkanGraphicsPipeline, 5> pipelines_{};
            VulkanComputePipeline cull_pipeline_;
            VulkanStorageDescriptorLayout storage_layout_;
            std::vector<VulkanBuffer> vertex_buffers_;
            std::size_t vertex_buffer_cursor_ = 0;
            std::array<float, 16> projection_{};
            std::uint32_t white_texture_ = 0;
            bool frame_active_ = false;
            bool vsync_enabled_ = true;
            int drawable_width_ = 0;
            int drawable_height_ = 0;
            struct VulkanStorageBuffer { VulkanBuffer buffer; };
            std::unordered_map<std::uint32_t, VulkanStorageBuffer> storage_buffers_;
            std::uint32_t next_storage_buffer_ = 1;
            std::array<std::uint32_t, 3> storage_handles_{};
            VkDescriptorSet storage_descriptor_ = VK_NULL_HANDLE;
            VkDescriptorSet lighting_descriptor_ = VK_NULL_HANDLE;
            std::array<int, 5> cull_constants_{};
            struct LightingConstants
            {
                int tile_count_x = 0;
                int tile_count_y = 0;
                int light_count = 0;
                int shadow_light_count = 0;
                float ambient = 0.0f;
                int flip_vertical = 0;
            } lighting_constants_;
            std::array<float, 16> lighting_projection_{};
            std::array<std::uint32_t, 9> lighting_textures_{};
            struct VignetteConstants { float radius = 0.0f, softness = 0.0f, intensity = 0.0f; } vignette_constants_;
            std::array<float, 16> vignette_projection_{};
            struct BrightConstants { float threshold = 0.0f; } bright_constants_;
            struct BlurConstants { float texel[2] = {0.0f, 0.0f}; float direction[2] = {0.0f, 0.0f}; float radius = 0.0f; } blur_constants_;
            struct CompositeConstants { float intensity = 0.0f; } composite_constants_;
            std::array<float, 16> postprocess_projection_{};
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
            std::vector<VulkanTexture> retired_textures_;
            std::unordered_map<std::uint32_t, VulkanRenderTarget> render_targets_;
            std::uint32_t next_texture_ = 1;
            std::uint32_t next_render_target_ = 1;

            struct DynamicVulkanProgram
            {
                std::array<VulkanGraphicsPipeline, 5> pipelines{};
                VulkanDescriptorSetLayout descriptor_layout;
                VulkanShaderModule vertex_module;
                VulkanShaderModule fragment_module;
                VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
                std::vector<std::string> sampler_names;
                std::vector<std::uint32_t> bound_textures;
                std::vector<ShaderUniformLayout> uniform_layout;
                std::vector<std::uint8_t> push_constants;
                std::uint32_t push_constant_size = 0;
                std::array<float, 16> projection{};
            };
            std::unordered_map<std::uint32_t, DynamicVulkanProgram> dynamic_programs_;
            std::uint32_t next_dynamic_program_ = 1;
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

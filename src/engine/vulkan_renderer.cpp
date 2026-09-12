/** @file
 * @brief Implements the Vulkan 1.3 Renderer backend.
 */
#include "vulkan_renderer.h"
#include "vulkan_context.h"

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
#include <map>
#include <cstring>
#include <cstdlib>
#include <system_error>

namespace sl::detail
{
    namespace
    {
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
            const fs::path log = directory / ("simlib_shader_" + unique + ".log");
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
                stage_extension + " \"" + input.string() + "\" -o \"" + output.string() + "\" > \"" + log.string() + "\" 2>&1";
            const int result = std::system(command.c_str());
            std::error_code remove_error;
            fs::remove(input, remove_error);
            if (result != 0)
            {
                fs::remove(output, remove_error);
                std::ifstream log_file(log);
                const std::string diagnostics((std::istreambuf_iterator<char>(log_file)), {});
                fs::remove(log, remove_error);
                error = "glslangValidator failed to compile the supplied Vulkan shader";
                if (!diagnostics.empty()) error += ":\n" + diagnostics;
                return false;
            }
            fs::remove(log, remove_error);
            std::ifstream file(output, std::ios::binary | std::ios::ate);
            if (!file)
            {
                fs::remove(output, remove_error);
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
                    !context_.load_shader_module(shader_dir / "vulkan/vulkan_colour_adjust.frag.spv", colour_adjust_fragment_module_, error) ||
                    !context_.load_shader_module(shader_dir / "vulkan/vulkan_pixelate.frag.spv", pixelate_fragment_module_, error) ||
                    !context_.load_shader_module(shader_dir / "vulkan/vulkan_radial_blur.frag.spv", radial_fragment_module_, error) ||
                    !context_.load_shader_module(shader_dir / "vulkan/vulkan_heat_haze.frag.spv", heat_fragment_module_, error) ||
                    !context_.load_shader_module(shader_dir / "vulkan/vulkan_shockwave.frag.spv", shockwave_fragment_module_, error) ||
                    !context_.load_shader_module(shader_dir / "vulkan/vulkan_chromatic_aberration.frag.spv", chromatic_fragment_module_, error) ||
                    !context_.load_shader_module(shader_dir / "vulkan/vulkan_crt.frag.spv", crt_fragment_module_, error) ||
                    !context_.load_shader_module(shader_dir / "vulkan/vulkan_dither.frag.spv", dither_fragment_module_, error) ||
                    !context_.load_shader_module(shader_dir / "vulkan/vulkan_film_grain.frag.spv", film_grain_fragment_module_, error) ||
                    !context_.load_shader_module(shader_dir / "vulkan/vulkan_bright_pass.frag.spv", bright_fragment_module_, error) ||
                    !context_.load_shader_module(shader_dir / "vulkan/vulkan_blur.frag.spv", blur_fragment_module_, error) ||
                    !context_.create_composite_descriptor_layout(composite_descriptor_layout_, error) ||
                    !context_.load_shader_module(shader_dir / "vulkan/vulkan_composite.frag.spv", composite_fragment_module_, error) ||
                    !create_effect_pipelines(error)) return false;
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
                    context_.destroy_image(target.depth);
                    context_.destroy_image(target.depth);
                }
                render_targets_.clear();
                for (VulkanGraphicsPipeline &pipeline : pipelines_)
                    context_.destroy_graphics_pipeline(pipeline);
                    context_.destroy_graphics_pipeline(bright_effect_.pipeline);
                    context_.destroy_graphics_pipeline(blur_effect_.pipeline);
                    context_.destroy_graphics_pipeline(composite_effect_.pipeline);
                    context_.destroy_graphics_pipeline(lighting_pipeline_);
                    context_.destroy_graphics_pipeline(vignette_effect_.pipeline);
                    context_.destroy_graphics_pipeline(colour_adjust_effect_.pipeline);
                    context_.destroy_graphics_pipeline(pixelate_effect_.pipeline);
                    context_.destroy_graphics_pipeline(radial_effect_.pipeline);
                    context_.destroy_graphics_pipeline(heat_effect_.pipeline);
                    context_.destroy_graphics_pipeline(shockwave_effect_.pipeline);
                    context_.destroy_graphics_pipeline(chromatic_effect_.pipeline);
                    context_.destroy_graphics_pipeline(crt_effect_.pipeline);
                    context_.destroy_graphics_pipeline(dither_effect_.pipeline);
                    context_.destroy_graphics_pipeline(film_grain_effect_.pipeline);
                context_.destroy_graphics_pipeline(bright_effect_.pipeline);
                context_.destroy_graphics_pipeline(blur_effect_.pipeline);
                context_.destroy_graphics_pipeline(composite_effect_.pipeline);
                    context_.destroy_compute_pipeline(cull_pipeline_);
                context_.destroy_shader_module(vertex_module_);
                context_.destroy_shader_module(fragment_module_);
                    context_.destroy_shader_module(cull_module_);
                context_.destroy_shader_module(lighting_vertex_module_);
                context_.destroy_shader_module(lighting_fragment_module_);
                context_.destroy_shader_module(vignette_fragment_module_);
                    context_.destroy_shader_module(colour_adjust_fragment_module_);
                    context_.destroy_shader_module(pixelate_fragment_module_);
                    context_.destroy_shader_module(radial_fragment_module_);
                    context_.destroy_shader_module(heat_fragment_module_);
                    context_.destroy_shader_module(shockwave_fragment_module_);
                    context_.destroy_shader_module(chromatic_fragment_module_);
                    context_.destroy_shader_module(crt_fragment_module_);
                    context_.destroy_shader_module(dither_fragment_module_);
                    context_.destroy_shader_module(film_grain_fragment_module_);
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
            bool resize(int width, int height, std::string &error) override
            {
                if (width <= 0 || height <= 0)
                {
                    error = "Invalid Vulkan viewport dimensions.";
                    return false;
                }
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
                context_.destroy_graphics_pipeline(vignette_effect_.pipeline);
                context_.destroy_graphics_pipeline(bright_effect_.pipeline);
                context_.destroy_graphics_pipeline(blur_effect_.pipeline);
                context_.destroy_graphics_pipeline(composite_effect_.pipeline);
                if (!context_.recreate_swapchain(width, height, last_error_))
                {
                    error = last_error_;
                    return false;
                }
                    if (!create_pipelines(last_error_) ||
                        !context_.create_graphics_pipeline(lighting_vertex_module_, lighting_fragment_module_,
                            lighting_descriptor_layout_, PrimitiveType::triangle_fan, lighting_pipeline_, last_error_,
                            &storage_layout_, sizeof(int) * 4 + sizeof(float) + sizeof(int)) ||
                        !create_effect_pipelines(last_error_))
                {
                    error = last_error_;
                    return false;
                }
                return true;
            }
            bool set_vsync(bool enabled) override
            {
                if (frame_active_ || (enabled == vsync_enabled_)) return !frame_active_;
                vsync_enabled_ = enabled;
                context_.set_vsync_enabled(enabled);
                std::string error;
                return resize(drawable_width_, drawable_height_, error) &&
                    context_.is_valid() && pipelines_[0].pipeline != VK_NULL_HANDLE;
            }
            bool vsync_active() const override { return vsync_enabled_; }
            void present() override
            {
                if (frame_active_)
                {
                    flush_2d();
                    context_.end_frame(last_error_);
                    frame_active_ = false;
                }
            }
            void wait_idle() override
            {
                flush_2d();
                if (context_.device() != VK_NULL_HANDLE) vkDeviceWaitIdle(context_.device());
            }
            bool begin_frame(std::string &error) override
            {
                if (frame_active_ && !context_.frame_active()) frame_active_ = false;
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
                flush_2d();
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
                if (iterator != textures_.end())
                    return context_.upload_image_rgba(iterator->second.image, width, height, pixels, last_error_);
                for (auto &[framebuffer, target] : render_targets_)
                {
                    if (target.texture_handle == texture)
                        return context_.upload_image_rgba(target.image, width, height, pixels, last_error_);
                }
                return false;
            }
            void destroy_texture(std::uint32_t texture) override
            {
                if (batch_texture_ == texture)
                {
                    flush_2d();
                }
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
                    !context_.create_image(width, height, context_.depth_format(),
                                           VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                           resource.depth, last_error_) ||
                    !context_.create_sampler(TextureFilter::linear, resource.sampler, last_error_) ||
                    !context_.create_render_target_framebuffer(resource.image, resource.depth, width, height,
                                                               resource.framebuffer, last_error_))
                {
                    context_.destroy_framebuffer(resource.framebuffer);
                    context_.destroy_sampler(resource.sampler);
                    context_.destroy_image(resource.image);
                    context_.destroy_image(resource.depth);
                    return false;
                }
                framebuffer = next_render_target_++;
                texture = 0x40000000u | framebuffer;
                resource.texture_handle = texture;
                if (!context_.allocate_texture_descriptor(descriptor_pool_, descriptor_layout_, resource.image,
                                                          resource.sampler, resource.descriptor, last_error_))
                {
                    context_.destroy_framebuffer(resource.framebuffer);
                    context_.destroy_sampler(resource.sampler);
                    context_.destroy_image(resource.image);
                    context_.destroy_image(resource.depth);
                    return false;
                }
                render_targets_.emplace(framebuffer, std::move(resource));
                return true;
            }
            void destroy_render_target(std::uint32_t texture, std::uint32_t framebuffer) override
            {
                flush_2d();
                const auto iterator = render_targets_.find(framebuffer);
                if (iterator == render_targets_.end()) return;
                context_.destroy_sampler(iterator->second.sampler);
                context_.destroy_framebuffer(iterator->second.framebuffer);
                context_.destroy_image(iterator->second.image);
                context_.destroy_image(iterator->second.depth);
                render_targets_.erase(iterator);
            }
            bool begin_render_target(std::uint32_t framebuffer, int width, int height, std::string &error) override
            {
                flush_2d();
                const auto iterator = render_targets_.find(framebuffer);
                if (iterator == render_targets_.end() || !begin_frame(error) ||
                    !context_.begin_offscreen_render_pass(iterator->second.framebuffer, iterator->second.image,
                        width, height, error))
                    return false;
                active_render_targets_.push_back(framebuffer);
                iterator->second.image.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                return true;
            }
            bool end_render_target(std::string &error) override
            {
                flush_2d();
                if (!context_.end_offscreen_render_pass(error)) return false;
                if (!active_render_targets_.empty())
                {
                    const std::uint32_t framebuffer = active_render_targets_.back();
                    active_render_targets_.pop_back();
                    const auto iterator = render_targets_.find(framebuffer);
                    if (iterator != render_targets_.end())
                    {
                        iterator->second.image.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        if (!context_.update_texture_descriptor(iterator->second.descriptor,
                            iterator->second.image, iterator->second.sampler, last_error_))
                            return false;
                    }
                }
                return true;
            }
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
                    vignette_effect_.pipeline.pipeline != VK_NULL_HANDLE)
                {
                    program = vignette_effect_.program;
                    return true;
                }
                if (vertex_source.asset_id == "colour-adjust" && fragment_source.asset_id == "colour-adjust" &&
                    colour_adjust_effect_.pipeline.pipeline != VK_NULL_HANDLE)
                {
                    program = colour_adjust_effect_.program;
                    return true;
                }
                if (vertex_source.asset_id == "chromatic-aberration" && fragment_source.asset_id == "chromatic-aberration" &&
                    chromatic_effect_.pipeline.pipeline != VK_NULL_HANDLE)
                {
                    program = chromatic_effect_.program;
                    return true;
                }
                if (vertex_source.asset_id == "pixelate" && fragment_source.asset_id == "pixelate" &&
                    pixelate_effect_.pipeline.pipeline != VK_NULL_HANDLE)
                {
                    program = pixelate_program_;
                    return true;
                }
                if (vertex_source.asset_id == "radial-blur" && fragment_source.asset_id == "radial-blur" &&
                    radial_effect_.pipeline.pipeline != VK_NULL_HANDLE)
                {
                    program = radial_program_;
                    return true;
                }
                if (vertex_source.asset_id == "heat-haze" && fragment_source.asset_id == "heat-haze" &&
                    heat_effect_.pipeline.pipeline != VK_NULL_HANDLE)
                {
                    program = heat_program_;
                    return true;
                }
                if (vertex_source.asset_id == "shockwave" && fragment_source.asset_id == "shockwave" &&
                    shockwave_effect_.pipeline.pipeline != VK_NULL_HANDLE)
                {
                    program = shockwave_program_;
                    return true;
                }
                if (vertex_source.asset_id == "crt" && fragment_source.asset_id == "crt" &&
                    crt_effect_.pipeline.pipeline != VK_NULL_HANDLE)
                {
                    program = crt_program_;
                    return true;
                }
                if (vertex_source.asset_id == "dither" && fragment_source.asset_id == "dither" &&
                    dither_effect_.pipeline.pipeline != VK_NULL_HANDLE)
                {
                    program = dither_program_;
                    return true;
                }
                if (vertex_source.asset_id == "film-grain" && fragment_source.asset_id == "film-grain" &&
                    film_grain_effect_.pipeline.pipeline != VK_NULL_HANDLE)
                {
                    program = film_grain_program_;
                    return true;
                }
                if (vertex_source.asset_id == "bloom-bright" && fragment_source.asset_id == "bloom-bright" &&
                    bright_effect_.pipeline.pipeline != VK_NULL_HANDLE)
                {
                    program = bright_program_;
                    return true;
                }
                if (vertex_source.asset_id == "bloom-blur" && fragment_source.asset_id == "bloom-blur" &&
                    blur_effect_.pipeline.pipeline != VK_NULL_HANDLE)
                {
                    program = blur_program_;
                    return true;
                }
                if (vertex_source.asset_id == "bloom-composite" && fragment_source.asset_id == "bloom-composite" &&
                    composite_effect_.pipeline.pipeline != VK_NULL_HANDLE)
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
                flush_2d();
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
                    if (active_program_ != program)
                    {
                        flush_2d();
                        active_program_ = program;
                    }
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::unknown && program != cull_program_) return false;
                if (active_program_ != program)
                {
                    flush_2d();
                    active_program_ = program;
                }
                return true;
            }
            void stop_shader() override
            {
                if (active_program_ != 0)
                {
                    flush_2d();
                    active_program_ = 0;
                }
            }
            bool dispatch_compute(std::uint32_t program, unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) override
            {
                flush_2d();
                if (program != cull_program_ || storage_descriptor_ == VK_NULL_HANDLE) return unsupported();
                return context_.record_compute_dispatch(cull_pipeline_.pipeline, cull_pipeline_.layout,
                    storage_descriptor_, cull_constants_.data(), sizeof(cull_constants_),
                    groups_x, groups_y, groups_z, last_error_);
            }
            bool set_shader_int(std::uint32_t program, const char *name, int value) override
            {
                if (dynamic_programs_.count(program)) return set_dynamic_uniform(program, name, &value, sizeof(value));
                if (program == colour_adjust_effect_.program && name && std::strcmp(name, "source") == 0)
                {
                    active_program_ = program;
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::radial && name && std::strcmp(name, "samples") == 0)
                {
                    active_program_ = program;
                    radial_constants_.samples = value;
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::blur && name)
                {
                    active_program_ = program;
                    return set_blur_uniform(name, value);
                }
                if (program == cull_program_ && name && std::strcmp(name, "lightCount") == 0)
                {
                    cull_constants_[4] = value;
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::lighting && name)
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
                if (program_kind(program) == BuiltinProgram::lighting && name && std::strcmp(name, "ambient") == 0)
                {
                    active_program_ = program;
                    lighting_constants_.ambient = value;
                    return true;
                }
                if (program == vignette_effect_.program && name)
                {
                    active_program_ = program;
                    if (std::strcmp(name, "radius") == 0) vignette_effect_.constants.radius = value;
                    else if (std::strcmp(name, "softness") == 0) vignette_effect_.constants.softness = value;
                    else if (std::strcmp(name, "intensity") == 0) vignette_effect_.constants.intensity = value;
                    else return true;
                    return true;
                }
                if (program == colour_adjust_effect_.program && name)
                {
                    active_program_ = program;
                    if (std::strcmp(name, "brightness") == 0) colour_adjust_constants_.values[0] = value;
                    else if (std::strcmp(name, "contrast") == 0) colour_adjust_constants_.values[1] = value;
                    else if (std::strcmp(name, "saturation") == 0) colour_adjust_constants_.values[2] = value;
                    else if (std::strcmp(name, "exposure") == 0) colour_adjust_constants_.values[3] = value;
                    else return true;
                    return true;
                }
                if (program == chromatic_effect_.program && name && std::strcmp(name, "strength") == 0)
                {
                    active_program_ = program;
                    chromatic_constants_.strength = value;
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::pixelate && name && std::strcmp(name, "pixelSize") == 0)
                {
                    active_program_ = program;
                    pixelate_constants_.pixel_size[0] = value;
                    pixelate_constants_.pixel_size[1] = value;
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::crt && name)
                {
                    active_program_ = program;
                    if (std::strcmp(name, "pixelSize") == 0) { crt_constants_.pixel_size = value; return true; }
                    if (std::strcmp(name, "scanlineStrength") == 0) { crt_constants_.scanline_strength = value; return true; }
                    if (std::strcmp(name, "curvature") == 0) { crt_constants_.curvature = value; return true; }
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::dither && name)
                {
                    active_program_ = program;
                    if (std::strcmp(name, "pixelSize") == 0) { dither_constants_.pixel_size = value; return true; }
                    if (std::strcmp(name, "levels") == 0) { dither_constants_.levels = value; return true; }
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::film_grain && name)
                {
                    active_program_ = program;
                    if (std::strcmp(name, "strength") == 0) film_grain_constants_.strength = value;
                    else if (std::strcmp(name, "time") == 0) film_grain_constants_.time = value;
                    else return true;
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::radial && name)
                {
                    active_program_ = program;
                    if (std::strcmp(name, "strength") == 0) radial_constants_.strength = value;
                    else return true;
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::heat && name)
                {
                    active_program_ = program;
                    if (std::strcmp(name, "strength") == 0) heat_constants_.strength = value;
                    else if (std::strcmp(name, "frequency") == 0) heat_constants_.frequency = value;
                    else if (std::strcmp(name, "time") == 0) heat_constants_.time = value;
                    else return true;
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::shockwave && name)
                {
                    active_program_ = program;
                    if (std::strcmp(name, "radius") == 0) shockwave_constants_.radius = value;
                    else if (std::strcmp(name, "width") == 0) shockwave_constants_.width = value;
                    else if (std::strcmp(name, "strength") == 0) shockwave_constants_.strength = value;
                    else return true;
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::bright && name && std::strcmp(name, "threshold") == 0)
                {
                    active_program_ = program;
                    bright_constants_.threshold = value;
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::blur && name)
                {
                    active_program_ = program;
                    return set_blur_uniform(name, value);
                }
                if (program_kind(program) == BuiltinProgram::composite && name && std::strcmp(name, "intensity") == 0)
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
                if (program_kind(program) == BuiltinProgram::blur && name)
                {
                    active_program_ = program;
                    if (std::strcmp(name, "texel") == 0) { blur_constants_.texel[0] = x; blur_constants_.texel[1] = y; return true; }
                    if (std::strcmp(name, "direction") == 0) { blur_constants_.direction[0] = x; blur_constants_.direction[1] = y; return true; }
                }
                if (program_kind(program) == BuiltinProgram::pixelate && name && std::strcmp(name, "resolution") == 0)
                {
                    active_program_ = program;
                    pixelate_constants_.resolution[0] = x;
                    pixelate_constants_.resolution[1] = y;
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::crt && name && std::strcmp(name, "resolution") == 0)
                {
                    active_program_ = program;
                    crt_constants_.resolution[0] = x;
                    crt_constants_.resolution[1] = y;
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::dither && name && std::strcmp(name, "resolution") == 0)
                {
                    active_program_ = program;
                    dither_constants_.resolution[0] = x;
                    dither_constants_.resolution[1] = y;
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::radial && name && std::strcmp(name, "centre") == 0)
                {
                    active_program_ = program;
                    radial_constants_.centre[0] = x;
                    radial_constants_.centre[1] = y;
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::shockwave && name && std::strcmp(name, "centre") == 0)
                {
                    active_program_ = program;
                    shockwave_constants_.centre[0] = x;
                    shockwave_constants_.centre[1] = y;
                    return true;
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
                if (program_kind(program) == BuiltinProgram::lighting && std::strcmp(name, "tileCount") == 0)
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
                if (program_kind(program) == BuiltinProgram::lighting && name && matrix && std::strcmp(name, "uProjection") == 0)
                {
                    active_program_ = program;
                    std::copy_n(matrix, lighting_projection_.size(), lighting_projection_.begin());
                    return true;
                }
                if (program == vignette_effect_.program && name && matrix && std::strcmp(name, "uProjection") == 0)
                {
                    active_program_ = program;
                    std::copy_n(matrix, vignette_projection_.size(), vignette_projection_.begin());
                    return true;
                }
                if ((program_kind(program) == BuiltinProgram::bright || program_kind(program) == BuiltinProgram::blur ||
                    program_kind(program) == BuiltinProgram::composite) &&
                    name && matrix && std::strcmp(name, "uProjection") == 0)
                {
                    active_program_ = program;
                    std::copy_n(matrix, postprocess_projection_.size(), postprocess_projection_.begin());
                    return true;
                }
                if (program == colour_adjust_effect_.program && name && matrix && std::strcmp(name, "uProjection") == 0)
                {
                    active_program_ = program;
                    std::copy_n(matrix, postprocess_projection_.size(), postprocess_projection_.begin());
                    return true;
                }
                if (program == chromatic_effect_.program && name && matrix && std::strcmp(name, "uProjection") == 0)
                {
                    active_program_ = program;
                    std::copy_n(matrix, postprocess_projection_.size(), postprocess_projection_.begin());
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::pixelate && name && matrix && std::strcmp(name, "uProjection") == 0)
                {
                    active_program_ = program;
                    std::copy_n(matrix, postprocess_projection_.size(), postprocess_projection_.begin());
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::radial && name && matrix && std::strcmp(name, "uProjection") == 0)
                {
                    active_program_ = program;
                    std::copy_n(matrix, postprocess_projection_.size(), postprocess_projection_.begin());
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::heat && name && matrix && std::strcmp(name, "uProjection") == 0)
                {
                    active_program_ = program;
                    std::copy_n(matrix, postprocess_projection_.size(), postprocess_projection_.begin());
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::shockwave && name && matrix && std::strcmp(name, "uProjection") == 0)
                {
                    active_program_ = program;
                    std::copy_n(matrix, postprocess_projection_.size(), postprocess_projection_.begin());
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::film_grain && name && matrix && std::strcmp(name, "uProjection") == 0)
                {
                    active_program_ = program;
                    std::copy_n(matrix, postprocess_projection_.size(), postprocess_projection_.begin());
                    return true;
                }
                if (program_kind(program) == BuiltinProgram::crt && name && matrix && std::strcmp(name, "uProjection") == 0)
                {
                    active_program_ = program;
                    std::copy_n(matrix, postprocess_projection_.size(), postprocess_projection_.begin());
                    return true;
                }
                return unsupported();
            }
            bool initialise_2d() override { return pipelines_[4].pipeline != VK_NULL_HANDLE; }
            void shutdown_2d() override
            {
                flush_2d();
                batch_vertices_.clear();
                batch_vertices_.shrink_to_fit();
            }
            bool begin_2d(int width, int height) override
            {
                if (!begin_frame(last_error_)) return false;
                std::array<float, 16> new_proj{};
                new_proj[0] = width > 0 ? 2.0f / width : 0.0f;
                new_proj[5] = height > 0 ? -2.0f / height : 0.0f;
                new_proj[10] = -1.0f;
                new_proj[12] = -1.0f + (width > 0 ? 2.0f * detail::screen_offset_x() / width : 0.0f);
                new_proj[13] = 1.0f - (height > 0 ? 2.0f * detail::screen_offset_y() / height : 0.0f);
                new_proj[15] = 1.0f;
                if (active_program_ != 0 || new_proj != projection_)
                {
                    flush_2d();
                    active_program_ = 0;
                    projection_ = new_proj;
                }
                return true;
            }
            bool begin_shader_2d(std::uint32_t program, int width, int height) override
            {
                flush_2d();
                if (!begin_frame(last_error_) || width <= 0 || height <= 0 || !use_shader(program)) return false;
                float projection[16] = {};
                projection[0] = 2.0f / width;
                projection[5] = -2.0f / height;
                projection[10] = -1.0f;
                projection[12] = -1.0f + 2.0f * detail::screen_offset_x() / width;
                projection[13] = 1.0f - 2.0f * detail::screen_offset_y() / height;
                projection[15] = 1.0f;
                return set_shader_mat4(program, "uProjection", projection);
            }
            void set_premultiplied_alpha(bool) override {}
            bool clear_frame(float red, float green, float blue, float alpha) override
            {
                flush_2d();
                if (!frame_active_ && !begin_frame(last_error_)) return false;
                return context_.clear_active_frame(red, green, blue, alpha, last_error_);
            }
            void flush_2d() override
            {
                if (batch_vertices_.empty() || !frame_active_) return;

                const Vertex2D *upload_vertices = batch_vertices_.data();
                const int upload_count = static_cast<int>(batch_vertices_.size());
                const PrimitiveType primitive = batch_primitive_;
                const std::uint32_t texture = batch_texture_;

                const VkDeviceSize required_size = sizeof(Vertex2D) * static_cast<VkDeviceSize>(upload_count);
                if (vertex_buffer_cursor_ == vertex_buffers_.size()) vertex_buffers_.emplace_back();
                VulkanBuffer &buffer = vertex_buffers_[vertex_buffer_cursor_++];
                if (buffer.size < required_size)
                {
                    VkDeviceSize new_size = std::max<VkDeviceSize>(buffer.size, sizeof(Vertex2D) * 64);
                    while (new_size < required_size) new_size *= 2;
                    context_.destroy_buffer(buffer);
                    if (!context_.create_buffer(new_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buffer, last_error_))
                    {
                        batch_vertices_.clear();
                        return;
                    }
                }
                if (!context_.upload_buffer(buffer, upload_vertices, required_size, last_error_))
                {
                    batch_vertices_.clear();
                    return;
                }
                const std::uint32_t texture_handle = texture == 0 ? white_texture_ : texture;
                auto prepare_texture = [this](VulkanImage &image)
                {
                    return context_.prepare_image_for_sampling(image, last_error_);
                };
                auto resolve_registered_texture = [this, &prepare_texture](std::uint32_t handle,
                    VulkanImage &image, VulkanSampler &sampler)
                {
                    const auto texture_iterator = textures_.find(handle);
                    if (texture_iterator != textures_.end())
                    {
                        image = texture_iterator->second.image;
                        sampler = texture_iterator->second.sampler;
                        return prepare_texture(image);
                    }
                    for (const auto &[framebuffer, target] : render_targets_)
                    {
                        if (target.texture_handle == handle)
                        {
                            image = target.image;
                            sampler = target.sampler;
                            return prepare_texture(image);
                        }
                    }
                    last_error_ = "Vulkan sampled texture handle is not registered.";
                    return false;
                };
                const auto dynamic_iterator = dynamic_programs_.find(batch_program_);
                if (dynamic_iterator != dynamic_programs_.end())
                {
                    DynamicVulkanProgram &dynamic = dynamic_iterator->second;
                    std::vector<VulkanImage> images(dynamic.sampler_names.size());
                    std::vector<VulkanSampler> samplers(dynamic.sampler_names.size());
                    for (std::size_t index = 0; index < images.size(); ++index)
                    {
                        const std::uint32_t handle = index < dynamic.bound_textures.size() && dynamic.bound_textures[index] != 0
                            ? dynamic.bound_textures[index]
                            : (index == 0 ? texture_handle : white_texture_);
                        if (!resolve_registered_texture(handle, images[index], samplers[index]) &&
                            !resolve_registered_texture(white_texture_, images[index], samplers[index]))
                        {
                            batch_vertices_.clear();
                            return;
                        }
                    }
                    if (dynamic.descriptor_set == VK_NULL_HANDLE)
                    {
                        if (!context_.allocate_descriptor_set(descriptor_pool_, dynamic.descriptor_layout,
                            dynamic.descriptor_set, last_error_))
                        {
                            batch_vertices_.clear();
                            return;
                        }
                    }
                    if (!images.empty() && !context_.update_dynamic_descriptor_set(dynamic.descriptor_set, images, samplers, last_error_))
                    {
                        batch_vertices_.clear();
                        return;
                    }
                    const std::size_t pipeline_index = static_cast<std::size_t>(primitive);
                    if (pipeline_index >= dynamic.pipelines.size())
                    {
                        batch_vertices_.clear();
                        return;
                    }
                    if (dynamic.pipelines[pipeline_index].pipeline == VK_NULL_HANDLE)
                    {
                        if (!context_.create_graphics_pipeline(dynamic.vertex_module, dynamic.fragment_module,
                            dynamic.descriptor_layout, primitive, dynamic.pipelines[pipeline_index], last_error_,
                            nullptr, dynamic.push_constant_size))
                        {
                            batch_vertices_.clear();
                            return;
                        }
                    }
                    context_.record_postprocess_draw(dynamic.pipelines[pipeline_index].pipeline,
                        dynamic.pipelines[pipeline_index].layout, buffer.buffer, dynamic.descriptor_set,
                        static_cast<std::uint32_t>(upload_count), dynamic.projection.data(),
                        dynamic.push_constants.empty() ? nullptr : dynamic.push_constants.data(),
                        static_cast<std::uint32_t>(dynamic.push_constants.size()), last_error_);
                    batch_vertices_.clear();
                    return;
                }
                if (program_kind(batch_program_) == BuiltinProgram::lighting)
                {
                    lighting_textures_[0] = texture_handle;
                    std::array<VulkanImage, 9> images{};
                    std::array<VulkanSampler, 9> samplers{};
                    for (std::size_t index = 0; index < lighting_textures_.size(); ++index)
                    {
                        if (!resolve_registered_texture(lighting_textures_[index], images[index], samplers[index]) &&
                            !resolve_registered_texture(white_texture_, images[index], samplers[index]))
                        {
                            batch_vertices_.clear();
                            return;
                        }
                    }
                    const bool descriptor_ready = lighting_descriptor_ == VK_NULL_HANDLE
                        ? context_.allocate_lighting_descriptor(descriptor_pool_, lighting_descriptor_layout_,
                            images.data(), samplers.data(), lighting_descriptor_, last_error_)
                        : context_.update_lighting_descriptor(lighting_descriptor_, images.data(), samplers.data(), last_error_);
                    if (!descriptor_ready || storage_descriptor_ == VK_NULL_HANDLE)
                    {
                        batch_vertices_.clear();
                        return;
                    }
                    context_.record_lighting_draw(lighting_pipeline_.pipeline, lighting_pipeline_.layout,
                        buffer.buffer, lighting_descriptor_, storage_descriptor_, static_cast<std::uint32_t>(upload_count),
                        lighting_projection_.data(), &lighting_constants_, sizeof(lighting_constants_), last_error_);
                    batch_vertices_.clear();
                    return;
                }
                if (program_kind(batch_program_) == BuiltinProgram::composite)
                {
                    VulkanImage images[2]{};
                    VulkanSampler samplers[2]{};
                    const std::uint32_t bloom_handle = composite_bloom_texture_ != 0 ? composite_bloom_texture_ : white_texture_;
                    if (!resolve_registered_texture(texture_handle, images[0], samplers[0]) ||
                        !resolve_registered_texture(bloom_handle, images[1], samplers[1]))
                    {
                        batch_vertices_.clear();
                        return;
                    }
                    const bool descriptor_ready = composite_descriptor_ == VK_NULL_HANDLE
                        ? context_.allocate_composite_descriptor(descriptor_pool_, composite_descriptor_layout_,
                            images, samplers, composite_descriptor_, last_error_)
                        : context_.update_composite_descriptor(composite_descriptor_, images, samplers, last_error_);
                    if (!descriptor_ready)
                    {
                        batch_vertices_.clear();
                        return;
                    }
                    context_.record_postprocess_draw(composite_effect_.pipeline.pipeline, composite_effect_.pipeline.layout,
                        buffer.buffer, composite_descriptor_, static_cast<std::uint32_t>(upload_count),
                        postprocess_projection_.data(), &composite_constants_, sizeof(composite_constants_), last_error_);
                    batch_vertices_.clear();
                    return;
                }
                VkDescriptorSet descriptor = VK_NULL_HANDLE;
                const auto texture_iterator = textures_.find(texture_handle);
                if (texture_iterator != textures_.end())
                {
                    descriptor = texture_iterator->second.descriptor;
                    VulkanImage image = texture_iterator->second.image;
                    VulkanSampler sampler = texture_iterator->second.sampler;
                    if (!prepare_texture(image))
                    {
                        batch_vertices_.clear();
                        return;
                    }
                }
                else
                {
                    for (const auto &[handle, target] : render_targets_)
                    {
                        if (target.texture_handle == texture_handle)
                        {
                            VulkanImage image = target.image;
                            VulkanSampler sampler = target.sampler;
                            if (!prepare_texture(image))
                            {
                                batch_vertices_.clear();
                                return;
                            }
                            descriptor = target.descriptor;
                        }
                    }
                }
                if (descriptor == VK_NULL_HANDLE)
                {
                    batch_vertices_.clear();
                    return;
                }
                if (batch_program_ == vignette_effect_.program)
                {
                    context_.record_postprocess_draw(vignette_effect_.pipeline.pipeline, vignette_effect_.pipeline.layout,
                        buffer.buffer, descriptor, static_cast<std::uint32_t>(upload_count), vignette_projection_.data(),
                        &vignette_effect_.constants, sizeof(vignette_effect_.constants), last_error_);
                    batch_vertices_.clear();
                    return;
                }
                if (batch_program_ == colour_adjust_effect_.program)
                {
                    context_.record_postprocess_draw(colour_adjust_effect_.pipeline.pipeline, colour_adjust_effect_.pipeline.layout,
                        buffer.buffer, descriptor, static_cast<std::uint32_t>(upload_count), postprocess_projection_.data(),
                        &colour_adjust_constants_, sizeof(colour_adjust_constants_), last_error_);
                    batch_vertices_.clear();
                    return;
                }
                if (batch_program_ == chromatic_effect_.program)
                {
                    context_.record_postprocess_draw(chromatic_effect_.pipeline.pipeline, chromatic_effect_.pipeline.layout,
                        buffer.buffer, descriptor, static_cast<std::uint32_t>(upload_count), postprocess_projection_.data(),
                        &chromatic_constants_, sizeof(chromatic_constants_), last_error_);
                    batch_vertices_.clear();
                    return;
                }
                if (program_kind(batch_program_) == BuiltinProgram::pixelate)
                {
                    context_.record_postprocess_draw(pixelate_effect_.pipeline.pipeline, pixelate_effect_.pipeline.layout,
                        buffer.buffer, descriptor, static_cast<std::uint32_t>(upload_count), postprocess_projection_.data(),
                        &pixelate_constants_, sizeof(pixelate_constants_), last_error_);
                    batch_vertices_.clear();
                    return;
                }
                if (program_kind(batch_program_) == BuiltinProgram::radial)
                {
                    context_.record_postprocess_draw(radial_effect_.pipeline.pipeline, radial_effect_.pipeline.layout,
                        buffer.buffer, descriptor, static_cast<std::uint32_t>(upload_count), postprocess_projection_.data(),
                        &radial_constants_, sizeof(radial_constants_), last_error_);
                    batch_vertices_.clear();
                    return;
                }
                if (program_kind(batch_program_) == BuiltinProgram::heat)
                {
                    context_.record_postprocess_draw(heat_effect_.pipeline.pipeline, heat_effect_.pipeline.layout,
                        buffer.buffer, descriptor, static_cast<std::uint32_t>(upload_count), postprocess_projection_.data(),
                        &heat_constants_, sizeof(heat_constants_), last_error_);
                    batch_vertices_.clear();
                    return;
                }
                if (program_kind(batch_program_) == BuiltinProgram::shockwave)
                {
                    context_.record_postprocess_draw(shockwave_effect_.pipeline.pipeline, shockwave_effect_.pipeline.layout,
                        buffer.buffer, descriptor, static_cast<std::uint32_t>(upload_count), postprocess_projection_.data(),
                        &shockwave_constants_, sizeof(shockwave_constants_), last_error_);
                    batch_vertices_.clear();
                    return;
                }
                if (program_kind(batch_program_) == BuiltinProgram::crt)
                {
                    context_.record_postprocess_draw(crt_effect_.pipeline.pipeline, crt_effect_.pipeline.layout,
                        buffer.buffer, descriptor, static_cast<std::uint32_t>(upload_count), postprocess_projection_.data(),
                        &crt_constants_, sizeof(crt_constants_), last_error_);
                    batch_vertices_.clear();
                    return;
                }
                if (program_kind(batch_program_) == BuiltinProgram::dither)
                {
                    context_.record_postprocess_draw(dither_effect_.pipeline.pipeline, dither_effect_.pipeline.layout,
                        buffer.buffer, descriptor, static_cast<std::uint32_t>(upload_count), postprocess_projection_.data(),
                        &dither_constants_, sizeof(dither_constants_), last_error_);
                    batch_vertices_.clear();
                    return;
                }
                if (program_kind(batch_program_) == BuiltinProgram::film_grain)
                {
                    context_.record_postprocess_draw(film_grain_effect_.pipeline.pipeline, film_grain_effect_.pipeline.layout,
                        buffer.buffer, descriptor, static_cast<std::uint32_t>(upload_count), postprocess_projection_.data(),
                        &film_grain_constants_, sizeof(film_grain_constants_), last_error_);
                    batch_vertices_.clear();
                    return;
                }
                if (program_kind(batch_program_) == BuiltinProgram::bright)
                {
                    context_.record_postprocess_draw(bright_effect_.pipeline.pipeline, bright_effect_.pipeline.layout,
                        buffer.buffer, descriptor, static_cast<std::uint32_t>(upload_count), postprocess_projection_.data(),
                        &bright_constants_, sizeof(bright_constants_), last_error_);
                    batch_vertices_.clear();
                    return;
                }
                if (program_kind(batch_program_) == BuiltinProgram::blur)
                {
                    context_.record_postprocess_draw(blur_effect_.pipeline.pipeline, blur_effect_.pipeline.layout,
                        buffer.buffer, descriptor, static_cast<std::uint32_t>(upload_count), postprocess_projection_.data(),
                        &blur_constants_, sizeof(blur_constants_), last_error_);
                    batch_vertices_.clear();
                    return;
                }
                const std::size_t pipeline_index = static_cast<std::size_t>(primitive);
                if (pipeline_index >= pipelines_.size())
                {
                    batch_vertices_.clear();
                    return;
                }
                context_.record_vertex_draw(pipelines_[pipeline_index].pipeline, pipelines_[pipeline_index].layout, buffer.buffer, descriptor,
                    static_cast<std::uint32_t>(upload_count), primitive, projection_.data(), last_error_);
                batch_vertices_.clear();
            }
            void submit_2d(PrimitiveType primitive, const Vertex2D *vertices, int count, std::uint32_t texture) override
            {
                if (!vertices || count <= 0 || !frame_active_) return;
                const std::uint32_t resolved_texture = (texture != 0 ? texture : white_texture_);
                const PrimitiveType target_primitive = get_target_batch_primitive(primitive);
                const int new_vertex_count = get_decomposed_vertex_count(primitive, count);
                if (new_vertex_count <= 0) return;

                if (!batch_vertices_.empty())
                {
                    if (batch_texture_ != resolved_texture ||
                        batch_primitive_ != target_primitive ||
                        batch_program_ != active_program_ ||
                        batch_vertices_.size() + static_cast<std::size_t>(new_vertex_count) > MAX_BATCH_VERTICES)
                    {
                        flush_2d();
                    }
                }

                if (batch_vertices_.empty())
                {
                    batch_texture_ = resolved_texture;
                    batch_primitive_ = target_primitive;
                    batch_program_ = active_program_;
                }

                PrimitiveType actual_primitive = batch_primitive_;
                append_decomposed_vertices(primitive, vertices, count, actual_primitive, batch_vertices_);
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
                flush_2d();
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
                flush_2d();
                if (binding < 2 || binding > 4) return;
                storage_handles_[binding - 2] = buffer;
                update_storage_descriptor();
            }
            void bind_texture_unit(unsigned int unit, std::uint32_t texture) override
            {
                flush_2d();
                const auto dynamic_iterator = dynamic_programs_.find(active_program_);
                if (dynamic_iterator != dynamic_programs_.end())
                {
                    if (unit < dynamic_iterator->second.bound_textures.size())
                        dynamic_iterator->second.bound_textures[unit] = texture;
                    return;
                }
                if (program_kind(active_program_) == BuiltinProgram::composite && unit == 1)
                {
                    composite_bloom_texture_ = texture;
                    return;
                }
                if (unit < lighting_textures_.size()) lighting_textures_[unit] = texture;
            }
            void storage_barrier() override
            {
                flush_2d();
                // record_compute_dispatch already emits the required compute-to-fragment barrier.
            }

        private:
            enum class BuiltinProgram
            {
                unknown,
                lighting,
                vignette,
                colour_adjust,
                chromatic,
                pixelate,
                radial,
                heat,
                shockwave,
                crt,
                dither,
                film_grain,
                bright,
                blur,
                composite,
            };

            static BuiltinProgram program_kind(std::uint32_t program)
            {
                if (program == lighting_program_) return BuiltinProgram::lighting;
                if (program == vignette_program_) return BuiltinProgram::vignette;
                if (program == colour_adjust_program_) return BuiltinProgram::colour_adjust;
                if (program == chromatic_program_) return BuiltinProgram::chromatic;
                if (program == pixelate_program_) return BuiltinProgram::pixelate;
                if (program == radial_program_) return BuiltinProgram::radial;
                if (program == heat_program_) return BuiltinProgram::heat;
                if (program == shockwave_program_) return BuiltinProgram::shockwave;
                if (program == crt_program_) return BuiltinProgram::crt;
                if (program == dither_program_) return BuiltinProgram::dither;
                if (program == film_grain_program_) return BuiltinProgram::film_grain;
                if (program == bright_program_) return BuiltinProgram::bright;
                if (program == blur_program_) return BuiltinProgram::blur;
                if (program == composite_program_) return BuiltinProgram::composite;
                return BuiltinProgram::unknown;
            }

            bool set_blur_uniform(const char *name, float value)
            {
                struct Field
                {
                    const char *name;
                    enum class Kind { radius, opacity } kind;
                };
                static constexpr Field fields[] = {
                    {"radius", Field::Kind::radius},
                    {"opacity", Field::Kind::opacity},
                };
                for (const Field &field : fields)
                {
                    if (std::strcmp(name, field.name) != 0) continue;
                    if (field.kind == Field::Kind::radius) blur_constants_.radius = value;
                    else blur_constants_.opacity = value;
                    return true;
                }
                return unsupported();
            }

            bool set_blur_uniform(const char *name, int value)
            {
                struct Field
                {
                    const char *name;
                    enum class Kind { premultiplied_source, premultiplied_output } kind;
                };
                static constexpr Field fields[] = {
                    {"premultipliedSource", Field::Kind::premultiplied_source},
                    {"premultipliedOutput", Field::Kind::premultiplied_output},
                };
                for (const Field &field : fields)
                {
                    if (std::strcmp(name, field.name) != 0) continue;
                    if (field.kind == Field::Kind::premultiplied_source)
                        blur_constants_.premultiplied_source = value;
                    else
                        blur_constants_.premultiplied_output = value;
                    return true;
                }
                return unsupported();
            }

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

            bool create_effect_pipelines(std::string &error)
            {
                struct EffectSpec
                {
                    BuiltinEffect *effect;
                    const VulkanShaderModule *fragment;
                    const VulkanDescriptorSetLayout *descriptors;
                    std::uint32_t push_constant_size;
                    bool premultiplied_alpha;
                };
                const EffectSpec effects[] = {
                    {&vignette_effect_, &vignette_fragment_module_, &descriptor_layout_, sizeof(BuiltinEffect::VignetteConstants), false},
                    {&colour_adjust_effect_, &colour_adjust_fragment_module_, &descriptor_layout_, sizeof(ColourAdjustConstants), false},
                    {&pixelate_effect_, &pixelate_fragment_module_, &descriptor_layout_, sizeof(PixelateConstants), false},
                    {&radial_effect_, &radial_fragment_module_, &descriptor_layout_, sizeof(RadialConstants), false},
                    {&heat_effect_, &heat_fragment_module_, &descriptor_layout_, sizeof(HeatConstants), false},
                    {&shockwave_effect_, &shockwave_fragment_module_, &descriptor_layout_, sizeof(ShockwaveConstants), false},
                    {&chromatic_effect_, &chromatic_fragment_module_, &descriptor_layout_, sizeof(ChromaticConstants), false},
                    {&crt_effect_, &crt_fragment_module_, &descriptor_layout_, sizeof(CRTConstants), false},
                    {&dither_effect_, &dither_fragment_module_, &descriptor_layout_, sizeof(DitherConstants), false},
                    {&film_grain_effect_, &film_grain_fragment_module_, &descriptor_layout_, sizeof(FilmGrainConstants), false},
                    {&bright_effect_, &bright_fragment_module_, &descriptor_layout_, sizeof(BrightConstants), false},
                    {&blur_effect_, &blur_fragment_module_, &descriptor_layout_, sizeof(BlurConstants), true},
                    {&composite_effect_, &composite_fragment_module_, &composite_descriptor_layout_, sizeof(CompositeConstants), false},
                };
                for (const EffectSpec &effect : effects)
                {
                    if (!context_.create_graphics_pipeline(lighting_vertex_module_, *effect.fragment,
                        *effect.descriptors, PrimitiveType::triangle_fan, effect.effect->pipeline, error,
                        nullptr, effect.push_constant_size, false, effect.premultiplied_alpha))
                        return false;
                }
                return true;
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
            struct BuiltinEffect
            {
                struct VignetteConstants { float radius = 0.0f, softness = 0.0f, intensity = 0.0f; };
                std::uint32_t program = 0;
                VulkanGraphicsPipeline pipeline;
                VignetteConstants constants;
            };
            BuiltinEffect vignette_effect_{vignette_program_, {}};
            VulkanShaderModule colour_adjust_fragment_module_;
            BuiltinEffect colour_adjust_effect_{colour_adjust_program_, {}};
            VulkanShaderModule chromatic_fragment_module_;
            BuiltinEffect chromatic_effect_{chromatic_program_, {}};
            VulkanShaderModule pixelate_fragment_module_;
            BuiltinEffect pixelate_effect_{pixelate_program_, {}};
            VulkanShaderModule radial_fragment_module_;
            BuiltinEffect radial_effect_{radial_program_, {}};
            VulkanShaderModule heat_fragment_module_;
            BuiltinEffect heat_effect_{heat_program_, {}};
            VulkanShaderModule shockwave_fragment_module_;
            BuiltinEffect shockwave_effect_{shockwave_program_, {}};
            VulkanShaderModule crt_fragment_module_;
            BuiltinEffect crt_effect_{crt_program_, {}};
            VulkanShaderModule dither_fragment_module_;
            BuiltinEffect dither_effect_{dither_program_, {}};
            VulkanShaderModule film_grain_fragment_module_;
            BuiltinEffect film_grain_effect_{film_grain_program_, {}};
            VulkanShaderModule bright_fragment_module_;
            BuiltinEffect bright_effect_{bright_program_, {}};
            VulkanShaderModule blur_fragment_module_;
            BuiltinEffect blur_effect_{blur_program_, {}};
            VulkanShaderModule composite_fragment_module_;
            BuiltinEffect composite_effect_{composite_program_, {}};
            VulkanDescriptorSetLayout composite_descriptor_layout_;
            VkDescriptorSet composite_descriptor_ = VK_NULL_HANDLE;
            std::uint32_t composite_bloom_texture_ = 0;
            static constexpr std::uint32_t cull_program_ = 0x80000000u;
            static constexpr std::uint32_t lighting_program_ = 0x80000001u;
            static constexpr std::uint32_t vignette_program_ = 0x80000002u;
            static constexpr std::uint32_t colour_adjust_program_ = 0x80000006u;
            static constexpr std::uint32_t chromatic_program_ = 0x80000007u;
            static constexpr std::uint32_t pixelate_program_ = 0x80000008u;
            static constexpr std::uint32_t radial_program_ = 0x80000009u;
            static constexpr std::uint32_t heat_program_ = 0x8000000Au;
            static constexpr std::uint32_t shockwave_program_ = 0x8000000Du;
            static constexpr std::uint32_t crt_program_ = 0x8000000Bu;
            static constexpr std::uint32_t dither_program_ = 0x8000000Cu;
            static constexpr std::uint32_t film_grain_program_ = 0x8000000Eu;
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
            // Matches the VulkanContext default above.
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
            struct ColourAdjustConstants { float values[4] = {0.0f, 1.0f, 1.0f, 0.0f}; } colour_adjust_constants_;
            struct ChromaticConstants { float strength = 0.0f; } chromatic_constants_;
            struct PixelateConstants { float resolution[2] = {0.0f, 0.0f}; float pixel_size[2] = {8.0f, 8.0f}; } pixelate_constants_;
            struct RadialConstants { float centre[2] = {0.5f, 0.5f}; float strength = 0.25f; int samples = 8; } radial_constants_;
            struct HeatConstants { float strength = 0.008f; float frequency = 24.0f; float time = 0.0f; } heat_constants_;
            struct ShockwaveConstants { float centre[2] = {0.5f, 0.5f}; float radius = 0.25f; float width = 0.08f; float strength = 0.025f; } shockwave_constants_;
            struct CRTConstants { float resolution[2] = {0.0f, 0.0f}; float pixel_size = 4.0f; float scanline_strength = 0.35f; float curvature = 0.18f; } crt_constants_;
            struct DitherConstants { float resolution[2] = {0.0f, 0.0f}; float pixel_size = 1.0f; float levels = 4.0f; } dither_constants_;
            struct FilmGrainConstants { float strength = 0.08f; float time = 0.0f; } film_grain_constants_;
            std::array<float, 16> vignette_projection_{};
            struct BrightConstants { float threshold = 0.0f; } bright_constants_;
            struct BlurConstants { float texel[2] = {0.0f, 0.0f}; float direction[2] = {0.0f, 0.0f}; float radius = 0.0f; int premultiplied_source = 0; float opacity = 1.0f; int premultiplied_output = 0; } blur_constants_;
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
                VulkanImage depth;
                VulkanSampler sampler;
                VkDescriptorSet descriptor = VK_NULL_HANDLE;
                VkFramebuffer framebuffer = VK_NULL_HANDLE;
                std::uint32_t texture_handle = 0;
            };
            std::unordered_map<std::uint32_t, VulkanTexture> textures_;
            std::vector<VulkanTexture> retired_textures_;
            std::map<std::uint32_t, VulkanRenderTarget> render_targets_;
            std::vector<std::uint32_t> active_render_targets_;
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

            static constexpr std::size_t MAX_BATCH_VERTICES = 65536;
            std::vector<Vertex2D> batch_vertices_;
            PrimitiveType batch_primitive_ = PrimitiveType::triangles;
            std::uint32_t batch_texture_ = 0;
            std::uint32_t batch_program_ = 0;
        };
    }

    std::unique_ptr<Renderer> create_vulkan_renderer()
    {
        return std::make_unique<VulkanRenderer>();
    }
}

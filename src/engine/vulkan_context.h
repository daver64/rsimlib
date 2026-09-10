#pragma once

#include <SDL2/SDL_video.h>
#include <vulkan/vulkan.h>

#include "renderer.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace sl::detail
{
    struct VulkanBuffer
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
    };

    struct VulkanImage
    {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    struct VulkanShaderModule
    {
        VkShaderModule module = VK_NULL_HANDLE;
    };

    struct VulkanDescriptorSetLayout
    {
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    };

    struct VulkanStorageDescriptorLayout
    {
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    };

    struct VulkanDescriptorPool
    {
        VkDescriptorPool pool = VK_NULL_HANDLE;
    };

    struct VulkanSampler
    {
        VkSampler sampler = VK_NULL_HANDLE;
    };

    struct VulkanGraphicsPipeline
    {
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
    };

    struct VulkanComputePipeline
    {
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
    };

    /** Owns the Vulkan instance, selected physical device, logical device, and graphics queue. */
    class VulkanContext
    {
    public:
        VulkanContext() = default;
        ~VulkanContext();

        VulkanContext(const VulkanContext &) = delete;
        VulkanContext &operator=(const VulkanContext &) = delete;

        bool initialise(SDL_Window *window, std::string &error);
        bool recreate_swapchain(int width, int height, std::string &error);
        void set_vsync_enabled(bool enabled) { vsync_enabled_ = enabled; }
        void destroy_swapchain();
        bool begin_frame(std::string &error);
        bool end_frame(std::string &error);
        bool begin_offscreen_render_pass(VkFramebuffer framebuffer, int width, int height,
                         std::string &error);
        bool end_offscreen_render_pass(std::string &error);
        bool record_vertex_draw(VkPipeline pipeline, VkPipelineLayout layout,
                    VkBuffer vertex_buffer, VkDescriptorSet descriptor_set,
                    std::uint32_t vertex_count, PrimitiveType topology,
                    const float *projection, std::string &error);
        bool record_lighting_draw(VkPipeline pipeline, VkPipelineLayout layout,
                VkBuffer vertex_buffer, VkDescriptorSet texture_descriptor,
                VkDescriptorSet storage_descriptor, std::uint32_t vertex_count,
                const float *projection, const void *lighting_constants,
                std::uint32_t lighting_constant_size, std::string &error);
        bool record_postprocess_draw(VkPipeline pipeline, VkPipelineLayout layout,
                VkBuffer vertex_buffer, VkDescriptorSet texture_descriptor,
                std::uint32_t vertex_count, const float *projection,
                const void *constants, std::uint32_t constant_size, std::string &error);
        bool clear_active_frame(float red, float green, float blue, float alpha,
                                std::string &error);
        bool record_compute_dispatch(VkPipeline pipeline, VkPipelineLayout layout,
                         VkDescriptorSet descriptor_set, const void *push_constants,
                         std::uint32_t push_constant_size,
                         std::uint32_t groups_x, std::uint32_t groups_y,
                         std::uint32_t groups_z, std::string &error);
        bool create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                   VkMemoryPropertyFlags properties, VulkanBuffer &result,
                   std::string &error);
        void destroy_buffer(VulkanBuffer &buffer);
        bool upload_buffer(const VulkanBuffer &buffer, const void *data, std::size_t size,
                   std::string &error);
        bool upload_image_rgba(VulkanImage &image, int width, int height,
                       const std::uint8_t *pixels, std::string &error);
        bool create_image(int width, int height, VkFormat format, VkImageUsageFlags usage,
                  VulkanImage &result, std::string &error);
        void destroy_image(VulkanImage &image);
        bool create_render_target_framebuffer(const VulkanImage &image, int width, int height,
                              VkFramebuffer &framebuffer, std::string &error);
        void destroy_framebuffer(VkFramebuffer &framebuffer);
        bool load_shader_module(const std::filesystem::path &path, VulkanShaderModule &result,
                    std::string &error);
        bool create_shader_module(const std::vector<std::uint32_t> &code,
                      VulkanShaderModule &result, std::string &error);
        void destroy_shader_module(VulkanShaderModule &module);
        bool create_texture_descriptor_layout(VulkanDescriptorSetLayout &result, std::string &error);
        bool create_lighting_descriptor_layout(VulkanDescriptorSetLayout &result, std::string &error);
        void destroy_descriptor_set_layout(VulkanDescriptorSetLayout &layout);
        bool create_descriptor_pool(VulkanDescriptorPool &result, std::string &error);
        void destroy_descriptor_pool(VulkanDescriptorPool &pool);
        bool create_sampler(TextureFilter filter, VulkanSampler &result, std::string &error);
        void destroy_sampler(VulkanSampler &sampler);
        bool allocate_texture_descriptor(const VulkanDescriptorPool &pool,
                         const VulkanDescriptorSetLayout &layout,
                         const VulkanImage &image, const VulkanSampler &sampler,
                         VkDescriptorSet &set, std::string &error);
        void free_descriptor_set(const VulkanDescriptorPool &pool, VkDescriptorSet &set);
        bool allocate_lighting_descriptor(const VulkanDescriptorPool &pool,
                 const VulkanDescriptorSetLayout &layout,
                 const VulkanImage *images, const VulkanSampler *samplers,
                 VkDescriptorSet &set, std::string &error);
        bool update_lighting_descriptor(VkDescriptorSet set, const VulkanImage *images,
                 const VulkanSampler *samplers, std::string &error);
        bool create_storage_descriptor_layout(VulkanStorageDescriptorLayout &result, std::string &error);
        void destroy_storage_descriptor_layout(VulkanStorageDescriptorLayout &layout);
        bool allocate_storage_descriptor(const VulkanDescriptorPool &pool,
                         const VulkanStorageDescriptorLayout &layout,
                         const VulkanBuffer *buffers, std::size_t count,
                         VkDescriptorSet &set, std::string &error);
        bool update_storage_descriptor(VkDescriptorSet set, const VulkanBuffer *buffers,
                 std::size_t count, std::string &error);
        bool create_imgui_descriptor_pool(VkDescriptorPool &pool, std::string &error);
        void destroy_imgui_descriptor_pool(VkDescriptorPool &pool);
        void bind_storage_descriptor(VkCommandBuffer command_buffer, VkPipelineLayout layout,
                         VkDescriptorSet descriptor_set, VkPipelineBindPoint bind_point);
        void storage_barrier(VkCommandBuffer command_buffer);
        bool create_pipeline_layout(const VulkanDescriptorSetLayout *descriptor_layout,
                        VkPipelineLayout &layout, std::string &error);
        void destroy_pipeline_layout(VkPipelineLayout &layout);
        bool create_graphics_pipeline(const VulkanShaderModule &vertex,
                          const VulkanShaderModule &fragment,
                          const VulkanDescriptorSetLayout &descriptor_layout,
                          PrimitiveType topology,
                          VulkanGraphicsPipeline &result, std::string &error,
                          const VulkanStorageDescriptorLayout *storage_layout = nullptr,
                          std::uint32_t fragment_push_constant_size = 0);
        void destroy_graphics_pipeline(VulkanGraphicsPipeline &pipeline);
            bool create_compute_pipeline(const VulkanShaderModule &compute,
                                         const VulkanStorageDescriptorLayout &descriptor_layout,
                             std::uint32_t push_constant_size,
                             VulkanComputePipeline &result, std::string &error);
            void destroy_compute_pipeline(VulkanComputePipeline &pipeline);
        void shutdown();
        bool is_valid() const;
        VkInstance instance() const { return instance_; }
        VkPhysicalDevice physical_device() const { return physical_device_; }
        VkDevice device() const { return device_; }
        VkQueue graphics_queue() const { return graphics_queue_; }
        std::uint32_t graphics_queue_family() const { return graphics_queue_family_; }
        VkRenderPass render_pass() const { return render_pass_; }
        VkFormat render_target_format() const { return swapchain_format_; }
        VkCommandBuffer command_buffer() const { return command_buffer_; }
        std::uint32_t swapchain_image_count() const { return static_cast<std::uint32_t>(framebuffers_.size()); }

    private:
        VkInstance instance_ = VK_NULL_HANDLE;
        VkSurfaceKHR surface_ = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
        VkDevice device_ = VK_NULL_HANDLE;
        VkQueue graphics_queue_ = VK_NULL_HANDLE;
        unsigned int graphics_queue_family_ = 0;
        VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
        VkFormat swapchain_format_ = VK_FORMAT_UNDEFINED;
        VkExtent2D swapchain_extent_{};
        std::vector<VkImage> swapchain_images_;
        std::vector<VkImageView> swapchain_image_views_;
        VkCommandPool command_pool_ = VK_NULL_HANDLE;
        VkRenderPass render_pass_ = VK_NULL_HANDLE;
        VkRenderPass resume_render_pass_ = VK_NULL_HANDLE;
        VkRenderPass offscreen_render_pass_ = VK_NULL_HANDLE;
        VkRenderPass resume_offscreen_render_pass_ = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> framebuffers_;
        VkCommandBuffer command_buffer_ = VK_NULL_HANDLE;
        VkSemaphore image_available_ = VK_NULL_HANDLE;
        VkSemaphore render_finished_ = VK_NULL_HANDLE;
        VkFence in_flight_ = VK_NULL_HANDLE;
        std::uint32_t current_image_ = 0;
        VkExtent2D active_extent_{};
        VkRenderPass active_render_pass_ = VK_NULL_HANDLE;
        VkRenderPass active_resume_render_pass_ = VK_NULL_HANDLE;
        VkFramebuffer active_framebuffer_ = VK_NULL_HANDLE;
        std::vector<VulkanBuffer> upload_staging_buffers_;
        bool frame_active_ = false;
        bool command_buffer_recording_ = false;
        bool render_pass_active_ = false;
        bool offscreen_active_ = false;
        bool vsync_enabled_ = true;

        std::uint32_t find_memory_type(std::uint32_t type_filter,
                           VkMemoryPropertyFlags properties) const;
    };
}

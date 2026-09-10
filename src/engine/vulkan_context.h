#pragma once

#include <SDL2/SDL_video.h>
#include <vulkan/vulkan.h>

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
    };

    struct VulkanShaderModule
    {
        VkShaderModule module = VK_NULL_HANDLE;
    };

    struct VulkanDescriptorSetLayout
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
        void destroy_swapchain();
        bool begin_frame(std::string &error);
        bool end_frame(std::string &error);
        bool record_vertex_draw(VkPipeline pipeline, VkPipelineLayout layout,
                    VkBuffer vertex_buffer, VkDescriptorSet descriptor_set,
                    std::uint32_t vertex_count, VkPrimitiveTopology topology,
                                const float *projection,
                    std::string &error);
        bool create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                   VkMemoryPropertyFlags properties, VulkanBuffer &result,
                   std::string &error);
        void destroy_buffer(VulkanBuffer &buffer);
        bool upload_buffer(const VulkanBuffer &buffer, const void *data, std::size_t size,
                   std::string &error);
        bool upload_image_rgba(const VulkanImage &image, int width, int height,
                       const std::uint8_t *pixels, std::string &error);
        bool create_image(int width, int height, VkFormat format, VkImageUsageFlags usage,
                  VulkanImage &result, std::string &error);
        void destroy_image(VulkanImage &image);
        bool create_render_target_framebuffer(const VulkanImage &image, int width, int height,
                              VkFramebuffer &framebuffer, std::string &error);
        void destroy_framebuffer(VkFramebuffer &framebuffer);
        bool load_shader_module(const std::filesystem::path &path, VulkanShaderModule &result,
                    std::string &error);
        void destroy_shader_module(VulkanShaderModule &module);
        bool create_texture_descriptor_layout(VulkanDescriptorSetLayout &result, std::string &error);
        void destroy_descriptor_set_layout(VulkanDescriptorSetLayout &layout);
        bool create_descriptor_pool(VulkanDescriptorPool &result, std::string &error);
        void destroy_descriptor_pool(VulkanDescriptorPool &pool);
        bool create_sampler(VulkanSampler &result, std::string &error);
        void destroy_sampler(VulkanSampler &sampler);
        bool allocate_texture_descriptor(const VulkanDescriptorPool &pool,
                         const VulkanDescriptorSetLayout &layout,
                         const VulkanImage &image, const VulkanSampler &sampler,
                         VkDescriptorSet &set, std::string &error);
        bool create_pipeline_layout(const VulkanDescriptorSetLayout *descriptor_layout,
                        VkPipelineLayout &layout, std::string &error);
        void destroy_pipeline_layout(VkPipelineLayout &layout);
        bool create_graphics_pipeline(const VulkanShaderModule &vertex,
                          const VulkanShaderModule &fragment,
                          const VulkanDescriptorSetLayout &descriptor_layout,
                          VkPrimitiveTopology topology,
                          VulkanGraphicsPipeline &result, std::string &error);
        void destroy_graphics_pipeline(VulkanGraphicsPipeline &pipeline);
        void shutdown();
        bool is_valid() const;

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
        std::vector<VkFramebuffer> framebuffers_;
        VkCommandBuffer command_buffer_ = VK_NULL_HANDLE;
        VkSemaphore image_available_ = VK_NULL_HANDLE;
        VkSemaphore render_finished_ = VK_NULL_HANDLE;
        VkFence in_flight_ = VK_NULL_HANDLE;
        std::uint32_t current_image_ = 0;
        bool frame_active_ = false;

        std::uint32_t find_memory_type(std::uint32_t type_filter,
                           VkMemoryPropertyFlags properties) const;
    };
}

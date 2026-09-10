#pragma once

#include <SDL2/SDL_video.h>
#include <vulkan/vulkan.h>

#include <cstdint>
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
        bool create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                   VkMemoryPropertyFlags properties, VulkanBuffer &result,
                   std::string &error);
        void destroy_buffer(VulkanBuffer &buffer);
        bool upload_buffer(const VulkanBuffer &buffer, const void *data, std::size_t size,
                   std::string &error);
        bool create_image(int width, int height, VkFormat format, VkImageUsageFlags usage,
                  VulkanImage &result, std::string &error);
        void destroy_image(VulkanImage &image);
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

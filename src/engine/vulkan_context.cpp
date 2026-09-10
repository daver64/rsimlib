#include "vulkan_context.h"

#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace sl::detail
{
    namespace
    {
        bool has_extension(const std::vector<VkExtensionProperties> &extensions, const char *name)
        {
            return std::any_of(extensions.begin(), extensions.end(), [name](const auto &extension)
            {
                return std::strcmp(extension.extensionName, name) == 0;
            });
        }
    }

    VulkanContext::~VulkanContext()
    {
        shutdown();
    }

    bool VulkanContext::initialise(SDL_Window *window, std::string &error)
    {
        shutdown();
        if (!window || (SDL_GetWindowFlags(window) & SDL_WINDOW_VULKAN) == 0)
        {
            error = "Vulkan requires an SDL_WINDOW_VULKAN window.";
            return false;
        }

        unsigned int extension_count = 0;
        if (!SDL_Vulkan_GetInstanceExtensions(window, &extension_count, nullptr))
        {
            error = SDL_GetError();
            return false;
        }
        std::vector<const char *> extensions(extension_count);
        if (!SDL_Vulkan_GetInstanceExtensions(window, &extension_count, extensions.data()))
        {
            error = SDL_GetError();
            return false;
        }

        unsigned int available_extension_count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &available_extension_count, nullptr);
        std::vector<VkExtensionProperties> available_extensions(available_extension_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &available_extension_count, available_extensions.data());
        for (const char *extension : extensions)
        {
            if (!has_extension(available_extensions, extension))
            {
                error = std::string("Missing Vulkan instance extension: ") + extension;
                return false;
            }
        }

        VkApplicationInfo application_info{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        application_info.pApplicationName = "simlib";
        application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        application_info.pEngineName = "simlib";
        application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        application_info.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo instance_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        instance_info.pApplicationInfo = &application_info;
        instance_info.enabledExtensionCount = static_cast<unsigned int>(extensions.size());
        instance_info.ppEnabledExtensionNames = extensions.data();
        if (vkCreateInstance(&instance_info, nullptr, &instance_) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan instance.";
            return false;
        }

        if (!SDL_Vulkan_CreateSurface(window, instance_, &surface_))
        {
            error = SDL_GetError();
            shutdown();
            return false;
        }

        unsigned int device_count = 0;
        vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);
        if (device_count == 0)
        {
            error = "No Vulkan physical devices are available.";
            shutdown();
            return false;
        }
        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());
        for (VkPhysicalDevice candidate : devices)
        {
            unsigned int queue_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queue_count, nullptr);
            std::vector<VkQueueFamilyProperties> queues(queue_count);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queue_count, queues.data());
            for (unsigned int index = 0; index < queue_count; ++index)
            {
                VkBool32 supports_surface = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(candidate, index, surface_, &supports_surface);
                if ((queues[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && supports_surface == VK_TRUE)
                {
                    physical_device_ = candidate;
                    graphics_queue_family_ = index;
                    break;
                }
            }
            if (physical_device_ != VK_NULL_HANDLE) break;
        }
        if (physical_device_ == VK_NULL_HANDLE)
        {
            error = "No Vulkan graphics queue with presentation support is available.";
            shutdown();
            return false;
        }

        constexpr float queue_priority = 1.0f;
        VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue_info.queueFamilyIndex = graphics_queue_family_;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &queue_priority;
        VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        device_info.queueCreateInfoCount = 1;
        device_info.pQueueCreateInfos = &queue_info;
        if (vkCreateDevice(physical_device_, &device_info, nullptr, &device_) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan logical device.";
            shutdown();
            return false;
        }
        vkGetDeviceQueue(device_, graphics_queue_family_, 0, &graphics_queue_);
        int width = 0;
        int height = 0;
        SDL_Vulkan_GetDrawableSize(window, &width, &height);
        return recreate_swapchain(width, height, error);
    }

    bool VulkanContext::recreate_swapchain(int width, int height, std::string &error)
    {
        if (!is_valid() || width <= 0 || height <= 0)
        {
            error = "Invalid Vulkan swapchain dimensions or context.";
            return false;
        }
        destroy_swapchain();

        VkSurfaceCapabilitiesKHR capabilities{};
        if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &capabilities) != VK_SUCCESS)
        {
            error = "Unable to query Vulkan surface capabilities.";
            return false;
        }
        unsigned int format_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, formats.data());
        if (formats.empty())
        {
            error = "No Vulkan surface formats are available.";
            return false;
        }
        VkSurfaceFormatKHR surface_format = formats.front();
        for (const auto &format : formats)
        {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                surface_format = format;
                break;
            }
        }

        unsigned int present_mode_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &present_mode_count, nullptr);
        std::vector<VkPresentModeKHR> present_modes(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &present_mode_count, present_modes.data());
        VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
        if (std::find(present_modes.begin(), present_modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != present_modes.end())
        {
            present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
        }

        VkExtent2D extent{
            static_cast<std::uint32_t>(width),
            static_cast<std::uint32_t>(height)};
        if (capabilities.currentExtent.width != UINT32_MAX)
        {
            extent = capabilities.currentExtent;
        }
        const std::uint32_t image_count = std::clamp(
            capabilities.minImageCount + 1,
            capabilities.minImageCount,
            capabilities.maxImageCount == 0 ? UINT32_MAX : capabilities.maxImageCount);
        VkSwapchainCreateInfoKHR swapchain_info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        swapchain_info.surface = surface_;
        swapchain_info.minImageCount = image_count;
        swapchain_info.imageFormat = surface_format.format;
        swapchain_info.imageColorSpace = surface_format.colorSpace;
        swapchain_info.imageExtent = extent;
        swapchain_info.imageArrayLayers = 1;
        swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_info.preTransform = capabilities.currentTransform;
        swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_info.presentMode = present_mode;
        swapchain_info.clipped = VK_TRUE;
        if (vkCreateSwapchainKHR(device_, &swapchain_info, nullptr, &swapchain_) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan swapchain.";
            return false;
        }
        swapchain_format_ = surface_format.format;
        swapchain_extent_ = extent;

        unsigned int actual_image_count = 0;
        vkGetSwapchainImagesKHR(device_, swapchain_, &actual_image_count, nullptr);
        swapchain_images_.resize(actual_image_count);
        vkGetSwapchainImagesKHR(device_, swapchain_, &actual_image_count, swapchain_images_.data());
        swapchain_image_views_.resize(actual_image_count);
        for (std::size_t index = 0; index < swapchain_images_.size(); ++index)
        {
            VkImageViewCreateInfo view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            view_info.image = swapchain_images_[index];
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = swapchain_format_;
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view_info.subresourceRange.levelCount = 1;
            view_info.subresourceRange.layerCount = 1;
            if (vkCreateImageView(device_, &view_info, nullptr, &swapchain_image_views_[index]) != VK_SUCCESS)
            {
                error = "Unable to create Vulkan swapchain image view.";
                destroy_swapchain();
                return false;
            }
        }

        VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = graphics_queue_family_;
        if (vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan command pool.";
            destroy_swapchain();
            return false;
        }

        VkAttachmentDescription attachment{};
        attachment.format = swapchain_format_;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference color_reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_reference;
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo render_pass_info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        render_pass_info.attachmentCount = 1;
        render_pass_info.pAttachments = &attachment;
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;
        render_pass_info.dependencyCount = 1;
        render_pass_info.pDependencies = &dependency;
        if (vkCreateRenderPass(device_, &render_pass_info, nullptr, &render_pass_) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan swapchain render pass.";
            destroy_swapchain();
            return false;
        }
        framebuffers_.resize(swapchain_image_views_.size());
        for (std::size_t index = 0; index < framebuffers_.size(); ++index)
        {
            VkFramebufferCreateInfo framebuffer_info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            framebuffer_info.renderPass = render_pass_;
            framebuffer_info.attachmentCount = 1;
            framebuffer_info.pAttachments = &swapchain_image_views_[index];
            framebuffer_info.width = swapchain_extent_.width;
            framebuffer_info.height = swapchain_extent_.height;
            framebuffer_info.layers = 1;
            if (vkCreateFramebuffer(device_, &framebuffer_info, nullptr, &framebuffers_[index]) != VK_SUCCESS)
            {
                error = "Unable to create Vulkan swapchain framebuffer.";
                destroy_swapchain();
                return false;
            }
        }
        VkCommandBufferAllocateInfo command_buffer_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        command_buffer_info.commandPool = command_pool_;
        command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_buffer_info.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(device_, &command_buffer_info, &command_buffer_) != VK_SUCCESS)
        {
            error = "Unable to allocate Vulkan command buffer.";
            destroy_swapchain();
            return false;
        }
        VkSemaphoreCreateInfo semaphore_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        if (vkCreateSemaphore(device_, &semaphore_info, nullptr, &image_available_) != VK_SUCCESS ||
            vkCreateSemaphore(device_, &semaphore_info, nullptr, &render_finished_) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan frame semaphores.";
            destroy_swapchain();
            return false;
        }
        VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        if (vkCreateFence(device_, &fence_info, nullptr, &in_flight_) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan frame fence.";
            destroy_swapchain();
            return false;
        }
        return true;
    }

    bool VulkanContext::begin_frame(std::string &error)
    {
        if (!is_valid() || render_pass_ == VK_NULL_HANDLE || frame_active_)
        {
            error = "Vulkan frame cannot begin in the current state.";
            return false;
        }
        if (vkWaitForFences(device_, 1, &in_flight_, VK_TRUE, UINT64_MAX) != VK_SUCCESS ||
            vkResetFences(device_, 1, &in_flight_) != VK_SUCCESS)
        {
            error = "Unable to synchronize Vulkan frame fence.";
            return false;
        }
        VkResult acquire = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, image_available_, VK_NULL_HANDLE, &current_image_);
        if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR)
        {
            error = "Unable to acquire a Vulkan swapchain image.";
            return false;
        }
        vkResetCommandBuffer(command_buffer_, 0);
        VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        if (vkBeginCommandBuffer(command_buffer_, &begin_info) != VK_SUCCESS)
        {
            error = "Unable to begin Vulkan command buffer.";
            return false;
        }
        VkClearValue clear_value{};
        clear_value.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkRenderPassBeginInfo render_begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        render_begin.renderPass = render_pass_;
        render_begin.framebuffer = framebuffers_[current_image_];
        render_begin.renderArea.extent = swapchain_extent_;
        render_begin.clearValueCount = 1;
        render_begin.pClearValues = &clear_value;
        vkCmdBeginRenderPass(command_buffer_, &render_begin, VK_SUBPASS_CONTENTS_INLINE);
        frame_active_ = true;
        return true;
    }

    bool VulkanContext::end_frame(std::string &error)
    {
        if (!frame_active_)
        {
            error = "Vulkan frame is not active.";
            return false;
        }
        vkCmdEndRenderPass(command_buffer_);
        if (vkEndCommandBuffer(command_buffer_) != VK_SUCCESS)
        {
            error = "Unable to end Vulkan command buffer.";
            frame_active_ = false;
            return false;
        }
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &image_available_;
        submit.pWaitDstStageMask = &wait_stage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command_buffer_;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &render_finished_;
        if (vkQueueSubmit(graphics_queue_, 1, &submit, in_flight_) != VK_SUCCESS)
        {
            error = "Unable to submit Vulkan command buffer.";
            frame_active_ = false;
            return false;
        }
        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &render_finished_;
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain_;
        present.pImageIndices = &current_image_;
        const VkResult result = vkQueuePresentKHR(graphics_queue_, &present);
        frame_active_ = false;
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            error = "Unable to present Vulkan swapchain image.";
            return false;
        }
        return true;
    }

    std::uint32_t VulkanContext::find_memory_type(std::uint32_t type_filter,
                                                  VkMemoryPropertyFlags properties) const
    {
        VkPhysicalDeviceMemoryProperties memory_properties{};
        vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties);
        for (std::uint32_t index = 0; index < memory_properties.memoryTypeCount; ++index)
        {
            if ((type_filter & (1u << index)) != 0 &&
                (memory_properties.memoryTypes[index].propertyFlags & properties) == properties)
            {
                return index;
            }
        }
        return UINT32_MAX;
    }

    bool VulkanContext::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                      VkMemoryPropertyFlags properties, VulkanBuffer &result,
                                      std::string &error)
    {
        result = {};
        if (!is_valid() || size == 0)
        {
            error = "Invalid Vulkan buffer request.";
            return false;
        }
        VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        buffer_info.size = size;
        buffer_info.usage = usage;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device_, &buffer_info, nullptr, &result.buffer) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan buffer.";
            return false;
        }
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device_, result.buffer, &requirements);
        const std::uint32_t memory_type = find_memory_type(requirements.memoryTypeBits, properties);
        if (memory_type == UINT32_MAX)
        {
            error = "Unable to find Vulkan buffer memory type.";
            destroy_buffer(result);
            return false;
        }
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = memory_type;
        if (vkAllocateMemory(device_, &allocation, nullptr, &result.memory) != VK_SUCCESS ||
            vkBindBufferMemory(device_, result.buffer, result.memory, 0) != VK_SUCCESS)
        {
            error = "Unable to allocate Vulkan buffer memory.";
            destroy_buffer(result);
            return false;
        }
        result.size = size;
        return true;
    }

    void VulkanContext::destroy_buffer(VulkanBuffer &buffer)
    {
        if (device_ != VK_NULL_HANDLE && buffer.buffer != VK_NULL_HANDLE)
            vkDestroyBuffer(device_, buffer.buffer, nullptr);
        if (device_ != VK_NULL_HANDLE && buffer.memory != VK_NULL_HANDLE)
            vkFreeMemory(device_, buffer.memory, nullptr);
        buffer = {};
    }

    bool VulkanContext::upload_buffer(const VulkanBuffer &buffer, const void *data, std::size_t size,
                                      std::string &error)
    {
        if (buffer.memory == VK_NULL_HANDLE || !data || size > buffer.size)
        {
            error = "Invalid Vulkan buffer upload.";
            return false;
        }
        void *mapped = nullptr;
        if (vkMapMemory(device_, buffer.memory, 0, size, 0, &mapped) != VK_SUCCESS)
        {
            error = "Unable to map Vulkan buffer memory.";
            return false;
        }
        std::memcpy(mapped, data, size);
        vkUnmapMemory(device_, buffer.memory);
        return true;
    }

    bool VulkanContext::create_image(int width, int height, VkFormat format, VkImageUsageFlags usage,
                                     VulkanImage &result, std::string &error)
    {
        result = {};
        VkImageCreateInfo image_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = format;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = usage;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(device_, &image_info, nullptr, &result.image) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan image.";
            return false;
        }
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, result.image, &requirements);
        const std::uint32_t memory_type = find_memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memory_type == UINT32_MAX)
        {
            error = "Unable to find Vulkan image memory type.";
            destroy_image(result);
            return false;
        }
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = memory_type;
        if (vkAllocateMemory(device_, &allocation, nullptr, &result.memory) != VK_SUCCESS ||
            vkBindImageMemory(device_, result.image, result.memory, 0) != VK_SUCCESS)
        {
            error = "Unable to allocate Vulkan image memory.";
            destroy_image(result);
            return false;
        }
        VkImageViewCreateInfo view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = result.image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device_, &view_info, nullptr, &result.view) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan image view.";
            destroy_image(result);
            return false;
        }
        return true;
    }

    void VulkanContext::destroy_image(VulkanImage &image)
    {
        if (device_ != VK_NULL_HANDLE && image.view != VK_NULL_HANDLE)
            vkDestroyImageView(device_, image.view, nullptr);
        if (device_ != VK_NULL_HANDLE && image.image != VK_NULL_HANDLE)
            vkDestroyImage(device_, image.image, nullptr);
        if (device_ != VK_NULL_HANDLE && image.memory != VK_NULL_HANDLE)
            vkFreeMemory(device_, image.memory, nullptr);
        image = {};
    }

    void VulkanContext::destroy_swapchain()
    {
        if (device_ != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(device_);
            for (VkFramebuffer framebuffer : framebuffers_)
            {
                vkDestroyFramebuffer(device_, framebuffer, nullptr);
            }
            framebuffers_.clear();
            if (render_pass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, render_pass_, nullptr);
            render_pass_ = VK_NULL_HANDLE;
            if (image_available_ != VK_NULL_HANDLE) vkDestroySemaphore(device_, image_available_, nullptr);
            if (render_finished_ != VK_NULL_HANDLE) vkDestroySemaphore(device_, render_finished_, nullptr);
            if (in_flight_ != VK_NULL_HANDLE) vkDestroyFence(device_, in_flight_, nullptr);
            image_available_ = VK_NULL_HANDLE;
            render_finished_ = VK_NULL_HANDLE;
            in_flight_ = VK_NULL_HANDLE;
            command_buffer_ = VK_NULL_HANDLE;
            if (command_pool_ != VK_NULL_HANDLE)
            {
                vkDestroyCommandPool(device_, command_pool_, nullptr);
                command_pool_ = VK_NULL_HANDLE;
            }
            for (VkImageView view : swapchain_image_views_)
            {
                vkDestroyImageView(device_, view, nullptr);
            }
            swapchain_image_views_.clear();
            swapchain_images_.clear();
            if (swapchain_ != VK_NULL_HANDLE)
            {
                vkDestroySwapchainKHR(device_, swapchain_, nullptr);
                swapchain_ = VK_NULL_HANDLE;
            }
        }
        swapchain_format_ = VK_FORMAT_UNDEFINED;
        swapchain_extent_ = {};
    }

    void VulkanContext::shutdown()
    {
        destroy_swapchain();
        if (device_ != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(device_);
            vkDestroyDevice(device_, nullptr);
            device_ = VK_NULL_HANDLE;
        }
        if (instance_ != VK_NULL_HANDLE && surface_ != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(instance_, surface_, nullptr);
            surface_ = VK_NULL_HANDLE;
        }
        if (instance_ != VK_NULL_HANDLE)
        {
            vkDestroyInstance(instance_, nullptr);
            instance_ = VK_NULL_HANDLE;
        }
        physical_device_ = VK_NULL_HANDLE;
        graphics_queue_ = VK_NULL_HANDLE;
        graphics_queue_family_ = 0;
    }

    bool VulkanContext::is_valid() const
    {
        return instance_ != VK_NULL_HANDLE && physical_device_ != VK_NULL_HANDLE &&
            device_ != VK_NULL_HANDLE && graphics_queue_ != VK_NULL_HANDLE;
    }
}

#include "vulkan_context.h"
#include "renderer.h"

#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
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
        const char *device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        unsigned int device_extension_count = 0;
        vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &device_extension_count, nullptr);
        std::vector<VkExtensionProperties> device_extensions_available(device_extension_count);
        vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &device_extension_count,
                                             device_extensions_available.data());
        if (!has_extension(device_extensions_available, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
        {
            error = "Selected Vulkan device does not support VK_KHR_swapchain.";
            shutdown();
            return false;
        }
        device_info.enabledExtensionCount = 1;
        device_info.ppEnabledExtensionNames = device_extensions;
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
        attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        if (vkCreateRenderPass(device_, &render_pass_info, nullptr, &offscreen_render_pass_) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan offscreen render pass.";
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

    bool VulkanContext::begin_offscreen_render_pass(VkFramebuffer framebuffer, int width, int height,
                                                    std::string &error)
    {
        if (!frame_active_ || offscreen_active_ || framebuffer == VK_NULL_HANDLE)
        {
            error = "Invalid Vulkan offscreen render-pass state.";
            return false;
        }
        vkCmdEndRenderPass(command_buffer_);
        VkClearValue clear{};
        clear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin.renderPass = offscreen_render_pass_;
        begin.framebuffer = framebuffer;
        begin.renderArea.extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
        begin.clearValueCount = 1;
        begin.pClearValues = &clear;
        vkCmdBeginRenderPass(command_buffer_, &begin, VK_SUBPASS_CONTENTS_INLINE);
        offscreen_active_ = true;
        return true;
    }

    bool VulkanContext::end_offscreen_render_pass(std::string &error)
    {
        if (!frame_active_ || !offscreen_active_)
        {
            error = "Vulkan offscreen render pass is not active.";
            return false;
        }
        vkCmdEndRenderPass(command_buffer_);
        VkClearValue clear{};
        clear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin.renderPass = render_pass_;
        begin.framebuffer = framebuffers_[current_image_];
        begin.renderArea.extent = swapchain_extent_;
        begin.clearValueCount = 1;
        begin.pClearValues = &clear;
        vkCmdBeginRenderPass(command_buffer_, &begin, VK_SUBPASS_CONTENTS_INLINE);
        offscreen_active_ = false;
        return true;
    }

    bool VulkanContext::record_vertex_draw(VkPipeline pipeline, VkPipelineLayout layout,
                                           VkBuffer vertex_buffer, VkDescriptorSet descriptor_set,
                                           std::uint32_t vertex_count, PrimitiveType topology,
                                           const float *projection,
                                           std::string &error)
    {
        if (!frame_active_ || pipeline == VK_NULL_HANDLE || layout == VK_NULL_HANDLE ||
            vertex_buffer == VK_NULL_HANDLE || vertex_count == 0)
        {
            error = "Invalid Vulkan vertex draw state.";
            return false;
        }
        VkViewport viewport{};
        viewport.width = static_cast<float>(swapchain_extent_.width);
        viewport.height = static_cast<float>(swapchain_extent_.height);
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{{0, 0}, swapchain_extent_};
        vkCmdSetViewport(command_buffer_, 0, 1, &viewport);
        vkCmdSetScissor(command_buffer_, 0, 1, &scissor);
        vkCmdBindPipeline(command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        if (projection)
        {
            vkCmdPushConstants(command_buffer_, layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                               sizeof(float) * 16, projection);
        }
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(command_buffer_, 0, 1, &vertex_buffer, &offset);
        if (descriptor_set != VK_NULL_HANDLE)
        {
            vkCmdBindDescriptorSets(command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    layout, 0, 1, &descriptor_set, 0, nullptr);
        }
        vkCmdDraw(command_buffer_, vertex_count, 1, 0, 0);
        (void)topology;
        return true;
    }

    bool VulkanContext::clear_active_frame(float red, float green, float blue, float alpha,
                                           std::string &error)
    {
        if (!frame_active_)
        {
            error = "Vulkan frame is not active.";
            return false;
        }
        VkClearAttachment attachment{};
        attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        attachment.clearValue.color = {{red, green, blue, alpha}};
        VkClearRect rect{};
        rect.rect.extent = swapchain_extent_;
        rect.layerCount = 1;
        vkCmdClearAttachments(command_buffer_, 1, &attachment, 1, &rect);
        return true;
    }

    bool VulkanContext::record_compute_dispatch(VkPipeline pipeline, VkPipelineLayout layout,
                                                VkDescriptorSet descriptor_set, const void *push_constants,
                                                std::uint32_t push_constant_size,
                                                std::uint32_t groups_x, std::uint32_t groups_y,
                                                std::uint32_t groups_z, std::string &error)
    {
        if (!frame_active_ || pipeline == VK_NULL_HANDLE || layout == VK_NULL_HANDLE ||
            descriptor_set == VK_NULL_HANDLE || groups_x == 0 || groups_y == 0 || groups_z == 0)
        {
            error = "Invalid Vulkan compute dispatch state.";
            return false;
        }
        vkCmdBindPipeline(command_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(command_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE,
                                layout, 0, 1, &descriptor_set, 0, nullptr);
        if (push_constants && push_constant_size > 0)
        {
            vkCmdPushConstants(command_buffer_, layout, VK_SHADER_STAGE_COMPUTE_BIT,
                               0, push_constant_size, push_constants);
        }
        vkCmdDispatch(command_buffer_, groups_x, groups_y, groups_z);
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(command_buffer_, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &barrier,
                             0, nullptr, 0, nullptr);
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

    bool VulkanContext::upload_image_rgba(const VulkanImage &image, int width, int height,
                                          const std::uint8_t *pixels, std::string &error)
    {
        if (image.image == VK_NULL_HANDLE || !pixels || width <= 0 || height <= 0)
        {
            error = "Invalid Vulkan image upload.";
            return false;
        }
        VulkanBuffer staging;
        const std::size_t byte_count = static_cast<std::size_t>(width) * height * 4;
        if (!create_buffer(byte_count, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           staging, error) || !upload_buffer(staging, pixels, byte_count, error))
        {
            destroy_buffer(staging);
            return false;
        }
        VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocation.commandPool = command_pool_;
        allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocation.commandBufferCount = 1;
        VkCommandBuffer command = VK_NULL_HANDLE;
        if (vkAllocateCommandBuffers(device_, &allocation, &command) != VK_SUCCESS)
        {
            error = "Unable to allocate Vulkan image upload command buffer.";
            destroy_buffer(staging);
            return false;
        }
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(command, &begin);
        VkImageMemoryBarrier to_transfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        to_transfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_transfer.srcAccessMask = 0;
        to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        to_transfer.image = image.image;
        to_transfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        to_transfer.subresourceRange.levelCount = 1;
        to_transfer.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_transfer);
        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};
        vkCmdCopyBufferToImage(command, staging.buffer, image.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        VkImageMemoryBarrier to_shader = to_transfer;
        to_shader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_shader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        to_shader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        to_shader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_shader);
        vkEndCommandBuffer(command);
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command;
        if (vkQueueSubmit(graphics_queue_, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS ||
            vkQueueWaitIdle(graphics_queue_) != VK_SUCCESS)
        {
            error = "Unable to submit Vulkan image upload.";
            vkFreeCommandBuffers(device_, command_pool_, 1, &command);
            destroy_buffer(staging);
            return false;
        }
        vkFreeCommandBuffers(device_, command_pool_, 1, &command);
        destroy_buffer(staging);
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

    bool VulkanContext::create_render_target_framebuffer(const VulkanImage &image, int width, int height,
                                                         VkFramebuffer &framebuffer,
                                                         std::string &error)
    {
        framebuffer = VK_NULL_HANDLE;
        if (render_pass_ == VK_NULL_HANDLE || image.view == VK_NULL_HANDLE)
        {
            error = "Vulkan render-target framebuffer requires an active render pass and image view.";
            return false;
        }
        VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        info.renderPass = offscreen_render_pass_;
        info.attachmentCount = 1;
        info.pAttachments = &image.view;
        info.width = static_cast<std::uint32_t>(width);
        info.height = static_cast<std::uint32_t>(height);
        info.layers = 1;
        if (vkCreateFramebuffer(device_, &info, nullptr, &framebuffer) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan render-target framebuffer.";
            return false;
        }
        return true;
    }

    void VulkanContext::destroy_framebuffer(VkFramebuffer &framebuffer)
    {
        if (device_ != VK_NULL_HANDLE && framebuffer != VK_NULL_HANDLE)
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        framebuffer = VK_NULL_HANDLE;
    }

    bool VulkanContext::load_shader_module(const std::filesystem::path &path,
                                           VulkanShaderModule &result, std::string &error)
    {
        result = {};
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
        {
            error = "Unable to open Vulkan shader: " + path.string();
            return false;
        }
        const std::streamsize byte_count = file.tellg();
        if (byte_count <= 0 || byte_count % sizeof(std::uint32_t) != 0)
        {
            error = "Invalid SPIR-V shader size: " + path.string();
            return false;
        }
        std::vector<std::uint32_t> code(static_cast<std::size_t>(byte_count) / sizeof(std::uint32_t));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char *>(code.data()), byte_count))
        {
            error = "Unable to read Vulkan shader: " + path.string();
            return false;
        }
        VkShaderModuleCreateInfo module_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        module_info.codeSize = static_cast<std::size_t>(byte_count);
        module_info.pCode = code.data();
        if (vkCreateShaderModule(device_, &module_info, nullptr, &result.module) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan shader module: " + path.string();
            return false;
        }
        return true;
    }

    void VulkanContext::destroy_shader_module(VulkanShaderModule &module)
    {
        if (device_ != VK_NULL_HANDLE && module.module != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(device_, module.module, nullptr);
        }
        module = {};
    }

    bool VulkanContext::create_texture_descriptor_layout(VulkanDescriptorSetLayout &result, std::string &error)
    {
        result = {};
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layout_info.bindingCount = 1;
        layout_info.pBindings = &binding;
        if (vkCreateDescriptorSetLayout(device_, &layout_info, nullptr, &result.layout) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan texture descriptor-set layout.";
            return false;
        }
        return true;
    }

    void VulkanContext::destroy_descriptor_set_layout(VulkanDescriptorSetLayout &layout)
    {
        if (device_ != VK_NULL_HANDLE && layout.layout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(device_, layout.layout, nullptr);
        }
        layout = {};
    }

    bool VulkanContext::create_descriptor_pool(VulkanDescriptorPool &result, std::string &error)
    {
        result = {};
        VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256},
        };
        VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pool_info.maxSets = 256;
        pool_info.poolSizeCount = 2;
        pool_info.pPoolSizes = pool_sizes;
        if (vkCreateDescriptorPool(device_, &pool_info, nullptr, &result.pool) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan descriptor pool.";
            return false;
        }
        return true;
    }

    void VulkanContext::destroy_descriptor_pool(VulkanDescriptorPool &pool)
    {
        if (device_ != VK_NULL_HANDLE && pool.pool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(device_, pool.pool, nullptr);
        pool = {};
    }

    bool VulkanContext::create_sampler(VulkanSampler &result, std::string &error)
    {
        result = {};
        VkSamplerCreateInfo sampler_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.maxLod = 1.0f;
        if (vkCreateSampler(device_, &sampler_info, nullptr, &result.sampler) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan texture sampler.";
            return false;
        }
        return true;
    }

    void VulkanContext::destroy_sampler(VulkanSampler &sampler)
    {
        if (device_ != VK_NULL_HANDLE && sampler.sampler != VK_NULL_HANDLE)
            vkDestroySampler(device_, sampler.sampler, nullptr);
        sampler = {};
    }

    bool VulkanContext::allocate_texture_descriptor(const VulkanDescriptorPool &pool,
                                                    const VulkanDescriptorSetLayout &layout,
                                                    const VulkanImage &image, const VulkanSampler &sampler,
                                                    VkDescriptorSet &set, std::string &error)
    {
        set = VK_NULL_HANDLE;
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool = pool.pool;
        allocation.descriptorSetCount = 1;
        allocation.pSetLayouts = &layout.layout;
        if (vkAllocateDescriptorSets(device_, &allocation, &set) != VK_SUCCESS)
        {
            error = "Unable to allocate Vulkan texture descriptor set.";
            return false;
        }
        VkDescriptorImageInfo image_info{};
        image_info.imageView = image.view;
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info.sampler = sampler.sampler;
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = set;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &image_info;
        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
        return true;
    }

    bool VulkanContext::create_storage_descriptor_layout(VulkanStorageDescriptorLayout &result, std::string &error)
    {
        result = {};
        VkDescriptorSetLayoutBinding bindings[3]{};
        for (std::uint32_t index = 0; index < 3; ++index)
        {
            bindings[index].binding = index;
            bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[index].descriptorCount = 1;
            bindings[index].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        info.bindingCount = 3;
        info.pBindings = bindings;
        if (vkCreateDescriptorSetLayout(device_, &info, nullptr, &result.layout) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan storage-buffer descriptor layout.";
            return false;
        }
        return true;
    }

    void VulkanContext::destroy_storage_descriptor_layout(VulkanStorageDescriptorLayout &layout)
    {
        if (device_ != VK_NULL_HANDLE && layout.layout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(device_, layout.layout, nullptr);
        layout = {};
    }

    bool VulkanContext::allocate_storage_descriptor(const VulkanDescriptorPool &pool,
                                                    const VulkanStorageDescriptorLayout &layout,
                                                    const VulkanBuffer *buffers, std::size_t count,
                                                    VkDescriptorSet &set, std::string &error)
    {
        set = VK_NULL_HANDLE;
        if (!buffers || count == 0 || count > 3)
        {
            error = "Invalid Vulkan storage descriptor request.";
            return false;
        }
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool = pool.pool;
        allocation.descriptorSetCount = 1;
        allocation.pSetLayouts = &layout.layout;
        if (vkAllocateDescriptorSets(device_, &allocation, &set) != VK_SUCCESS)
        {
            error = "Unable to allocate Vulkan storage descriptor set.";
            return false;
        }
        std::vector<VkDescriptorBufferInfo> infos(count);
        std::vector<VkWriteDescriptorSet> writes(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            infos[index].buffer = buffers[index].buffer;
            infos[index].offset = 0;
            infos[index].range = buffers[index].size;
            writes[index] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[index].dstSet = set;
            writes[index].dstBinding = static_cast<std::uint32_t>(index);
            writes[index].descriptorCount = 1;
            writes[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[index].pBufferInfo = &infos[index];
        }
        vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
        return true;
    }

    void VulkanContext::bind_storage_descriptor(VkCommandBuffer command_buffer, VkPipelineLayout layout,
                                                VkDescriptorSet descriptor_set, VkPipelineBindPoint bind_point)
    {
        if (descriptor_set != VK_NULL_HANDLE)
            vkCmdBindDescriptorSets(command_buffer, bind_point, layout, 0, 1, &descriptor_set, 0, nullptr);
    }

    void VulkanContext::storage_barrier(VkCommandBuffer command_buffer)
    {
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    }

    bool VulkanContext::create_pipeline_layout(const VulkanDescriptorSetLayout *descriptor_layout,
                                               VkPipelineLayout &layout, std::string &error)
    {
        layout = VK_NULL_HANDLE;
        VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        VkPushConstantRange push_constants{};
        push_constants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        push_constants.offset = 0;
        push_constants.size = sizeof(float) * 16;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &push_constants;
        if (descriptor_layout)
        {
            layout_info.setLayoutCount = 1;
            layout_info.pSetLayouts = &descriptor_layout->layout;
        }
        if (vkCreatePipelineLayout(device_, &layout_info, nullptr, &layout) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan pipeline layout.";
            return false;
        }
        return true;
    }

    void VulkanContext::destroy_pipeline_layout(VkPipelineLayout &layout)
    {
        if (device_ != VK_NULL_HANDLE && layout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device_, layout, nullptr);
        }
        layout = VK_NULL_HANDLE;
    }

    bool VulkanContext::create_graphics_pipeline(const VulkanShaderModule &vertex,
                                                 const VulkanShaderModule &fragment,
                                                 const VulkanDescriptorSetLayout &descriptor_layout,
                                                 PrimitiveType topology,
                                                 VulkanGraphicsPipeline &result, std::string &error)
    {
        result = {};
        if (vertex.module == VK_NULL_HANDLE || fragment.module == VK_NULL_HANDLE ||
            !create_pipeline_layout(&descriptor_layout, result.layout, error))
        {
            if (error.empty()) error = "Invalid Vulkan graphics pipeline inputs.";
            return false;
        }

        VkPipelineShaderStageCreateInfo stages[2] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertex.module, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragment.module, "main", nullptr},
        };
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(Vertex2D);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        VkVertexInputAttributeDescription attributes[3] = {
            {0, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<std::uint32_t>(offsetof(Vertex2D, x))},
            {1, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<std::uint32_t>(offsetof(Vertex2D, u))},
            {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<std::uint32_t>(offsetof(Vertex2D, r))},
        };
        VkPipelineVertexInputStateCreateInfo vertex_input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertex_input.vertexBindingDescriptionCount = 1;
        vertex_input.pVertexBindingDescriptions = &binding;
        vertex_input.vertexAttributeDescriptionCount = 3;
        vertex_input.pVertexAttributeDescriptions = attributes;
        VkPipelineInputAssemblyStateCreateInfo input_assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        switch (topology)
        {
        case PrimitiveType::points: input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
        case PrimitiveType::lines: input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
        case PrimitiveType::line_loop: input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; break;
        case PrimitiveType::triangles: input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
        case PrimitiveType::triangle_fan: input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN; break;
        }
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo rasterization{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode = VK_CULL_MODE_NONE;
        rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState blend_attachment{};
        blend_attachment.blendEnable = VK_TRUE;
        blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blending.attachmentCount = 1;
        blending.pAttachments = &blend_attachment;
        const VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = dynamic_states;
        VkGraphicsPipelineCreateInfo pipeline_info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipeline_info.stageCount = 2;
        pipeline_info.pStages = stages;
        pipeline_info.pVertexInputState = &vertex_input;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport;
        pipeline_info.pRasterizationState = &rasterization;
        pipeline_info.pMultisampleState = &multisample;
        pipeline_info.pColorBlendState = &blending;
        pipeline_info.pDynamicState = &dynamic;
        pipeline_info.layout = result.layout;
        pipeline_info.renderPass = render_pass_;
        pipeline_info.subpass = 0;
        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &result.pipeline) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan graphics pipeline.";
            destroy_pipeline_layout(result.layout);
            result = {};
            return false;
        }
        return true;
    }

    void VulkanContext::destroy_graphics_pipeline(VulkanGraphicsPipeline &pipeline)
    {
        if (device_ != VK_NULL_HANDLE && pipeline.pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device_, pipeline.pipeline, nullptr);
        destroy_pipeline_layout(pipeline.layout);
        pipeline = {};
    }

    bool VulkanContext::create_compute_pipeline(const VulkanShaderModule &compute,
                                                const VulkanStorageDescriptorLayout &descriptor_layout,
                                                std::uint32_t push_constant_size,
                                                VulkanComputePipeline &result, std::string &error)
    {
        result = {};
        if (compute.module == VK_NULL_HANDLE)
        {
            error = "Invalid Vulkan compute shader module.";
            return false;
        }
        VkPushConstantRange push_constants{};
        push_constants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        push_constants.size = push_constant_size;
        VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &descriptor_layout.layout;
        layout_info.pushConstantRangeCount = push_constant_size > 0 ? 1u : 0u;
        layout_info.pPushConstantRanges = push_constant_size > 0 ? &push_constants : nullptr;
        if (vkCreatePipelineLayout(device_, &layout_info, nullptr, &result.layout) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan compute pipeline layout.";
            return false;
        }
        VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = compute.module;
        stage.pName = "main";
        VkComputePipelineCreateInfo pipeline_info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipeline_info.stage = stage;
        pipeline_info.layout = result.layout;
        if (vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &result.pipeline) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan compute pipeline.";
            destroy_pipeline_layout(result.layout);
            result = {};
            return false;
        }
        return true;
    }

    void VulkanContext::destroy_compute_pipeline(VulkanComputePipeline &pipeline)
    {
        if (device_ != VK_NULL_HANDLE && pipeline.pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device_, pipeline.pipeline, nullptr);
        destroy_pipeline_layout(pipeline.layout);
        pipeline = {};
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
            if (offscreen_render_pass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, offscreen_render_pass_, nullptr);
            render_pass_ = VK_NULL_HANDLE;
            offscreen_render_pass_ = VK_NULL_HANDLE;
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

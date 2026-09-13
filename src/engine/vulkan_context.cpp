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
            if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
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
        if (!vsync_enabled_ &&
            std::find(present_modes.begin(), present_modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != present_modes.end())
        {
            present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
        }
        else if (!vsync_enabled_ &&
                 std::find(present_modes.begin(), present_modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != present_modes.end())
        {
            present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
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
        VkImageUsageFlags swapchain_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        {
            swapchain_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }
        swapchain_info.imageUsage = swapchain_usage;
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

        const VkFormat depth_candidates[] = {
            VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM};
        for (const VkFormat candidate : depth_candidates)
        {
            VkFormatProperties properties{};
            vkGetPhysicalDeviceFormatProperties(physical_device_, candidate, &properties);
            if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
            {
                depth_format_ = candidate;
                break;
            }
        }
        if (depth_format_ == VK_FORMAT_UNDEFINED ||
            !create_image(static_cast<int>(extent.width), static_cast<int>(extent.height), depth_format_,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depth_image_, error))
        {
            error = "Unable to create the Vulkan depth buffer.";
            destroy_swapchain();
            return false;
        }

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
        VkAttachmentDescription depth_attachment{};
        depth_attachment.format = depth_format_;
        depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkAttachmentReference depth_reference{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_reference;
        subpass.pDepthStencilAttachment = &depth_reference;
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo render_pass_info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        VkAttachmentDescription attachments[] = {attachment, depth_attachment};
        render_pass_info.attachmentCount = 2;
        render_pass_info.pAttachments = attachments;
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
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        depth_attachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments[0] = attachment;
        attachments[1] = depth_attachment;
        if (vkCreateRenderPass(device_, &render_pass_info, nullptr, &resume_render_pass_) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan swapchain resume render pass.";
            destroy_swapchain();
            return false;
        }
        subpass.pDepthStencilAttachment = &depth_reference;
        render_pass_info.attachmentCount = 2;
        render_pass_info.pAttachments = attachments;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.format = swapchain_format_;
        attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        if (vkCreateRenderPass(device_, &render_pass_info, nullptr, &offscreen_render_pass_) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan offscreen render pass.";
            destroy_swapchain();
            return false;
        }
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        depth_attachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments[0] = attachment;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachments[1] = depth_attachment;
        if (vkCreateRenderPass(device_, &render_pass_info, nullptr, &resume_offscreen_render_pass_) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan offscreen resume render pass.";
            destroy_swapchain();
            return false;
        }
        framebuffers_.resize(swapchain_image_views_.size());
        swapchain_image_layouts_.assign(swapchain_images_.size(), VK_IMAGE_LAYOUT_UNDEFINED);
        for (std::size_t index = 0; index < framebuffers_.size(); ++index)
        {
            VkFramebufferCreateInfo framebuffer_info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            framebuffer_info.renderPass = render_pass_;
            VkImageView framebuffer_attachments[] = {swapchain_image_views_[index], depth_image_.view};
            framebuffer_info.attachmentCount = 2;
            framebuffer_info.pAttachments = framebuffer_attachments;
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
        if (!is_valid() || render_pass_ == VK_NULL_HANDLE)
        {
            error = "Vulkan frame cannot begin in the current state.";
            return false;
        }
        if (frame_active_) return true;
        if (vkWaitForFences(device_, 1, &in_flight_, VK_TRUE, UINT64_MAX) != VK_SUCCESS ||
            vkResetFences(device_, 1, &in_flight_) != VK_SUCCESS)
        {
            error = "Unable to synchronize Vulkan frame fence.";
            return false;
        }
        for (VulkanBuffer &buffer : upload_staging_buffers_) destroy_buffer(buffer);
        upload_staging_buffers_.clear();
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
        command_buffer_recording_ = true;
        VkClearValue clear_values[2]{};
        clear_values[1].depthStencil = {1.0f, 0};
        clear_values[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkRenderPassBeginInfo render_begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        render_begin.renderPass = render_pass_;
        render_begin.framebuffer = framebuffers_[current_image_];
        render_begin.renderArea.extent = swapchain_extent_;
        render_begin.clearValueCount = 2;
        render_begin.pClearValues = clear_values;
        vkCmdBeginRenderPass(command_buffer_, &render_begin, VK_SUBPASS_CONTENTS_INLINE);
        active_extent_ = swapchain_extent_;
        active_render_pass_ = render_pass_;
        active_resume_render_pass_ = resume_render_pass_;
        active_framebuffer_ = framebuffers_[current_image_];
        frame_active_ = true;
        render_pass_active_ = true;
        render_pass_stack_.clear();
        return true;
    }

    bool VulkanContext::end_frame(std::string &error)
    {
        if (!frame_active_)
        {
            error = "Vulkan frame is not active.";
            return false;
        }
        if (command_buffer_recording_ && render_pass_active_) vkCmdEndRenderPass(command_buffer_);
        render_pass_active_ = false;
        if (vkEndCommandBuffer(command_buffer_) != VK_SUCCESS)
        {
            error = "Unable to end Vulkan command buffer.";
            frame_active_ = false;
            return false;
        }
        command_buffer_recording_ = false;
        render_pass_stack_.clear();
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

    bool VulkanContext::begin_offscreen_render_pass(VkFramebuffer framebuffer, VulkanImage &image,
                                                    int width, int height,
                                                    std::string &error)
    {
        if (!frame_active_ || framebuffer == VK_NULL_HANDLE)
        {
            error = "Invalid Vulkan offscreen render-pass state.";
            return false;
        }
        if (command_buffer_recording_ && render_pass_active_) vkCmdEndRenderPass(command_buffer_);
        render_pass_stack_.push_back({active_extent_, active_render_pass_, active_resume_render_pass_,
            active_framebuffer_, offscreen_active_, active_offscreen_image_});
        VkClearValue clears[2]{};
        clears[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
        clears[1].depthStencil = {1.0f, 0};
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin.renderPass = offscreen_render_pass_;
        begin.framebuffer = framebuffer;
        begin.renderArea.extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
        begin.clearValueCount = 2;
        begin.pClearValues = clears;
        vkCmdBeginRenderPass(command_buffer_, &begin, VK_SUBPASS_CONTENTS_INLINE);
        active_extent_ = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
        active_render_pass_ = offscreen_render_pass_;
        active_resume_render_pass_ = resume_offscreen_render_pass_;
        active_framebuffer_ = framebuffer;
        render_pass_active_ = true;
        offscreen_active_ = true;
        active_offscreen_image_ = image.image;
        image.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        return true;
    }

    bool VulkanContext::end_offscreen_render_pass(std::string &error)
    {
        if (!frame_active_ || !offscreen_active_ || render_pass_stack_.empty())
        {
            error = "Vulkan offscreen render pass is not active.";
            return false;
        }
        if (command_buffer_recording_ && render_pass_active_) vkCmdEndRenderPass(command_buffer_);
        render_pass_active_ = false;
        const RenderPassState previous = render_pass_stack_.back();
        render_pass_stack_.pop_back();
        VkClearValue clear_values[2]{};
        clear_values[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clear_values[1].depthStencil = {1.0f, 0};
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin.renderPass = previous.resume_render_pass;
        begin.framebuffer = previous.framebuffer;
        begin.renderArea.extent = previous.extent;
        begin.clearValueCount = 2;
        begin.pClearValues = clear_values;
        vkCmdBeginRenderPass(command_buffer_, &begin, VK_SUBPASS_CONTENTS_INLINE);
        active_extent_ = previous.extent;
        active_render_pass_ = previous.resume_render_pass;
        active_resume_render_pass_ = previous.resume_render_pass;
        active_framebuffer_ = previous.framebuffer;
        offscreen_active_ = previous.offscreen;
        active_offscreen_image_ = previous.offscreen_image;
        render_pass_active_ = true;
        return true;
    }

    bool VulkanContext::prepare_image_for_sampling(VulkanImage &image, std::string &error)
    {
        if (!frame_active_ || !command_buffer_recording_ || image.image == VK_NULL_HANDLE)
        {
            error = "Invalid Vulkan sampled-image transition state.";
            return false;
        }
        if (image.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            return true;
        std::fprintf(stderr, "Vulkan sample barrier image=%llx tracked=%d\n",
            static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(image.image)),
            static_cast<int>(image.layout));
        if (render_pass_active_)
        {
            vkCmdEndRenderPass(command_buffer_);
            render_pass_active_ = false;
        }
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.oldLayout = image.layout == VK_IMAGE_LAYOUT_UNDEFINED
            ? VK_IMAGE_LAYOUT_UNDEFINED : image.layout;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = image.layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.image = image.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        const VkPipelineStageFlags source_stage = image.layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        vkCmdPipelineBarrier(command_buffer_, source_stage, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        image.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkClearValue clear_values[2]{};
        clear_values[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clear_values[1].depthStencil = {1.0f, 0};
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin.renderPass = active_resume_render_pass_;
        begin.framebuffer = active_framebuffer_;
        begin.renderArea.extent = active_extent_;
        begin.clearValueCount = 2;
        begin.pClearValues = clear_values;
        vkCmdBeginRenderPass(command_buffer_, &begin, VK_SUBPASS_CONTENTS_INLINE);
        active_render_pass_ = active_resume_render_pass_;
        render_pass_active_ = true;
        return true;
    }

    bool VulkanContext::record_vertex_draw(VkPipeline pipeline, VkPipelineLayout layout,
                                           VkBuffer vertex_buffer, VkDescriptorSet descriptor_set,
                                           std::uint32_t vertex_count, PrimitiveType topology,
                                           const float *projection,
                                           std::string &error)
    {
        if (!frame_active_ || !command_buffer_recording_ || pipeline == VK_NULL_HANDLE || layout == VK_NULL_HANDLE ||
            vertex_buffer == VK_NULL_HANDLE || vertex_count == 0)
        {
            error = "Invalid Vulkan vertex draw state.";
            return false;
        }
        VkViewport viewport{};
        viewport.y = static_cast<float>(active_extent_.height);
        viewport.width = static_cast<float>(active_extent_.width);
        viewport.height = -static_cast<float>(active_extent_.height);
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{{0, 0}, active_extent_};
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

    bool VulkanContext::record_3d_draw(VkPipeline pipeline, VkPipelineLayout layout,
                                       VkBuffer vertex_buffer, std::uint32_t vertex_count,
                                       const float *mvp, std::string &error)
    {
        if (!frame_active_ || !command_buffer_recording_ || pipeline == VK_NULL_HANDLE ||
            layout == VK_NULL_HANDLE || vertex_buffer == VK_NULL_HANDLE || !mvp || vertex_count == 0)
        {
            error = "Invalid Vulkan 3D draw state.";
            return false;
        }
        VkViewport viewport{0.0f, 0.0f, static_cast<float>(active_extent_.width),
            static_cast<float>(active_extent_.height), 0.0f, 1.0f};
        VkRect2D scissor{{0, 0}, active_extent_};
        vkCmdSetViewport(command_buffer_, 0, 1, &viewport);
        vkCmdSetScissor(command_buffer_, 0, 1, &scissor);
        vkCmdBindPipeline(command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdPushConstants(command_buffer_, layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
            sizeof(float) * 16, mvp);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(command_buffer_, 0, 1, &vertex_buffer, &offset);
        vkCmdDraw(command_buffer_, vertex_count, 1, 0, 0);
        return true;
    }

    bool VulkanContext::record_lighting_draw(VkPipeline pipeline, VkPipelineLayout layout,
                                             VkBuffer vertex_buffer, VkDescriptorSet texture_descriptor,
                                             VkDescriptorSet storage_descriptor, std::uint32_t vertex_count,
                                             const float *projection, const void *lighting_constants,
                                             std::uint32_t lighting_constant_size, std::string &error)
    {
        if (!frame_active_ || pipeline == VK_NULL_HANDLE || layout == VK_NULL_HANDLE ||
            vertex_buffer == VK_NULL_HANDLE || texture_descriptor == VK_NULL_HANDLE ||
            storage_descriptor == VK_NULL_HANDLE || !projection || !lighting_constants || vertex_count == 0)
        {
            error = "Invalid Vulkan lighting draw state.";
            return false;
        }
        VkViewport viewport{};
        viewport.y = static_cast<float>(active_extent_.height);
        viewport.width = static_cast<float>(active_extent_.width);
        viewport.height = -static_cast<float>(active_extent_.height);
        viewport.maxDepth = 1.0f;
        const VkRect2D scissor{{0, 0}, active_extent_};
        vkCmdSetViewport(command_buffer_, 0, 1, &viewport);
        vkCmdSetScissor(command_buffer_, 0, 1, &scissor);
        vkCmdBindPipeline(command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        const VkDescriptorSet descriptors[] = {texture_descriptor, storage_descriptor};
        vkCmdBindDescriptorSets(command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 2, descriptors, 0, nullptr);
        vkCmdPushConstants(command_buffer_, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 16, projection);
        vkCmdPushConstants(command_buffer_, layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(float) * 16,
                           lighting_constant_size, lighting_constants);
        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(command_buffer_, 0, 1, &vertex_buffer, &offset);
        vkCmdDraw(command_buffer_, vertex_count, 1, 0, 0);
        return true;
    }

    bool VulkanContext::record_postprocess_draw(VkPipeline pipeline, VkPipelineLayout layout,
                                                VkBuffer vertex_buffer, VkDescriptorSet texture_descriptor,
                                                std::uint32_t vertex_count, const float *projection,
                                                const void *constants, std::uint32_t constant_size,
                                                std::string &error)
    {
        if (!frame_active_ || !command_buffer_recording_ || !render_pass_active_ ||
            pipeline == VK_NULL_HANDLE || layout == VK_NULL_HANDLE || vertex_buffer == VK_NULL_HANDLE ||
            texture_descriptor == VK_NULL_HANDLE || !projection || (constant_size > 0 && !constants) || vertex_count == 0)
        {
            error = "Invalid Vulkan post-process draw state.";
            return false;
        }
        VkViewport viewport{};
        viewport.y = static_cast<float>(active_extent_.height);
        viewport.width = static_cast<float>(active_extent_.width);
        viewport.height = -static_cast<float>(active_extent_.height);
        viewport.maxDepth = 1.0f;
        const VkRect2D scissor{{0, 0}, active_extent_};
        vkCmdSetViewport(command_buffer_, 0, 1, &viewport);
        vkCmdSetScissor(command_buffer_, 0, 1, &scissor);
        vkCmdBindPipeline(command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1,
                                &texture_descriptor, 0, nullptr);
        vkCmdPushConstants(command_buffer_, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 16, projection);
        if (constant_size > 0)
        {
            vkCmdPushConstants(command_buffer_, layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(float) * 16,
                               constant_size, constants);
        }
        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(command_buffer_, 0, 1, &vertex_buffer, &offset);
        vkCmdDraw(command_buffer_, vertex_count, 1, 0, 0);
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
        rect.rect.extent = active_extent_;
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
        if (command_buffer_recording_ && render_pass_active_) vkCmdEndRenderPass(command_buffer_);
        render_pass_active_ = false;
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
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin.renderPass = active_resume_render_pass_;
        begin.framebuffer = active_framebuffer_;
        begin.renderArea.extent = active_extent_;
        VkClearValue clear{};
        begin.clearValueCount = 1;
        begin.pClearValues = &clear;
        vkCmdBeginRenderPass(command_buffer_, &begin, VK_SUBPASS_CONTENTS_INLINE);
        active_render_pass_ = active_resume_render_pass_;
        render_pass_active_ = true;
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

    bool VulkanContext::upload_image_rgba(VulkanImage &image, int width, int height,
                                          const std::uint8_t *pixels, std::string &error)
    {
        if (image.image == VK_NULL_HANDLE || !pixels || width <= 0 || height <= 0)
        {
            error = "Invalid Vulkan image upload.";
            return false;
        }
        VulkanBuffer staging;
        const std::size_t byte_count = static_cast<std::size_t>(width) * height * 4;
        std::vector<std::uint8_t> upload_pixels(pixels, pixels + byte_count);
        if (image.format == VK_FORMAT_B8G8R8A8_UNORM || image.format == VK_FORMAT_B8G8R8A8_SRGB)
        {
            for (std::size_t index = 0; index < upload_pixels.size(); index += 4)
                std::swap(upload_pixels[index], upload_pixels[index + 2]);
        }
        if (!create_buffer(byte_count, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           staging, error) || !upload_buffer(staging, upload_pixels.data(), byte_count, error))
        {
            destroy_buffer(staging);
            return false;
        }
        const bool use_active_command = frame_active_;
        VkCommandBuffer command = command_buffer_;
        if (!use_active_command)
        {
            VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            allocation.commandPool = command_pool_;
            allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocation.commandBufferCount = 1;
            if (vkAllocateCommandBuffers(device_, &allocation, &command) != VK_SUCCESS)
            {
                error = "Unable to allocate Vulkan image upload command buffer.";
                destroy_buffer(staging);
                return false;
            }
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(command, &begin);
        }
        else
        {
            if (command_buffer_recording_ && render_pass_active_) vkCmdEndRenderPass(command);
            render_pass_active_ = false;
        }
        VkImageMemoryBarrier to_transfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        to_transfer.oldLayout = image.layout;
        to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_transfer.srcAccessMask = image.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            ? VK_ACCESS_SHADER_READ_BIT : 0;
        to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        to_transfer.image = image.image;
        to_transfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        to_transfer.subresourceRange.levelCount = 1;
        to_transfer.subresourceRange.layerCount = 1;
        const VkPipelineStageFlags source_stage = image.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        vkCmdPipelineBarrier(command, source_stage,
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
        if (use_active_command)
        {
            VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            begin.renderPass = active_resume_render_pass_;
            begin.framebuffer = active_framebuffer_;
            begin.renderArea.extent = active_extent_;
            VkClearValue clear{};
            begin.clearValueCount = 1;
            begin.pClearValues = &clear;
            vkCmdBeginRenderPass(command, &begin, VK_SUBPASS_CONTENTS_INLINE);
            active_render_pass_ = active_resume_render_pass_;
            render_pass_active_ = true;
            upload_staging_buffers_.push_back(staging);
            image.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            return true;
        }
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
        image.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        return true;
    }

    bool VulkanContext::download_image_rgba(VulkanImage &image, int width, int height,
                                            std::uint8_t *out_pixels, std::string &error)
    {
        if (image.image == VK_NULL_HANDLE || !out_pixels || width <= 0 || height <= 0)
        {
            error = "Invalid Vulkan image download parameters.";
            return false;
        }

        if (device_ != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(device_);
        }

        const std::size_t byte_count = static_cast<std::size_t>(width) * height * 4;
        VulkanBuffer staging;
        if (!create_buffer(byte_count, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           staging, error))
        {
            return false;
        }

        VkCommandBufferAllocateInfo alloc_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        alloc_info.commandPool = command_pool_;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        if (vkAllocateCommandBuffers(device_, &alloc_info, &cmd) != VK_SUCCESS)
        {
            error = "Unable to allocate Vulkan download command buffer.";
            destroy_buffer(staging);
            return false;
        }

        VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(cmd, &begin_info) != VK_SUCCESS)
        {
            error = "Unable to begin Vulkan download command buffer.";
            vkFreeCommandBuffers(device_, command_pool_, 1, &cmd);
            destroy_buffer(staging);
            return false;
        }

        const VkImageLayout original_layout = image.layout != VK_IMAGE_LAYOUT_UNDEFINED
            ? image.layout : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkImageMemoryBarrier to_src{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        to_src.oldLayout = original_layout;
        to_src.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        to_src.srcAccessMask = (original_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT;
        to_src.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        to_src.image = image.image;
        to_src.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        to_src.subresourceRange.levelCount = 1;
        to_src.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_src);

        VkBufferImageCopy copy_region{};
        copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_region.imageSubresource.layerCount = 1;
        copy_region.imageExtent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};

        vkCmdCopyImageToBuffer(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               staging.buffer, 1, &copy_region);

        VkImageMemoryBarrier to_orig = to_src;
        to_orig.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        to_orig.newLayout = original_layout;
        to_orig.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        to_orig.dstAccessMask = to_src.srcAccessMask;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_orig);

        if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
        {
            error = "Unable to end Vulkan download command buffer.";
            vkFreeCommandBuffers(device_, command_pool_, 1, &cmd);
            destroy_buffer(staging);
            return false;
        }

        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        if (vkQueueSubmit(graphics_queue_, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS ||
            vkQueueWaitIdle(graphics_queue_) != VK_SUCCESS)
        {
            error = "Unable to submit Vulkan download commands.";
            vkFreeCommandBuffers(device_, command_pool_, 1, &cmd);
            destroy_buffer(staging);
            return false;
        }
        vkFreeCommandBuffers(device_, command_pool_, 1, &cmd);

        void *mapped = nullptr;
        if (vkMapMemory(device_, staging.memory, 0, byte_count, 0, &mapped) != VK_SUCCESS || !mapped)
        {
            error = "Unable to map Vulkan staging buffer for download.";
            destroy_buffer(staging);
            return false;
        }

        std::memcpy(out_pixels, mapped, byte_count);
        vkUnmapMemory(device_, staging.memory);
        destroy_buffer(staging);

        if (image.format == VK_FORMAT_B8G8R8A8_UNORM || image.format == VK_FORMAT_B8G8R8A8_SRGB)
        {
            for (std::size_t index = 0; index < byte_count; index += 4)
            {
                std::swap(out_pixels[index], out_pixels[index + 2]);
            }
        }

        image.layout = original_layout;
        return true;
    }

    bool VulkanContext::download_swapchain_rgba(int width, int height,
                                                std::uint8_t *out_pixels, std::string &error)
    {
        if (swapchain_images_.empty() || current_image_ >= swapchain_images_.size() || !out_pixels || width <= 0 || height <= 0)
        {
            error = "Invalid Vulkan swapchain download state.";
            return false;
        }
        VulkanImage swapchain_img;
        swapchain_img.image = swapchain_images_[current_image_];
        swapchain_img.format = swapchain_format_;
        swapchain_img.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        return download_image_rgba(swapchain_img, width, height, out_pixels, error);
    }

    bool VulkanContext::create_image(int width, int height, VkFormat format, VkImageUsageFlags usage,
                                     VulkanImage &result, std::string &error)
    {
        result = {};
        result.format = format;
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
        view_info.subresourceRange.aspectMask =
            (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0
                ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
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

    bool VulkanContext::create_render_target_framebuffer(const VulkanImage &image, const VulkanImage &depth,
                                                         int width, int height,
                                                         VkFramebuffer &framebuffer,
                                                         std::string &error)
    {
        framebuffer = VK_NULL_HANDLE;
        if (render_pass_ == VK_NULL_HANDLE || image.view == VK_NULL_HANDLE || depth.view == VK_NULL_HANDLE)
        {
            error = "Vulkan render-target framebuffer requires an active render pass and image view.";
            return false;
        }
        VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        info.renderPass = offscreen_render_pass_;
        const VkImageView attachments[] = {image.view, depth.view};
        info.attachmentCount = 2;
        info.pAttachments = attachments;
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

    bool VulkanContext::create_shader_module(const std::vector<std::uint32_t> &code,
                                             VulkanShaderModule &result, std::string &error)
    {
        result = {};
        if (code.empty() || code.front() != 0x07230203u)
        {
            error = "Invalid SPIR-V shader module payload.";
            return false;
        }
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = code.size() * sizeof(std::uint32_t);
        info.pCode = code.data();
        if (vkCreateShaderModule(device_, &info, nullptr, &result.module) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan shader module from payload.";
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

    bool VulkanContext::create_lighting_descriptor_layout(VulkanDescriptorSetLayout &result, std::string &error)
    {
        result = {};
        VkDescriptorSetLayoutBinding bindings[2]{};
        bindings[0] = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        bindings[1] = {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        info.bindingCount = 2;
        info.pBindings = bindings;
        if (vkCreateDescriptorSetLayout(device_, &info, nullptr, &result.layout) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan lighting descriptor-set layout.";
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
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 256},
        };
        VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 256;
        pool_info.poolSizeCount = 3;
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

    bool VulkanContext::create_sampler(TextureFilter filter, VulkanSampler &result, std::string &error)
    {
        result = {};
        VkSamplerCreateInfo sampler_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        const VkFilter vulkan_filter = filter == TextureFilter::linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        sampler_info.magFilter = vulkan_filter;
        sampler_info.minFilter = vulkan_filter;
        sampler_info.mipmapMode = filter == TextureFilter::linear
            ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
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
        image_info.imageLayout = image.layout == VK_IMAGE_LAYOUT_UNDEFINED
            ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : image.layout;
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

    bool VulkanContext::update_texture_descriptor(VkDescriptorSet set, const VulkanImage &image,
                                                  const VulkanSampler &sampler, std::string &error)
    {
        if (set == VK_NULL_HANDLE || image.view == VK_NULL_HANDLE || sampler.sampler == VK_NULL_HANDLE)
        {
            error = "Invalid Vulkan texture descriptor update.";
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

    void VulkanContext::free_descriptor_set(const VulkanDescriptorPool &pool, VkDescriptorSet &set)
    {
        if (device_ != VK_NULL_HANDLE && pool.pool != VK_NULL_HANDLE && set != VK_NULL_HANDLE)
            vkFreeDescriptorSets(device_, pool.pool, 1, &set);
        set = VK_NULL_HANDLE;
    }

    bool VulkanContext::allocate_lighting_descriptor(const VulkanDescriptorPool &pool,
                                                     const VulkanDescriptorSetLayout &layout,
                                                     const VulkanImage *images, const VulkanSampler *samplers,
                                                     VkDescriptorSet &set, std::string &error)
    {
        set = VK_NULL_HANDLE;
        if (!images || !samplers)
        {
            error = "Invalid Vulkan lighting descriptor request.";
            return false;
        }
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool = pool.pool;
        allocation.descriptorSetCount = 1;
        allocation.pSetLayouts = &layout.layout;
        if (vkAllocateDescriptorSets(device_, &allocation, &set) != VK_SUCCESS)
        {
            error = "Unable to allocate Vulkan lighting descriptor set.";
            return false;
        }
        VkDescriptorImageInfo image_infos[9]{};
        for (std::uint32_t index = 0; index < 9; ++index)
        {
            if (images[index].view == VK_NULL_HANDLE || samplers[index].sampler == VK_NULL_HANDLE)
            {
                error = "Invalid Vulkan lighting texture descriptor.";
                return false;
            }
            image_infos[index].imageView = images[index].view;
            image_infos[index].imageLayout = images[index].layout == VK_IMAGE_LAYOUT_UNDEFINED
                ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : images[index].layout;
            image_infos[index].sampler = samplers[index].sampler;
        }
        VkWriteDescriptorSet writes[2]{};
        writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[0].dstSet = set;
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].pImageInfo = image_infos;
        writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[1].dstSet = set;
        writes[1].dstBinding = 1;
        writes[1].descriptorCount = 8;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].pImageInfo = image_infos + 1;
        vkUpdateDescriptorSets(device_, 2, writes, 0, nullptr);
        return true;
    }

    bool VulkanContext::update_lighting_descriptor(VkDescriptorSet set, const VulkanImage *images,
                                                    const VulkanSampler *samplers, std::string &error)
    {
        if (set == VK_NULL_HANDLE || !images || !samplers)
        {
            error = "Invalid Vulkan lighting descriptor update.";
            return false;
        }
        VkDescriptorImageInfo image_infos[9]{};
        for (std::uint32_t index = 0; index < 9; ++index)
        {
            if (images[index].view == VK_NULL_HANDLE || samplers[index].sampler == VK_NULL_HANDLE)
            {
                error = "Invalid Vulkan lighting texture descriptor.";
                return false;
            }
            image_infos[index].imageView = images[index].view;
            image_infos[index].imageLayout = images[index].layout == VK_IMAGE_LAYOUT_UNDEFINED
                ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : images[index].layout;
            image_infos[index].sampler = samplers[index].sampler;
        }
        VkWriteDescriptorSet writes[2]{};
        writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[0].dstSet = set;
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].pImageInfo = image_infos;
        writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[1].dstSet = set;
        writes[1].dstBinding = 1;
        writes[1].descriptorCount = 8;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].pImageInfo = image_infos + 1;
        vkUpdateDescriptorSets(device_, 2, writes, 0, nullptr);
        return true;
    }

    bool VulkanContext::create_composite_descriptor_layout(VulkanDescriptorSetLayout &result, std::string &error)
    {
        result = {};
        VkDescriptorSetLayoutBinding bindings[2]{};
        bindings[0] = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        bindings[1] = {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        info.bindingCount = 2;
        info.pBindings = bindings;
        if (vkCreateDescriptorSetLayout(device_, &info, nullptr, &result.layout) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan composite descriptor-set layout.";
            return false;
        }
        return true;
    }

    bool VulkanContext::allocate_composite_descriptor(const VulkanDescriptorPool &pool,
                                                       const VulkanDescriptorSetLayout &layout,
                                                       const VulkanImage *images, const VulkanSampler *samplers,
                                                       VkDescriptorSet &set, std::string &error)
    {
        set = VK_NULL_HANDLE;
        if (!images || !samplers)
        {
            error = "Invalid Vulkan composite descriptor request.";
            return false;
        }
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool = pool.pool;
        allocation.descriptorSetCount = 1;
        allocation.pSetLayouts = &layout.layout;
        if (vkAllocateDescriptorSets(device_, &allocation, &set) != VK_SUCCESS)
        {
            error = "Unable to allocate Vulkan composite descriptor set.";
            return false;
        }
        return update_composite_descriptor(set, images, samplers, error);
    }

    bool VulkanContext::update_composite_descriptor(VkDescriptorSet set, const VulkanImage *images,
                                                     const VulkanSampler *samplers, std::string &error)
    {
        if (set == VK_NULL_HANDLE || !images || !samplers)
        {
            error = "Invalid Vulkan composite descriptor update.";
            return false;
        }
        VkDescriptorImageInfo image_infos[2]{};
        for (std::uint32_t index = 0; index < 2; ++index)
        {
            if (images[index].view == VK_NULL_HANDLE || samplers[index].sampler == VK_NULL_HANDLE)
            {
                error = "Invalid Vulkan composite texture descriptor.";
                return false;
            }
            image_infos[index].imageView = images[index].view;
            image_infos[index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_infos[index].sampler = samplers[index].sampler;
        }
        VkWriteDescriptorSet writes[2]{};
        for (std::uint32_t index = 0; index < 2; ++index)
        {
            writes[index] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[index].dstSet = set;
            writes[index].dstBinding = index;
            writes[index].descriptorCount = 1;
            writes[index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[index].pImageInfo = &image_infos[index];
        }
        vkUpdateDescriptorSets(device_, 2, writes, 0, nullptr);
        return true;
    }

    bool VulkanContext::create_dynamic_descriptor_layout(const std::vector<VkDescriptorSetLayoutBinding> &bindings,
                                                         VulkanDescriptorSetLayout &result, std::string &error)
    {
        result = {};
        VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        info.bindingCount = static_cast<std::uint32_t>(bindings.size());
        info.pBindings = bindings.empty() ? nullptr : bindings.data();
        if (vkCreateDescriptorSetLayout(device_, &info, nullptr, &result.layout) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan dynamic descriptor-set layout.";
            return false;
        }
        return true;
    }

    bool VulkanContext::allocate_descriptor_set(const VulkanDescriptorPool &pool, const VulkanDescriptorSetLayout &layout,
                                                VkDescriptorSet &set, std::string &error)
    {
        set = VK_NULL_HANDLE;
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool = pool.pool;
        allocation.descriptorSetCount = 1;
        allocation.pSetLayouts = &layout.layout;
        if (vkAllocateDescriptorSets(device_, &allocation, &set) != VK_SUCCESS)
        {
            error = "Unable to allocate Vulkan dynamic descriptor set.";
            return false;
        }
        return true;
    }

    bool VulkanContext::update_dynamic_descriptor_set(VkDescriptorSet set, const std::vector<VulkanImage> &images,
                                                      const std::vector<VulkanSampler> &samplers, std::string &error)
    {
        if (set == VK_NULL_HANDLE || images.size() != samplers.size())
        {
            error = "Invalid Vulkan dynamic descriptor update.";
            return false;
        }
        if (images.empty()) return true;
        std::vector<VkDescriptorImageInfo> image_infos(images.size());
        std::vector<VkWriteDescriptorSet> writes(images.size());
        for (std::size_t index = 0; index < images.size(); ++index)
        {
            if (images[index].view == VK_NULL_HANDLE || samplers[index].sampler == VK_NULL_HANDLE)
            {
                error = "Invalid Vulkan dynamic texture descriptor.";
                return false;
            }
            image_infos[index].imageView = images[index].view;
            image_infos[index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_infos[index].sampler = samplers[index].sampler;
            writes[index] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[index].dstSet = set;
            writes[index].dstBinding = static_cast<std::uint32_t>(index);
            writes[index].descriptorCount = 1;
            writes[index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[index].pImageInfo = &image_infos[index];
        }
        vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
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

    bool VulkanContext::create_compute_descriptor_layout_flexible(std::uint32_t storage_buffer_count,
                                                                  std::uint32_t storage_image_count,
                                                                  std::uint32_t sampler_count,
                                                                  VulkanStorageDescriptorLayout &result,
                                                                  std::string &error)
    {
        result = {};
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        std::uint32_t binding_slot = 0;
        for (std::uint32_t i = 0; i < storage_buffer_count; ++i)
        {
            VkDescriptorSetLayoutBinding b{};
            b.binding = binding_slot++;
            b.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            b.descriptorCount = 1;
            b.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            bindings.push_back(b);
        }
        for (std::uint32_t i = 0; i < storage_image_count; ++i)
        {
            VkDescriptorSetLayoutBinding b{};
            b.binding = binding_slot++;
            b.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            b.descriptorCount = 1;
            b.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            bindings.push_back(b);
        }
        for (std::uint32_t i = 0; i < sampler_count; ++i)
        {
            VkDescriptorSetLayoutBinding b{};
            b.binding = binding_slot++;
            b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            b.descriptorCount = 1;
            b.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            bindings.push_back(b);
        }
        VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        info.bindingCount = static_cast<std::uint32_t>(bindings.size());
        info.pBindings = bindings.data();
        if (vkCreateDescriptorSetLayout(device_, &info, nullptr, &result.layout) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan compute descriptor layout.";
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

    bool VulkanContext::allocate_compute_descriptor_set(const VulkanDescriptorPool &pool,
                                                        const VulkanStorageDescriptorLayout &layout,
                                                        VkDescriptorSet &set, std::string &error)
    {
        set = VK_NULL_HANDLE;
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool = pool.pool;
        allocation.descriptorSetCount = 1;
        allocation.pSetLayouts = &layout.layout;
        if (vkAllocateDescriptorSets(device_, &allocation, &set) != VK_SUCCESS)
        {
            error = "Unable to allocate Vulkan compute descriptor set.";
            return false;
        }
        return true;
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

    bool VulkanContext::update_storage_descriptor(VkDescriptorSet set, const VulkanBuffer *buffers,
                                                  std::size_t count, std::string &error)
    {
        if (set == VK_NULL_HANDLE || !buffers || count != 3)
        {
            error = "Invalid Vulkan storage descriptor update.";
            return false;
        }
        VkDescriptorBufferInfo infos[3]{};
        VkWriteDescriptorSet writes[3]{};
        for (std::uint32_t index = 0; index < 3; ++index)
        {
            if (buffers[index].buffer == VK_NULL_HANDLE || buffers[index].size == 0)
            {
                error = "Invalid Vulkan storage buffer descriptor update.";
                return false;
            }
            infos[index].buffer = buffers[index].buffer;
            infos[index].range = buffers[index].size;
            writes[index] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[index].dstSet = set;
            writes[index].dstBinding = index;
            writes[index].descriptorCount = 1;
            writes[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[index].pBufferInfo = &infos[index];
        }
        vkUpdateDescriptorSets(device_, 3, writes, 0, nullptr);
        return true;
    }

    bool VulkanContext::download_buffer(const VulkanBuffer &buffer, void *out_data, std::size_t size,
                                        std::size_t offset, std::string &error)
    {
        if (buffer.memory == VK_NULL_HANDLE || !out_data || (offset + size) > buffer.size)
        {
            error = "Invalid Vulkan buffer download request.";
            return false;
        }
        void *mapped = nullptr;
        if (vkMapMemory(device_, buffer.memory, static_cast<VkDeviceSize>(offset), static_cast<VkDeviceSize>(size), 0, &mapped) != VK_SUCCESS)
        {
            error = "Unable to map Vulkan buffer memory for readback.";
            return false;
        }
        std::memcpy(out_data, mapped, size);
        vkUnmapMemory(device_, buffer.memory);
        return true;
    }

    bool VulkanContext::create_imgui_descriptor_pool(VkDescriptorPool &pool, std::string &error)
    {
        pool = VK_NULL_HANDLE;
        const VkDescriptorPoolSize sizes[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, 256},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 256},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 64},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 64},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 64},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 64},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 64},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 64},
        };
        VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        info.maxSets = 1024;
        info.poolSizeCount = static_cast<std::uint32_t>(std::size(sizes));
        info.pPoolSizes = sizes;
        if (vkCreateDescriptorPool(device_, &info, nullptr, &pool) != VK_SUCCESS)
        {
            error = "Unable to create Vulkan ImGui descriptor pool.";
            return false;
        }
        return true;
    }

    void VulkanContext::destroy_imgui_descriptor_pool(VkDescriptorPool &pool)
    {
        if (device_ != VK_NULL_HANDLE && pool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(device_, pool, nullptr);
        pool = VK_NULL_HANDLE;
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
                                                 VulkanGraphicsPipeline &result, std::string &error,
                                                 const VulkanStorageDescriptorLayout *storage_layout,
                                                 std::uint32_t fragment_push_constant_size,
                                                 bool three_dimensional,
                                                 bool premultiplied_alpha)
    {
        result = {};
        if (vertex.module == VK_NULL_HANDLE || fragment.module == VK_NULL_HANDLE)
        {
            error = "Invalid Vulkan graphics pipeline inputs.";
            return false;
        }
        if (three_dimensional)
        {
            VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 16};
            VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layout_info.setLayoutCount = 1;
            layout_info.pSetLayouts = &descriptor_layout.layout;
            layout_info.pushConstantRangeCount = 1;
            layout_info.pPushConstantRanges = &range;
            if (vkCreatePipelineLayout(device_, &layout_info, nullptr, &result.layout) != VK_SUCCESS)
            {
                error = "Unable to create Vulkan 3D pipeline layout.";
                return false;
            }
        }
        else if (storage_layout || fragment_push_constant_size > 0)
        {
            VkDescriptorSetLayout layouts[2] = {descriptor_layout.layout, VK_NULL_HANDLE};
            if (storage_layout) layouts[1] = storage_layout->layout;
            const VkPushConstantRange ranges[] = {
                {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 16},
                {VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(float) * 16, fragment_push_constant_size}};
            VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layout_info.setLayoutCount = storage_layout ? 2u : 1u;
            layout_info.pSetLayouts = layouts;
            layout_info.pushConstantRangeCount = fragment_push_constant_size > 0 ? 2u : 1u;
            layout_info.pPushConstantRanges = ranges;
            if (vkCreatePipelineLayout(device_, &layout_info, nullptr, &result.layout) != VK_SUCCESS)
            {
                error = "Unable to create Vulkan lighting pipeline layout.";
                return false;
            }
        }
        else if (!create_pipeline_layout(&descriptor_layout, result.layout, error)) return false;

        VkPipelineShaderStageCreateInfo stages[2] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertex.module, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragment.module, "main", nullptr},
        };
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = three_dimensional ? sizeof(Vulkan3DVertex) : sizeof(Vertex2D);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        VkVertexInputAttributeDescription attributes[3] = {
            {0, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<std::uint32_t>(offsetof(Vertex2D, x))},
            {1, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<std::uint32_t>(offsetof(Vertex2D, u))},
            {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<std::uint32_t>(offsetof(Vertex2D, r))},
        };
        if (three_dimensional)
        {
            attributes[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<std::uint32_t>(offsetof(Vulkan3DVertex, position))};
            attributes[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<std::uint32_t>(offsetof(Vulkan3DVertex, colour))};
        }
        VkPipelineVertexInputStateCreateInfo vertex_input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertex_input.vertexBindingDescriptionCount = 1;
        vertex_input.pVertexBindingDescriptions = &binding;
        vertex_input.vertexAttributeDescriptionCount = three_dimensional ? 2u : 3u;
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
        blend_attachment.srcColorBlendFactor = premultiplied_alpha ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_SRC_ALPHA;
        blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blending.attachmentCount = 1;
        blending.pAttachments = &blend_attachment;
        VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depth.depthTestEnable = three_dimensional ? VK_TRUE : VK_FALSE;
        depth.depthWriteEnable = three_dimensional ? VK_TRUE : VK_FALSE;
        depth.depthCompareOp = VK_COMPARE_OP_LESS;
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
        pipeline_info.pDepthStencilState = &depth;
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
            render_pass_stack_.clear();
            destroy_image(depth_image_);
            if (render_pass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, render_pass_, nullptr);
            if (resume_render_pass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, resume_render_pass_, nullptr);
            if (offscreen_render_pass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, offscreen_render_pass_, nullptr);
            if (resume_offscreen_render_pass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, resume_offscreen_render_pass_, nullptr);
            render_pass_ = VK_NULL_HANDLE;
            resume_render_pass_ = VK_NULL_HANDLE;
            offscreen_render_pass_ = VK_NULL_HANDLE;
            resume_offscreen_render_pass_ = VK_NULL_HANDLE;
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
        depth_format_ = VK_FORMAT_UNDEFINED;
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

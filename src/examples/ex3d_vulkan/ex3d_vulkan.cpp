#include "sl.h"
#include "scene3d.h"
#include "renderer.h"
#include "vulkan_context.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>

namespace
{
    using Vertex = sl::detail::Vulkan3DVertex;

    void append_face(std::array<Vertex, 36> &vertices, int &offset, const float corners[8][3],
                     int a, int b, int c, int d, const float colour[3])
    {
        const int indices[] = {a, b, c, a, c, d};
        for (int index : indices)
        {
            for (int component = 0; component < 3; ++component)
            {
                vertices[static_cast<std::size_t>(offset)].position[component] = corners[index][component];
                vertices[static_cast<std::size_t>(offset)].colour[component] = colour[component];
            }
            ++offset;
        }
    }
}

int main(int argc, char *argv[])
{
    if (!sl::set_graphics_backend(sl::GraphicsBackend::vulkan) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        std::fprintf(stderr, "Failed to set Vulkan graphics backend or graphics mode.\n");  
        return -1;
    }

    sl::detail::VulkanContext *context = sl::detail::active_renderer()->vulkan_context();
    if (!context)
    {
        sl::shutdown();
        return -1;
    }

    sl::detail::VulkanShaderModule vertex_module;
    sl::detail::VulkanShaderModule fragment_module;
    sl::detail::VulkanDescriptorSetLayout descriptor_layout;
    sl::detail::VulkanGraphicsPipeline pipeline;
    sl::detail::VulkanBuffer vertex_buffer;
    std::string error;
    const std::filesystem::path shader_dir = SIMLIB_SHADER_BINARY_DIR;
    if (!context->load_shader_module(shader_dir / "vulkan/vulkan_ex3d.vert.spv", vertex_module, error) ||
        !context->load_shader_module(shader_dir / "vulkan/vulkan_ex3d.frag.spv", fragment_module, error) ||
        !context->create_dynamic_descriptor_layout({}, descriptor_layout, error) ||
        !context->create_graphics_pipeline(vertex_module, fragment_module, descriptor_layout,
            sl::detail::PrimitiveType::triangles, pipeline, error, nullptr, 0, true))
    {
        std::fprintf(stderr, "Vulkan 3D setup failed: %s\n", error.c_str());
        context->destroy_graphics_pipeline(pipeline);
        context->destroy_descriptor_set_layout(descriptor_layout);
        context->destroy_shader_module(vertex_module);
        context->destroy_shader_module(fragment_module);
        sl::shutdown();
        return -1;
    }

    const float corners[8][3] = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}};
    const float colours[6][3] = {
        {0.95f, 0.25f, 0.25f}, {0.25f, 0.75f, 1.0f}, {0.35f, 0.95f, 0.45f},
        {1.0f, 0.75f, 0.2f}, {0.75f, 0.35f, 1.0f}, {0.2f, 0.9f, 0.85f}};
    std::array<Vertex, 36> vertices{};
    int offset = 0;
    append_face(vertices, offset, corners, 0, 1, 2, 3, colours[0]);
    append_face(vertices, offset, corners, 5, 4, 7, 6, colours[1]);
    append_face(vertices, offset, corners, 4, 0, 3, 7, colours[2]);
    append_face(vertices, offset, corners, 1, 5, 6, 2, colours[3]);
    append_face(vertices, offset, corners, 3, 2, 6, 7, colours[4]);
    append_face(vertices, offset, corners, 4, 5, 1, 0, colours[5]);
    if (!context->create_buffer(sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertex_buffer, error) ||
        !context->upload_buffer(vertex_buffer, vertices.data(), sizeof(vertices), error))
    {
        std::fprintf(stderr, "Vulkan 3D buffer setup failed: %s\n", error.c_str());
        context->destroy_buffer(vertex_buffer);
        context->destroy_graphics_pipeline(pipeline);
        context->destroy_descriptor_set_layout(descriptor_layout);
        context->destroy_shader_module(vertex_module);
        context->destroy_shader_module(fragment_module);
        sl::shutdown();
        return -1;
    }

    sl::Camera camera;
    bool running = true;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            sl::display_handle_event(event);
        }

        const float time = static_cast<float>(sl::time_ms()) * 0.001f;
        const glm::mat4 model = sl::model_matrix({0.0f, 0.0f, 0.0f}, {time * 0.7f, time, time * 0.35f});
        const glm::mat4 view = camera.view_matrix();
        const glm::mat4 projection = camera.projection_matrix(800.0f / 600.0f);
        const glm::mat4 mvp = projection * view * model;
        std::string frame_error;
        if (context->begin_frame(frame_error))
        {
            context->clear_active_frame(0.035f, 0.05f, 0.08f, 1.0f, frame_error);
            context->record_3d_draw(pipeline.pipeline, pipeline.layout, vertex_buffer.buffer,
                36, glm::value_ptr(mvp), frame_error);
            sl::gprintf_center(32, {232, 236, 244}, "Vulkan 3D example - Escape to exit");
            context->end_frame(frame_error);
        }
    }

    context->destroy_buffer(vertex_buffer);
    context->destroy_graphics_pipeline(pipeline);
    context->destroy_descriptor_set_layout(descriptor_layout);
    context->destroy_shader_module(vertex_module);
    context->destroy_shader_module(fragment_module);
    sl::shutdown();
    return 0;
}
/** @file
 * @brief Selects and creates the active Renderer backend implementation.
 */
#include "renderer.h"
#include "gl_renderer.h"
#include "vulkan_renderer.h"
#ifdef _WIN32
#include "d3d11_renderer.h"
#include "d3d12_renderer.h"
#endif

namespace sl::detail
{
    std::unique_ptr<Renderer> create_renderer(GraphicsBackend backend)
    {
        switch (backend)
        {
        case GraphicsBackend::opengl:
            return create_gl_renderer();
        case GraphicsBackend::vulkan:
            return create_vulkan_renderer();
        case GraphicsBackend::d3d11:
#ifdef _WIN32
            return create_d3d11_renderer();
#else
            return nullptr;
#endif
        case GraphicsBackend::d3d12:
#ifdef _WIN32
            return create_d3d12_renderer();
#else
            return nullptr;
#endif
        default:
            return nullptr;
        }
    }
}

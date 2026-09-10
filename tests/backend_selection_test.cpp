#include "display.h"

#include <cassert>

int main()
{
    assert(sl::graphics_backend() == sl::GraphicsBackend::opengl);
    assert(sl::graphics_backend_available(sl::GraphicsBackend::opengl));
    assert(sl::graphics_backend_available(sl::GraphicsBackend::vulkan));

    char program[] = "backend_selection_test";
    char vulkan_argument[] = "--vulkan";
    char *arguments[] = {program, vulkan_argument};
    assert(sl::configure_graphics_backend_from_args(2, arguments));
    assert(sl::graphics_backend() == sl::GraphicsBackend::vulkan);

#ifndef _WIN32
    assert(!sl::graphics_backend_available(sl::GraphicsBackend::d3d11));
    assert(!sl::graphics_backend_available(sl::GraphicsBackend::d3d12));
#endif
    return 0;
}
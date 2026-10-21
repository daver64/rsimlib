#include "display.h"

#include <cassert>

int main()
{
    assert(sl::GFX_AUTODETECT_WINDOWED != sl::GFX_AUTODETECT_FULLSCREEN);

#ifdef SIMLIB_RENDERER_GL
    constexpr bool gl = true;
#else
    constexpr bool gl = false;
#endif
#ifdef SIMLIB_RENDERER_VULKAN
    constexpr bool vulkan = true;
#else
    constexpr bool vulkan = false;
#endif
#if defined(SIMLIB_RENDERER_D3D12) && defined(_WIN32)
    constexpr bool d3d12 = true;
#else
    constexpr bool d3d12 = false;
#endif
    assert(gl || vulkan || d3d12);

    assert(sl::graphics_backend_available(sl::GraphicsBackend::opengl) == gl);
    assert(sl::graphics_backend_available(sl::GraphicsBackend::vulkan) == vulkan);
    assert(sl::graphics_backend_available(sl::GraphicsBackend::d3d12) == d3d12);

    // The default is the first enabled renderer; D3D12 is preferred on Windows.
    const sl::GraphicsBackend expected_default = d3d12 ? sl::GraphicsBackend::d3d12
                                                 : gl  ? sl::GraphicsBackend::opengl
                                                       : sl::GraphicsBackend::vulkan;
    assert(sl::graphics_backend() == expected_default);

    char program[] = "backend_selection_test";
    char gl_argument[] = "--gl";
    char vulkan_argument[] = "--vulkan";
    char d3d12_argument[] = "--d3d12";

    char *vulkan_arguments[] = {program, vulkan_argument};
    assert(sl::configure_graphics_backend_from_args(2, vulkan_arguments) == vulkan);
    if (vulkan)
        assert(sl::graphics_backend() == sl::GraphicsBackend::vulkan);

    char *gl_arguments[] = {program, gl_argument};
    assert(sl::configure_graphics_backend_from_args(2, gl_arguments) == gl);

    char *d3d12_arguments[] = {program, d3d12_argument};
    assert(sl::configure_graphics_backend_from_args(2, d3d12_arguments) == d3d12);
    return 0;
}

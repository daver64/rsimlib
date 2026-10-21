/** @file
 * @brief Selects and creates the active Renderer backend implementation.
 */
#include "renderer.h"
#include "gl_renderer.h"
#include "vulkan_renderer.h"
#ifdef _WIN32
#include "d3d12_renderer.h"
#endif

namespace sl::detail
{
    std::vector<std::uint8_t> build_mip_chain(const std::uint8_t *pixels, int width, int height)
    {
        std::vector<std::uint8_t> chain;
        if (!pixels || width <= 0 || height <= 0)
            return chain;
        const int levels = mip_level_count(width, height);
        std::size_t total = 0;
        for (int level = 1; level < levels; ++level)
            total += static_cast<std::size_t>(std::max(width >> level, 1)) * std::max(height >> level, 1) * 4;
        chain.resize(total);

        const std::uint8_t *source = pixels;
        std::uint8_t *destination = chain.data();
        int source_width = width;
        int source_height = height;
        for (int level = 1; level < levels; ++level)
        {
            const int destination_width = std::max(width >> level, 1);
            const int destination_height = std::max(height >> level, 1);
            for (int y = 0; y < destination_height; ++y)
            {
                const int y0 = std::min(y * 2, source_height - 1);
                const int y1 = std::min(y * 2 + 1, source_height - 1);
                for (int x = 0; x < destination_width; ++x)
                {
                    const int x0 = std::min(x * 2, source_width - 1);
                    const int x1 = std::min(x * 2 + 1, source_width - 1);
                    const std::uint8_t *taps[4] = {
                        source + (static_cast<std::size_t>(y0) * source_width + x0) * 4,
                        source + (static_cast<std::size_t>(y0) * source_width + x1) * 4,
                        source + (static_cast<std::size_t>(y1) * source_width + x0) * 4,
                        source + (static_cast<std::size_t>(y1) * source_width + x1) * 4};
                    unsigned weighted[3] = {0, 0, 0};
                    unsigned plain[3] = {0, 0, 0};
                    unsigned alpha = 0;
                    for (const std::uint8_t *tap : taps)
                    {
                        for (int channel = 0; channel < 3; ++channel)
                        {
                            weighted[channel] += tap[channel] * tap[3];
                            plain[channel] += tap[channel];
                        }
                        alpha += tap[3];
                    }
                    std::uint8_t *out = destination + (static_cast<std::size_t>(y) * destination_width + x) * 4;
                    for (int channel = 0; channel < 3; ++channel)
                        out[channel] = static_cast<std::uint8_t>(
                            alpha > 0 ? (weighted[channel] + alpha / 2) / alpha : (plain[channel] + 2) / 4);
                    out[3] = static_cast<std::uint8_t>((alpha + 2) / 4);
                }
            }
            source = destination;
            source_width = destination_width;
            source_height = destination_height;
            destination += static_cast<std::size_t>(destination_width) * destination_height * 4;
        }
        return chain;
    }

    std::unique_ptr<Renderer> create_renderer(GraphicsBackend backend)
    {
        switch (backend)
        {
        case GraphicsBackend::opengl:
            return create_gl_renderer();
        case GraphicsBackend::vulkan:
            return create_vulkan_renderer();
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

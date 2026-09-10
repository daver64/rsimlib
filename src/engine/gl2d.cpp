#include "gl2d.h"

namespace sl::detail
{
    void gl2d_ortho_matrix(int windowWidth, int windowHeight, float *outMatrix16)
    {
        const float width = static_cast<float>(windowWidth);
        const float height = static_cast<float>(windowHeight);
        for (int index = 0; index < 16; ++index) outMatrix16[index] = 0.0f;
        outMatrix16[0] = width > 0.0f ? 2.0f / width : 0.0f;
        outMatrix16[5] = height > 0.0f ? -2.0f / height : 0.0f;
        outMatrix16[10] = -1.0f;
        outMatrix16[12] = -1.0f;
        outMatrix16[13] = 1.0f;
        outMatrix16[15] = 1.0f;
    }

    bool gl2d_init()
    {
        Renderer *renderer = active_renderer();
        return renderer && renderer->initialise_2d();
    }

    void gl2d_shutdown()
    {
        if (Renderer *renderer = active_renderer()) renderer->shutdown_2d();
    }

    void gl2d_begin(int windowWidth, int windowHeight)
    {
        if (Renderer *renderer = active_renderer()) renderer->begin_2d(windowWidth, windowHeight);
    }

    void gl2d_submit(std::uint32_t primitiveMode, const GLVertex *vertices, int count, std::uint32_t texture)
    {
        if (Renderer *renderer = active_renderer()) renderer->submit_2d(primitiveMode, vertices, count, texture);
    }
}

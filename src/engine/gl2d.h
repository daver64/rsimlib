#pragma once

#include <cstdint>

namespace simlib
{
    namespace detail
    {

        /** One 2D vertex: screen-space position, texture coordinate, and RGBA colour (0-1 floats). */
        struct GLVertex
        {
            float x = 0.0f;
            float y = 0.0f;
            float u = 0.0f;
            float v = 0.0f;
            float r = 1.0f;
            float g = 1.0f;
            float b = 1.0f;
            float a = 1.0f;
        };

        /** Lazily create the shared VAO/VBO, default shader, and 1x1 white texture. */
        bool gl2d_init();
        /** Release the shared 2D renderer's GPU resources. */
        void gl2d_shutdown();

        /** Bind the default textured/flat-colour shader and set its orthographic projection. */
        void gl2d_begin(int windowWidth, int windowHeight);

        /**
         * Upload vertices to the shared VBO and draw them with whichever shader program is
         * currently bound. Pass texture = 0 to sample the built-in white pixel (flat colour).
         */
        void gl2d_submit(std::uint32_t primitiveMode, const GLVertex *vertices, int count, std::uint32_t texture = 0);

        /** Compute a top-left-origin orthographic projection matrix (column-major, 16 floats). */
        void gl2d_ortho_matrix(int windowWidth, int windowHeight, float *outMatrix16);

    } // namespace detail
} // namespace simlib

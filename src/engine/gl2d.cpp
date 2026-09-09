#define GL_GLEXT_PROTOTYPES

#include "gl2d.h"

#include "graphics_fx.h"

#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

namespace simlib
{
    namespace detail
    {
        namespace
        {

            constexpr const char *default_vertex_source = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;

uniform mat4 uProjection;

out vec2 vTexCoord;
out vec4 vColor;

void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)";

            constexpr const char *default_fragment_source = R"(
#version 330 core
in vec2 vTexCoord;
in vec4 vColor;

uniform sampler2D uTexture;

out vec4 fragColor;

void main() {
    fragColor = texture(uTexture, vTexCoord) * vColor;
}
)";

            bool initialised = false;
            GLuint vao = 0;
            GLuint vbo = 0;
            GLuint whiteTexture = 0;
            Shader *defaultShader = nullptr;

            GLuint create_white_texture()
            {
                GLuint texture = 0;
                const unsigned char whitePixel[4] = {255, 255, 255, 255};
                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
                return texture;
            }

        } // namespace

        void gl2d_ortho_matrix(int windowWidth, int windowHeight, float *outMatrix16)
        {
            const float width = static_cast<float>(windowWidth);
            const float height = static_cast<float>(windowHeight);
            for (int i = 0; i < 16; ++i)
            {
                outMatrix16[i] = 0.0f;
            }
            // top-left-origin orthographic projection, matching the retired glOrtho(0, w, h, 0, -1, 1)
            outMatrix16[0] = width > 0.0f ? 2.0f / width : 0.0f;
            outMatrix16[5] = height > 0.0f ? -2.0f / height : 0.0f;
            outMatrix16[10] = -1.0f;
            outMatrix16[12] = -1.0f;
            outMatrix16[13] = 1.0f;
            outMatrix16[15] = 1.0f;
        }

        bool gl2d_init()
        {
            if (initialised)
            {
                return true;
            }

            defaultShader = new Shader(default_vertex_source, default_fragment_source);
            if (!defaultShader->is_valid())
            {
                delete defaultShader;
                defaultShader = nullptr;
                return false;
            }

            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);

            glGenBuffers(1, &vbo);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLVertex), reinterpret_cast<void *>(offsetof(GLVertex, x)));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLVertex), reinterpret_cast<void *>(offsetof(GLVertex, u)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GLVertex), reinterpret_cast<void *>(offsetof(GLVertex, r)));

            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            whiteTexture = create_white_texture();

            initialised = true;
            return true;
        }

        void gl2d_shutdown()
        {
            if (!initialised)
            {
                return;
            }
            delete defaultShader;
            defaultShader = nullptr;
            if (vbo != 0)
            {
                glDeleteBuffers(1, &vbo);
                vbo = 0;
            }
            if (vao != 0)
            {
                glDeleteVertexArrays(1, &vao);
                vao = 0;
            }
            if (whiteTexture != 0)
            {
                glDeleteTextures(1, &whiteTexture);
                whiteTexture = 0;
            }
            initialised = false;
        }

        void gl2d_begin(int windowWidth, int windowHeight)
        {
            if (!gl2d_init())
            {
                return;
            }

            float projection[16];
            gl2d_ortho_matrix(windowWidth, windowHeight, projection);
            defaultShader->set_uniform_mat4("uProjection", projection);
            defaultShader->set_uniform("uTexture", 0);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glActiveTexture(GL_TEXTURE0);
        }

        void gl2d_submit(std::uint32_t primitiveMode, const GLVertex *vertices, int count, std::uint32_t texture)
        {
            if (!gl2d_init() || !vertices || count <= 0)
            {
                return;
            }

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture != 0 ? static_cast<GLuint>(texture) : whiteTexture);

            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(GLVertex)) * count, vertices, GL_DYNAMIC_DRAW);
            glDrawArrays(static_cast<GLenum>(primitiveMode), 0, count);
            glBindVertexArray(0);
        }

    } // namespace detail
} // namespace simlib

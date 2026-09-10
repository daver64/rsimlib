#define GL_GLEXT_PROTOTYPES

#include "renderer.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#include <cstdint>

namespace sl::detail
{
    namespace
    {
        class OpenGLRenderer final : public Renderer
        {
        public:
            void configure_window() override
            {
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
            }

            bool initialise(SDL_Window *window, std::string &error) override
            {
                window_ = window;
                context_ = SDL_GL_CreateContext(window);
                if (!context_)
                {
                    error = SDL_GetError();
                    window_ = nullptr;
                    return false;
                }
                return true;
            }

            void shutdown() override
            {
                if (context_)
                {
                    SDL_GL_DeleteContext(context_);
                    context_ = nullptr;
                }
                window_ = nullptr;
            }

            void resize(int width, int height) override
            {
                glViewport(0, 0, width, height);
            }

            bool set_vsync(bool enabled) override
            {
                return context_ && SDL_GL_SetSwapInterval(enabled ? 1 : 0) == 0;
            }

            void present() override
            {
                if (window_)
                {
                    SDL_GL_SwapWindow(window_);
                }
            }

            SDL_GLContext native_context() const override
            {
                return context_;
            }

            bool create_texture(int width, int height, bool linear, std::uint32_t &texture) override
            {
                GLuint handle = 0;
                glGenTextures(1, &handle);
                if (handle == 0)
                {
                    return false;
                }
                glBindTexture(GL_TEXTURE_2D, handle);
                const GLint filter = linear ? GL_LINEAR : GL_NEAREST;
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                texture = handle;
                return true;
            }

            bool upload_texture(std::uint32_t texture, int width, int height,
                                const std::uint8_t *pixels) override
            {
                if (texture == 0 || !pixels)
                {
                    return false;
                }
                glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                                GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                return true;
            }

            void destroy_texture(std::uint32_t texture) override
            {
                if (texture != 0)
                {
                    const GLuint handle = static_cast<GLuint>(texture);
                    glDeleteTextures(1, &handle);
                }
            }

            bool create_render_target(int width, int height, std::uint32_t &texture,
                                      std::uint32_t &framebuffer) override
            {
                texture = 0;
                framebuffer = 0;
                if (!create_texture(width, height, true, texture))
                {
                    return false;
                }
                GLuint fbo = 0;
                glGenFramebuffers(1, &fbo);
                glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                       static_cast<GLuint>(texture), 0);
                const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                if (!complete)
                {
                    destroy_texture(texture);
                    texture = 0;
                    if (fbo != 0)
                    {
                        glDeleteFramebuffers(1, &fbo);
                    }
                    return false;
                }
                framebuffer = fbo;
                return true;
            }

            void destroy_render_target(std::uint32_t texture, std::uint32_t framebuffer) override
            {
                if (framebuffer != 0)
                {
                    const GLuint handle = static_cast<GLuint>(framebuffer);
                    glDeleteFramebuffers(1, &handle);
                }
                destroy_texture(texture);
            }

        private:
            SDL_Window *window_ = nullptr;
            SDL_GLContext context_ = nullptr;
        };
    }

    std::unique_ptr<Renderer> create_renderer()
    {
        return std::make_unique<OpenGLRenderer>();
    }
}

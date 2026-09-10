#include "renderer.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

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

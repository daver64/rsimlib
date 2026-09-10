/** @file
 * @brief Owns the SDL/OpenGL context lifecycle: creation, viewport, vsync, and swap.
 */
#include "gl_context.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

namespace sl::detail
{
    GLContext::~GLContext()
    {
        shutdown();
    }

    void GLContext::configure_window_attributes()
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    }

    bool GLContext::initialise(SDL_Window *window, std::string &error)
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

    void GLContext::shutdown()
    {
        if (context_)
        {
            SDL_GL_DeleteContext(context_);
            context_ = nullptr;
        }
        window_ = nullptr;
    }

    void GLContext::resize(int width, int height)
    {
        glViewport(0, 0, width, height);
    }

    bool GLContext::set_vsync(bool enabled)
    {
        return context_ && SDL_GL_SetSwapInterval(enabled ? 1 : 0) == 0;
    }

    void GLContext::present()
    {
        if (window_)
        {
            SDL_GL_SwapWindow(window_);
        }
    }

    void GLContext::clear(float red, float green, float blue, float alpha)
    {
        glClearColor(red, green, blue, alpha);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    bool GLContext::is_valid() const
    {
        return context_ != nullptr;
    }
}

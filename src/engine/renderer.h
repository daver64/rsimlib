#pragma once

#include <SDL2/SDL_video.h>

#include <memory>
#include <string>

namespace sl::detail
{
    /** Internal backend boundary for window/context and presentation ownership. */
    class Renderer
    {
    public:
        virtual ~Renderer() = default;

        /** Configure SDL window attributes before the window is created. */
        virtual void configure_window() = 0;
        /** Create the backend context for an existing SDL window. */
        virtual bool initialise(SDL_Window *window, std::string &error) = 0;
        /** Release backend resources. */
        virtual void shutdown() = 0;
        /** Resize the backend drawable viewport. */
        virtual void resize(int width, int height) = 0;
        /** Set the backend presentation interval. */
        virtual bool set_vsync(bool enabled) = 0;
        /** Present the current backend framebuffer. */
        virtual void present() = 0;
        /** Return the native context for integrations such as ImGui. */
        virtual SDL_GLContext native_context() const = 0;
    };

    /** Create the currently selected renderer backend. */
    std::unique_ptr<Renderer> create_renderer();
}

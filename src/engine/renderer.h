#pragma once

#include <SDL2/SDL_video.h>

#include <memory>
#include <cstdint>
#include <string>
#include <vector>

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
        /** Create an RGBA texture with backend-managed sampling state. */
        virtual bool create_texture(int width, int height, bool linear, std::uint32_t &texture) = 0;
        /** Upload RGBA8 pixels into a texture. */
        virtual bool upload_texture(std::uint32_t texture, int width, int height,
                        const std::uint8_t *pixels) = 0;
        /** Destroy a texture handle. */
        virtual void destroy_texture(std::uint32_t texture) = 0;
        /** Create a color render target and return its texture and framebuffer handles. */
        virtual bool create_render_target(int width, int height, std::uint32_t &texture,
                          std::uint32_t &framebuffer) = 0;
        /** Destroy a render target's framebuffer and texture handles. */
        virtual void destroy_render_target(std::uint32_t texture, std::uint32_t framebuffer) = 0;
    };

    /** Create the currently selected renderer backend. */
    std::unique_ptr<Renderer> create_renderer();
    /** Return the active renderer, or nullptr before display initialization. */
    Renderer *active_renderer();
}

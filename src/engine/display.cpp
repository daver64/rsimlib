/** @file
 * @brief Implements SDL window, OpenGL context, and display lifecycle functions.
 */

#include "display.h"

#include "draw.h"
#include "font.h"
#include "error.h"
#include "renderer.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_opengl.h>

#include <memory>
#include <string_view>

namespace sl
{

    extern Font *sl_default_monospace_font;

    namespace
    {

        SDL_Window *window = nullptr;
        std::unique_ptr<detail::Renderer> renderer;
        GraphicsBackend selected_backend = GraphicsBackend::opengl;
        bool ttfInitialized = false;
        int width = 0;
        int height = 0;
        int logicalWidth = 0;
        int logicalHeight = 0;
        int renderTargetWidth = 0;
        int renderTargetHeight = 0;
        float screenOffsetX = 0.0f;
        float screenOffsetY = 0.0f;
    } // namespace

    bool set_graphics_backend(GraphicsBackend backend)
    {
        if (window)
        {
            sl::detail::set_error("Graphics backend cannot change while the display is active");
            return false;
        }
        if (!graphics_backend_available(backend))
        {
            sl::detail::set_error("Requested graphics backend is not available");
            return false;
        }
        selected_backend = backend;
        return true;
    }

    bool configure_graphics_backend_from_args(int argc, char *argv[])
    {
        GraphicsBackend requested = GraphicsBackend::opengl;
        for (int index = 1; index < argc; ++index)
        {
            const std::string_view argument(argv[index]);
            if (argument == "--gl") requested = GraphicsBackend::opengl;
            else if (argument == "--vulkan") requested = GraphicsBackend::vulkan;
            else if (argument == "--d3d11") requested = GraphicsBackend::d3d11;
            else if (argument == "--d3d12") requested = GraphicsBackend::d3d12;
        }
        return set_graphics_backend(requested);
    }

    bool graphics_backend_available(GraphicsBackend backend)
    {
#ifdef _WIN32
        return backend == GraphicsBackend::opengl || backend == GraphicsBackend::vulkan ||
            backend == GraphicsBackend::d3d11 || backend == GraphicsBackend::d3d12;
#else
        return backend == GraphicsBackend::opengl || backend == GraphicsBackend::vulkan;
#endif
    }

    GraphicsBackend graphics_backend()
    {
        return selected_backend;
    }

    bool set_gfx_mode(int driver,
                      int requestedWidth, int requestedHeight,
                      int virtualWidth, int virtualHeight)
    {
        if (driver != GFX_AUTODETECT_WINDOWED || requestedWidth <= 0 || requestedHeight <= 0)
        {
            sl::detail::set_error("Invalid graphics mode or dimensions");
            return false;
        }

        display_shutdown();
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            sl::detail::set_error(SDL_GetError());
            return false;
        }

        if (TTF_Init() != 0)
        {
            sl::detail::set_error(TTF_GetError());
            SDL_Quit();
            return false;
        }
        ttfInitialized = true;

        renderer = detail::create_renderer(selected_backend);
        if (!renderer)
        {
            sl::detail::set_error("Requested graphics backend is not implemented");
            TTF_Quit();
            ttfInitialized = false;
            SDL_Quit();
            return false;
        }
        renderer->configure_window();
        uint32_t windowflags = renderer->window_flags();
        window = SDL_CreateWindow(
            "simlib",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            requestedWidth,
            requestedHeight,
            windowflags);
        if (!window)
        {
            sl::detail::set_error(SDL_GetError());
            TTF_Quit();
            ttfInitialized = false;
            SDL_Quit();
            return false;
        }

        std::string renderer_error;
        if (!renderer->initialise(window, renderer_error))
        {
            sl::detail::set_error(renderer_error);
            renderer->shutdown();
            renderer.reset();
            SDL_DestroyWindow(window);
            window = nullptr;
            TTF_Quit();
            ttfInitialized = false;
            SDL_Quit();
            return false;
        }

        SDL_GetWindowSize(window, &width, &height);
        logicalWidth = virtualWidth > 0 ? virtualWidth : width;
        logicalHeight = virtualHeight > 0 ? virtualHeight : height;
        if (!renderer->resize(width, height, renderer_error))
        {
            sl::detail::set_error(renderer_error);
            renderer->shutdown();
            renderer.reset();
            SDL_DestroyWindow(window);
            window = nullptr;
            TTF_Quit();
            ttfInitialized = false;
            SDL_Quit();
            return false;
        }
        detail::initialise_screen(logicalWidth, logicalHeight);
        sl_default_monospace_font = open_monospace_font(12);
        if (!sl_default_monospace_font)
        {
            sl::detail::set_error("Unable to load the default monospace font");
            display_shutdown();
            return false;
        }
        return true;
    }

    void display_handle_event(const Event &event)
    {
        if (event.type() != Event::Type::window_resized)
        {
            return;
        }

        width = event.window_width();
        height = event.window_height();
        if (renderer)
        {
            std::string error;
            if (!renderer->resize(width, height, error) && !error.empty())
                sl::detail::set_error(error);
        }
        detail::resize_screen(logicalWidth > 0 ? logicalWidth : width, logicalHeight > 0 ? logicalHeight : height);
    }

    void set_window_title(const char *title)
    {
        if (window && title)
            SDL_SetWindowTitle(window, title);
    }

    bool set_fullscreen(bool enabled)
    {
        if (!window || SDL_SetWindowFullscreen(window, enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0)
        {
            return false;
        }
        // fullscreen toggles don't reliably deliver a window_resized event, so sync the viewport now
        SDL_GetWindowSize(window, &width, &height);
        if (renderer)
        {
            std::string error;
            if (!renderer->resize(width, height, error) && !error.empty())
                sl::detail::set_error(error);
        }
        return true;
    }

    bool is_fullscreen()
    {
        return window && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
    }
    bool toggle_fullscreen()
    {
        if (!window)
            return false;
        bool currently_fullscreen = is_fullscreen();
        return set_fullscreen(!currently_fullscreen);
    }
    bool set_vsync(bool enabled)
    {
        return renderer && renderer->set_vsync(enabled);
    }

    int screen_width()
    {
        if (renderTargetWidth > 0)
        {
            return renderTargetWidth;
        }
        return logicalWidth > 0 ? logicalWidth : width;
    }

    int screen_height()
    {
        if (renderTargetHeight > 0)
        {
            return renderTargetHeight;
        }
        return logicalHeight > 0 ? logicalHeight : height;
    }

    int virtual_screen_width()
    {
        return logicalWidth > 0 ? logicalWidth : width;
    }

    int virtual_screen_height()
    {
        return logicalHeight > 0 ? logicalHeight : height;
    }

    void restore_window_viewport()
    {
        if (renderer && selected_backend == GraphicsBackend::opengl)
        {
            std::string error;
            renderer->resize(width, height, error);
        }
    }

    namespace detail
    {
        Renderer *active_renderer()
        {
            return renderer.get();
        }

        void set_render_target_size(int targetWidth, int targetHeight)
        {
            renderTargetWidth = targetWidth;
            renderTargetHeight = targetHeight;
        }

        void set_screen_offset(float x, float y)
        {
            screenOffsetX = x;
            screenOffsetY = y;
        }

        float screen_offset_x() { return screenOffsetX; }
        float screen_offset_y() { return screenOffsetY; }

    } // namespace detail

    void clear_to_colour(Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha)
    {
        if (renderer)
        {
            renderer->clear_frame(
                static_cast<float>(red) / 255.0f,
                static_cast<float>(green) / 255.0f,
                static_cast<float>(blue) / 255.0f,
                static_cast<float>(alpha) / 255.0f);
            return;
        }
        sl::detail::set_error("No active renderer is available to clear the display");
    }

    void show_video_bitmap()
    {
        if (renderer)
        {
            renderer->present();
        }
    }

    void wait_for_graphics()
    {
        if (renderer)
        {
            renderer->wait_idle();
        }
    }

    void display_shutdown()
    {
        wait_for_graphics();

        if (sl_default_monospace_font && ttfInitialized)
        {
            close_font(sl_default_monospace_font);
            sl_default_monospace_font = nullptr;
        }

        detail::destroy_screen();
        if (renderer)
        {
            renderer->shutdown();
            renderer.reset();
        }
        if (window)
        {
            SDL_DestroyWindow(window);
            window = nullptr;
        }

        if (ttfInitialized)
        {

            TTF_Quit();
            ttfInitialized = false;
        }
        width = 0;
        height = 0;
        logicalWidth = 0;
        logicalHeight = 0;
        renderTargetWidth = 0;
        renderTargetHeight = 0;
        SDL_Quit();
    }

    SDL_Window *get_window()
    {
        return window;
    }

    SDL_GLContext get_gl_context()
    {
        return renderer ? renderer->native_context() : nullptr;
    }

} // namespace sl
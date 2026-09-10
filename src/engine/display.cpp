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

namespace sl
{

    extern Font *sl_default_monospace_font;

    namespace
    {

        SDL_Window *window = nullptr;
        std::unique_ptr<detail::Renderer> renderer;
        bool ttfInitialized = false;
        int width = 0;
        int height = 0;
        int logicalWidth = 0;
        int logicalHeight = 0;
        int renderTargetWidth = 0;
        int renderTargetHeight = 0;
    } // namespace

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

        renderer = detail::create_renderer();
        renderer->configure_window();
        uint32_t windowflags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
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
        renderer->resize(width, height);
        detail::initialise_screen(logicalWidth, logicalHeight);
        sl_default_monospace_font = open_monospace_font(12);
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
            renderer->resize(width, height);
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
            renderer->resize(width, height);
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
        if (renderer)
        {
            renderer->resize(width, height);
        }
    }

    namespace detail
    {
        void set_render_target_size(int targetWidth, int targetHeight)
        {
            renderTargetWidth = targetWidth;
            renderTargetHeight = targetHeight;
        }
    } // namespace detail

    void clear_to_colour(Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha)
    {
        glClearColor(
            static_cast<float>(red) / 255.0f,
            static_cast<float>(green) / 255.0f,
            static_cast<float>(blue) / 255.0f,
            static_cast<float>(alpha) / 255.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void show_video_bitmap()
    {
        if (renderer)
        {
            renderer->present();
        }
    }

    void display_shutdown()
    {
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

        if (sl_default_monospace_font && ttfInitialized)
        {
            TTF_CloseFont(sl_default_monospace_font);
            sl_default_monospace_font = nullptr;
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
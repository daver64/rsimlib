#include "display.h"

#include "draw.h"
#include "font.h"
#include "error.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_opengl.h>

namespace simlib {

extern Font *sl_default_monospace_font;

namespace {

SDL_Window* window = nullptr;
SDL_GLContext context = nullptr;
bool ttfInitialized = false;
int width = 0;
int height = 0;
int logicalWidth = 0;
int logicalHeight = 0;
int renderTargetWidth = 0;
int renderTargetHeight = 0;
} // namespace

bool set_gfx_mode(int driver, int requestedWidth, int requestedHeight, int virtualWidth, int virtualHeight) {
    if (driver != GFX_AUTODETECT_WINDOWED || requestedWidth <= 0 || requestedHeight <= 0) {
        simlib::detail::set_error("Invalid graphics mode or dimensions");
        return false;
    }

    display_shutdown();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        simlib::detail::set_error(SDL_GetError());
        return false;
    }

    if (TTF_Init() != 0) {
        simlib::detail::set_error(TTF_GetError());
        SDL_Quit();
        return false;
    }
    ttfInitialized = true;

    // require a core profile so no legacy fixed-function GL state is available
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    window = SDL_CreateWindow(
        "simlib",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        requestedWidth,
        requestedHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        simlib::detail::set_error(SDL_GetError());
        TTF_Quit();
        ttfInitialized = false;
        SDL_Quit();
        return false;
    }

    context = SDL_GL_CreateContext(window);
    if (!context) {
        simlib::detail::set_error(SDL_GetError());
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
    glViewport(0, 0, width, height);
    detail::initialise_screen(logicalWidth, logicalHeight);
    sl_default_monospace_font = open_monospace_font(12);
    return true;
}

void display_handle_event(const Event& event) {
    if (event.type() != Event::Type::window_resized) {
        return;
    }

    width = event.window_width();
    height = event.window_height();
    glViewport(0, 0, width, height);
    detail::resize_screen(logicalWidth > 0 ? logicalWidth : width, logicalHeight > 0 ? logicalHeight : height);
}

void set_window_title(const char* title) {
    if (window && title) SDL_SetWindowTitle(window, title);
}

bool set_fullscreen(bool enabled) {
    return window && SDL_SetWindowFullscreen(window, enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) == 0;
}

bool is_fullscreen() {
    return window && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
}
bool toggle_fullscreen() {
    if (!window) return false;
    bool currently_fullscreen = is_fullscreen();
    return set_fullscreen(!currently_fullscreen);
}
bool set_vsync(bool enabled) {
    return context && SDL_GL_SetSwapInterval(enabled ? 1 : 0) == 0;
}

int screen_width() {
    if (renderTargetWidth > 0) {
        return renderTargetWidth;
    }
    return logicalWidth > 0 ? logicalWidth : width;
}

int screen_height() {
    if (renderTargetHeight > 0) {
        return renderTargetHeight;
    }
    return logicalHeight > 0 ? logicalHeight : height;
}

int virtual_screen_width() {
    return logicalWidth > 0 ? logicalWidth : width;
}

int virtual_screen_height() {
    return logicalHeight > 0 ? logicalHeight : height;
}

void restore_window_viewport() {
    glViewport(0, 0, width, height);
}

namespace detail {
void set_render_target_size(int targetWidth, int targetHeight) {
    renderTargetWidth = targetWidth;
    renderTargetHeight = targetHeight;
}
} // namespace detail

void clear_to_colour(Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha) {
    glClearColor(
        static_cast<float>(red) / 255.0f,
        static_cast<float>(green) / 255.0f,
        static_cast<float>(blue) / 255.0f,
        static_cast<float>(alpha) / 255.0f
    );
    glClear(GL_COLOR_BUFFER_BIT);
}

void show_video_bitmap() {
    if (window) {
        SDL_GL_SwapWindow(window);
    }
}

void display_shutdown() {
    detail::destroy_screen();
    if (context) {
        SDL_GL_DeleteContext(context);
        context = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    if (sl_default_monospace_font && ttfInitialized) {
        TTF_CloseFont(sl_default_monospace_font);
        sl_default_monospace_font = nullptr;
    }

    if (ttfInitialized) {
        
        TTF_Quit();
        ttfInitialized = false;
    }
    width = 0;
    height = 0;
    logicalWidth = 0;
    logicalHeight = 0;
    SDL_Quit();
}

SDL_Window* get_window() {
    return window;
}

SDL_GLContext get_gl_context() {
    return context;
}

} // namespace simlib
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

void display_handle_event(const SDL_Event& event) {
    if (event.type != SDL_WINDOWEVENT || event.window.event != SDL_WINDOWEVENT_SIZE_CHANGED) {
        return;
    }

    width = event.window.data1;
    height = event.window.data2;
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

bool set_vsync(bool enabled) {
    return context && SDL_GL_SetSwapInterval(enabled ? 1 : 0) == 0;
}

int screen_width() {
    return logicalWidth > 0 ? logicalWidth : width;
}

int screen_height() {
    return logicalHeight > 0 ? logicalHeight : height;
}

int virtual_screen_width() {
    return logicalWidth > 0 ? logicalWidth : width;
}

int virtual_screen_height() {
    return logicalHeight > 0 ? logicalHeight : height;
}

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
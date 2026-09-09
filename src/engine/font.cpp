/** @file
 * @brief Implements font discovery, text rendering, and text caching.
 */

#include "font.h"

#include "display.h"
#include "error.h"
#include "gl2d.h"
#include "resource.h"

#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#include <filesystem>
#include <fstream>
#include <cstdarg>
#include <cstdio>

namespace sl
{

    /** Temporary OpenGL texture used to draw one rendered text string. */
    struct TextTexture
    {
        GLuint id = 0;
        int width = 0;
        int height = 0;
    };

    /** Convert a UTF-8 string into a temporary GPU text texture. */
    std::optional<TextTexture> build_text_texture(Font *font, const std::string &text, const Colour &colour)
    {
        const SDL_Color sdlColor{colour.red, colour.green, colour.blue, colour.alpha};
        SDL_Surface *rendered = TTF_RenderUTF8_Blended(font, text.c_str(), sdlColor);
        if (!rendered)
        {
            sl::detail::set_error(TTF_GetError());
            return std::nullopt;
        }

        SDL_Surface *rgba = SDL_ConvertSurfaceFormat(rendered, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(rendered);
        if (!rgba)
        {
            sl::detail::set_error(SDL_GetError());
            return std::nullopt;
        }

        TextTexture texture;
        texture.width = rgba->w;
        texture.height = rgba->h;

        glGenTextures(1, &texture.id);
        glBindTexture(GL_TEXTURE_2D, texture.id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            texture.width,
            texture.height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            rgba->pixels);

        SDL_FreeSurface(rgba);
        return texture;
    }

    /** Release a temporary text texture. */
    void destroy_text_texture(TextTexture &texture)
    {
        if (texture.id != 0)
        {
            glDeleteTextures(1, &texture.id);
            texture.id = 0;
        }
    }

    /** Draw a temporary text texture in screen coordinates. */
    void draw_text_texture(const TextTexture &texture, int x, int y, int windowWidth, int windowHeight)
    {
        if (texture.id == 0)
        {
            return;
        }

        detail::gl2d_begin(windowWidth, windowHeight);

        const float x0 = static_cast<float>(x);
        const float y0 = static_cast<float>(y);
        const float x1 = static_cast<float>(x + texture.width);
        const float y1 = static_cast<float>(y + texture.height);

        const detail::GLVertex vertices[4] = {
            {x0, y0, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f},
            {x1, y0, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f},
            {x1, y1, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
            {x0, y1, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
        };
        detail::gl2d_submit(GL_TRIANGLE_FAN, vertices, 4, texture.id);
    }

    /** Draw a solid rectangle behind rendered text. */
    void fill_rect(int x, int y, int width, int height, int windowWidth, int windowHeight, const Colour &colour)
    {
        if (width <= 0 || height <= 0)
        {
            return;
        }

        detail::gl2d_begin(windowWidth, windowHeight);

        const float x0 = static_cast<float>(x);
        const float y0 = static_cast<float>(y);
        const float x1 = static_cast<float>(x + width);
        const float y1 = static_cast<float>(y + height);
        const float r = static_cast<float>(colour.red) / 255.0f;
        const float g = static_cast<float>(colour.green) / 255.0f;
        const float b = static_cast<float>(colour.blue) / 255.0f;
        const float a = static_cast<float>(colour.alpha) / 255.0f;

        const detail::GLVertex vertices[4] = {
            {x0, y0, 0.0f, 0.0f, r, g, b, a},
            {x1, y0, 0.0f, 0.0f, r, g, b, a},
            {x1, y1, 0.0f, 0.0f, r, g, b, a},
            {x0, y1, 0.0f, 0.0f, r, g, b, a},
        };
        detail::gl2d_submit(GL_TRIANGLE_FAN, vertices, 4);
    }

    std::optional<std::vector<unsigned char>> load_font()
    {
        std::vector<std::filesystem::path> candidates;

#ifdef _WIN32
        candidates = {
            R"(C:\Windows\Fonts\consola.ttf)",
            R"(C:\Windows\Fonts\lucon.ttf)",
            R"(C:\Windows\Fonts\cour.ttf)"};
#else
        candidates = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
            "/usr/share/fonts/truetype/liberation2/LiberationMono-Regular.ttf",
            "/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf"};
#endif

        for (const auto &fontPath : candidates)
        {
            if (!std::filesystem::exists(fontPath))
            {
                continue;
            }

            std::ifstream file(fontPath, std::ios::binary);
            if (!file)
            {
                continue;
            }

            std::vector<unsigned char> bytes(
                (std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>());

            if (!bytes.empty())
            {
                return bytes;
            }
        }

        return std::nullopt;
    }

    Font *sl_default_monospace_font = nullptr;

    Font *get_default_monospace_font()
    {
        return sl_default_monospace_font;
    }
    Font *open_monospace_font(int pointSize)
    {
        // TTF_OpenFontRW reads glyph data from this buffer for the font's entire
        // lifetime, so it must outlive this call rather than being freed here
        static const std::optional<std::vector<unsigned char>> fontBytes = load_font();
        if (!fontBytes)
        {
            return nullptr;
        }

        SDL_RWops *rw = SDL_RWFromConstMem(fontBytes->data(), static_cast<int>(fontBytes->size()));
        if (!rw)
        {
            return nullptr;
        }

        return TTF_OpenFontRW(rw, 1, pointSize);
    }

    Font *open_sans_font(int pointSize)
    {
#ifdef _WIN32
        static const std::vector<std::filesystem::path> candidates = {
            R"(C:\Windows\Fonts\segoeui.ttf)",
            R"(C:\Windows\Fonts\arial.ttf)"};
#else
        static const std::vector<std::filesystem::path> candidates = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf"};
#endif

        for (const std::filesystem::path &path : candidates)
        {
            if (std::filesystem::exists(path))
            {
                return open_font(path.string(), pointSize);
            }
        }
        return nullptr;
    }

    Font *open_font(const std::string &path, int pointSize)
    {
        Font *font = TTF_OpenFont(path.c_str(), pointSize);
        if (!font)
            sl::detail::set_error(TTF_GetError());
        return font;
    }

    Font *open_font_from_memory(const std::uint8_t *data, std::size_t size, int pointSize)
    {
        if (!data || size == 0 || size > std::numeric_limits<int>::max())
            return nullptr;
        SDL_RWops *rw = SDL_RWFromConstMem(data, static_cast<int>(size));
        Font *font = rw ? TTF_OpenFontRW(rw, 1, pointSize) : nullptr;
        if (!font)
            sl::detail::set_error(TTF_GetError());
        return font;
    }

    Font *open_font(const Archive &archive, const std::string &name, int pointSize)
    {
        const auto bytes = archive.read(name);
        return open_font_from_memory(bytes.data(), bytes.size(), pointSize);
    }

    void close_font(Font *font)
    {
        if (font)
            TTF_CloseFont(font);
    }

    int text_length(Font *font, const std::string &text)
    {
        int width = 0;
        int height = 0;
        return font && TTF_SizeUTF8(font, text.c_str(), &width, &height) == 0 ? width : 0;
    }

    int text_height(Font *font)
    {
        return font ? TTF_FontHeight(font) : 0;
    }

    void gl_printf(
        Font *font,
        int x,
        int y,
        const Colour &foreground,
        const Colour &background,
        int windowWidth,
        int windowHeight,
        const std::string &text)
    {
        if (!font || text.empty())
        {
            return;
        }

        const auto texture = build_text_texture(font, text, foreground);
        if (!texture)
        {
            return;
        }

        fill_rect(x, y, texture->width, texture->height, windowWidth, windowHeight, background);
        draw_text_texture(*texture, x, y, windowWidth, windowHeight);

        TextTexture mutableTexture = *texture;
        destroy_text_texture(mutableTexture);
    }

    void textout(Font *font, int x, int y, const Colour &colour, const std::string &text)
    {
        gl_printf(font, x, y, colour, {0, 0, 0, 0}, screen_width(), screen_height(), text);
    }

    void textprintf(Font *font, int x, int y, const Colour &colour, const char *format, ...)
    {
        if (!format)
            return;
        char buffer[1024];
        va_list arguments;
        va_start(arguments, format);
        std::vsnprintf(buffer, sizeof(buffer), format, arguments);
        va_end(arguments);
        textout(font, x, y, colour, buffer);
    }

    void gprintf(int x, int y, const Colour &colour, const char *fmt, ...)
    {
        if (!fmt)
            return;
        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        textout(get_default_monospace_font(), x, y, colour, buffer);
    }

    void gprintf_center(int y, const Colour &colour, const char *fmt, ...)
    {
        if (!fmt)
            return;
        Font *font = get_default_monospace_font();
        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        const int text_width = text_length(font, buffer);
        const int x = (screen->width - text_width) / 2;
        textout(font, x, y, colour, buffer);
    }

    struct TextCache
    {
        std::string text;
        Font *font = nullptr;
        Colour colour{};
        TextTexture texture;
        bool has_texture = false;
    };

    TextCache *create_text_cache()
    {
        return new TextCache();
    }

    void destroy_text_cache(TextCache *cache)
    {
        if (!cache)
        {
            return;
        }
        if (cache->has_texture)
        {
            destroy_text_texture(cache->texture);
        }
        delete cache;
    }

    void textout_cached(TextCache *cache, Font *font, int x, int y, const Colour &colour, const std::string &text)
    {
        if (!cache || !font || text.empty())
        {
            return;
        }

        const bool same_colour = cache->colour.red == colour.red && cache->colour.green == colour.green &&
                                 cache->colour.blue == colour.blue && cache->colour.alpha == colour.alpha;
        const bool up_to_date = cache->has_texture && cache->font == font && cache->text == text && same_colour;

        if (!up_to_date)
        {
            if (cache->has_texture)
            {
                destroy_text_texture(cache->texture);
                cache->has_texture = false;
            }
            const auto texture = build_text_texture(font, text, colour);
            if (!texture)
            {
                return;
            }
            cache->texture = *texture;
            cache->has_texture = true;
            cache->font = font;
            cache->text = text;
            cache->colour = colour;
        }

        draw_text_texture(cache->texture, x, y, screen_width(), screen_height());
    }

} // namespace sl
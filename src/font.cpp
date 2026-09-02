#include "font.h"

#include <SDL2/SDL_opengl.h>

#include <filesystem>
#include <fstream>

struct TextTexture {
    GLuint id = 0;
    int width = 0;
    int height = 0;
};

std::optional<TextTexture> build_text_texture(TTF_Font* font, const std::string& text, const simlib::draw::Colour& colour) {
    const SDL_Color sdlColor{colour.red, colour.green, colour.blue, colour.alpha};
    SDL_Surface* rendered = TTF_RenderUTF8_Blended(font, text.c_str(), sdlColor);
    if (!rendered) {
        return std::nullopt;
    }

    SDL_Surface* rgba = SDL_ConvertSurfaceFormat(rendered, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(rendered);
    if (!rgba) {
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
        rgba->pixels
    );

    SDL_FreeSurface(rgba);
    return texture;
}

void destroy_text_texture(TextTexture& texture) {
    if (texture.id != 0) {
        glDeleteTextures(1, &texture.id);
        texture.id = 0;
    }
}

void draw_text_texture(const TextTexture& texture, int x, int y, int windowWidth, int windowHeight) {
    if (texture.id == 0) {
        return;
    }

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(windowWidth), static_cast<double>(windowHeight), 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glColor3f(1.0f, 1.0f, 1.0f);

    const float x0 = static_cast<float>(x);
    const float y0 = static_cast<float>(y);
    const float x1 = static_cast<float>(x + texture.width);
    const float y1 = static_cast<float>(y + texture.height);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(x0, y0);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(x1, y0);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(x1, y1);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(x0, y1);
    glEnd();

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void fill_rect(int x, int y, int width, int height, int windowWidth, int windowHeight, const simlib::draw::Colour& colour) {
    if (width <= 0 || height <= 0) {
        return;
    }

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(windowWidth), static_cast<double>(windowHeight), 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4ub(colour.red, colour.green, colour.blue, colour.alpha);

    const float x0 = static_cast<float>(x);
    const float y0 = static_cast<float>(y);
    const float x1 = static_cast<float>(x + width);
    const float y1 = static_cast<float>(y + height);

    glBegin(GL_QUADS);
    glVertex2f(x0, y0);
    glVertex2f(x1, y0);
    glVertex2f(x1, y1);
    glVertex2f(x0, y1);
    glEnd();

    glDisable(GL_BLEND);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

std::optional<std::vector<unsigned char>> load_font() {
    std::vector<std::filesystem::path> candidates;

#ifdef _WIN32
    candidates = {
        R"(C:\Windows\Fonts\consola.ttf)",
        R"(C:\Windows\Fonts\lucon.ttf)",
        R"(C:\Windows\Fonts\cour.ttf)"
    };
#else
    candidates = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf"
    };
#endif

    for (const auto& fontPath : candidates) {
        if (!std::filesystem::exists(fontPath)) {
            continue;
        }

        std::ifstream file(fontPath, std::ios::binary);
        if (!file) {
            continue;
        }

        std::vector<unsigned char> bytes(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );

        if (!bytes.empty()) {
            return bytes;
        }
    }

    return std::nullopt;
}

TTF_Font* open_monospace_font(int pointSize) {
    const auto fontBytes = load_font();
    if (!fontBytes) {
        return nullptr;
    }

    SDL_RWops* rw = SDL_RWFromConstMem(fontBytes->data(), static_cast<int>(fontBytes->size()));
    if (!rw) {
        return nullptr;
    }

    return TTF_OpenFontRW(rw, 1, pointSize);
}

void gl_printf(
    TTF_Font* font,
    int x,
    int y,
    const simlib::draw::Colour& foreground,
    const simlib::draw::Colour& background,
    int windowWidth,
    int windowHeight,
    const std::string& text
) {
    if (!font || text.empty()) {
        return;
    }

    const auto texture = build_text_texture(font, text, foreground);
    if (!texture) {
        return;
    }

    fill_rect(x, y, texture->width, texture->height, windowWidth, windowHeight, background);
    draw_text_texture(*texture, x, y, windowWidth, windowHeight);

    TextTexture mutableTexture = *texture;
    destroy_text_texture(mutableTexture);
}
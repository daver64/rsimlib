#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "draw.h"

#include <optional>
#include <string>
#include <vector>

namespace simlib { class Archive; }

namespace simlib {

/** Load the first available platform monospace font into memory. */
std::optional<std::vector<unsigned char>> load_font();
/** Get the default monospace font. */
TTF_Font *get_default_monospace_font() ;
/** Open the loaded font bytes as an SDL_ttf font. */
TTF_Font* open_monospace_font(int pointSize);
/** Open a font from an explicit file path. */
TTF_Font* open_font(const std::string& path, int pointSize);
/** Open a font from memory. */
TTF_Font* open_font_from_memory(const std::uint8_t* data, std::size_t size, int pointSize);
/** Open a font entry from a ZIP archive. */
TTF_Font* open_font(const Archive& archive, const std::string& name, int pointSize);
/** Return the rendered width of UTF-8 text in pixels. */
int text_length(TTF_Font* font, const std::string& text);
/** Return the rendered height of a font in pixels. */
int text_height(TTF_Font* font);

/** Render text with a solid background rectangle in screen coordinates. */
void gl_printf(
    TTF_Font* font,
    int x,
    int y,
    const Colour& foreground,
    const Colour& background,
    int windowWidth,
    int windowHeight,
    const std::string& text
);

/** Draw UTF-8 text at a screen position without a background. */
void textout(TTF_Font* font, int x, int y, const Colour& colour, const std::string& text);
/** Format and draw UTF-8 text at a screen position without a background. */
void textprintf(TTF_Font* font, int x, int y, const Colour& colour, const char* format, ...);

} // namespace simlib
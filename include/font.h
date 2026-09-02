#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "draw.h"

#include <optional>
#include <string>
#include <vector>

/** Load the first available platform monospace font into memory. */
std::optional<std::vector<unsigned char>> load_font();
/** Open the loaded font bytes as an SDL_ttf font. */
TTF_Font* open_monospace_font(int pointSize);

/** Render text with a solid background rectangle in screen coordinates. */
void gl_printf(
    TTF_Font* font,
    int x,
    int y,
    const simlib::draw::Colour& foreground,
    const simlib::draw::Colour& background,
    int windowWidth,
    int windowHeight,
    const std::string& text
);
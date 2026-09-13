#pragma once

#include "game_types.h"

void draw_cell(sl::Bitmap *target, int cell_x, int cell_y, char character, const sl::Colour &foreground, const sl::Colour &background = Colours::Black);
void draw_text(sl::Bitmap *target, int x, int y, const std::string &text, const sl::Colour &colour);

void render_tile(sl::Bitmap *target, Game &game, int world_x, int world_y, int screen_x, int screen_y);
void render_actors(sl::Bitmap *target, Game &game);
void render_map(sl::Bitmap *target, Game &game);
void render_header(sl::Bitmap *target, Game &game);
void render_environment_panel(sl::Bitmap *target, Game &game);
void render_status(sl::Bitmap *target, Game &game);
void render_messages(sl::Bitmap *target, Game &game);
void render_ui(sl::Bitmap *target, Game &game);
void render_game(sl::Bitmap *target, Game &game);

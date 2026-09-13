#pragma once

#include "game_types.h"

void draw_cell(int cell_x, int cell_y, char character, const sl::Colour &foreground, const sl::Colour &background = Colours::Black);
void draw_text(int x, int y, const std::string &text, const sl::Colour &colour);

void render_tile(Game &game, int world_x, int world_y, int screen_x, int screen_y);
void render_actors(Game &game);
void render_map(Game &game);
void render_header(Game &game);
void render_environment_panel(Game &game);
void render_status(Game &game);
void render_messages(Game &game);
void render_ui(Game &game);
void render_game(Game &game);

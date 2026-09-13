#pragma once

#include "game_types.h"
#include <vector>

const char *terrain_name(Terrain terrain);
char terrain_glyph(Terrain terrain);
sl::Colour get_torch_colour(int x, int y);
sl::Colour terrain_colour(Terrain terrain, int x = 0, int y = 0);
bool terrain_blocks_movement(Terrain terrain);
bool terrain_blocks_sight(Terrain terrain);
void set_tile(Tile &tile, Terrain terrain);

Dungeon *find_dungeon_at(Game &game, int x, int y);
void enter_dungeon(Game &game);
void descend_dungeon(Game &game);
void ascend_dungeon(Game &game);

Tile *current_tile(Game &game, int x, int y);
Actor *actor_at(Game &game, int x, int y);
void attack(Game &game, Actor &attacker, Actor &defender);
bool try_player_move(Game &game, int dx, int dy);
bool line_of_sight(Game &game, int x0, int y0, int x1, int y1);
void clear_visibility(Game &game);
void update_fov(Game &game);
void monster_turns(Game &game);
void advance_turn(Game &game);
bool use_stairs(Game &game);
void update_camera(Game &game);
void build_lights(Game &game);
std::vector<sl::Light> build_simlib_lights(Game &game);
std::vector<sl::ShadowCaster> build_simlib_shadow_casters(Game &game);

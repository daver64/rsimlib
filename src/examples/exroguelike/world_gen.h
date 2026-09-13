#pragma once

#include "game_types.h"
#include <memory>
#include <vector>

void grid_to_geo(int x, int y, float &lon, float &lat);
void generate_overworld(Game &game);
void ensure_overworld_connectivity(Game &game, const std::vector<Position> &entrances);

void initialise_dungeon(DungeonLevel &level);
void carve_floor(DungeonLevel &level, int x, int y);
void carve_room(DungeonLevel &level, const Room &room);
void carve_horizontal(DungeonLevel &level, int x1, int x2, int y);
void carve_vertical(DungeonLevel &level, int y1, int y2, int x);
void connect_rooms(DungeonLevel &level, const Room &a, const Room &b);
Room make_room_in_sector(int sector_x, int sector_y);
std::unique_ptr<DungeonLevel> generate_dungeon_level(int depth);

Actor make_monster(Game &game, MonsterType type, int x, int y);
void populate_dungeon(Game &game);

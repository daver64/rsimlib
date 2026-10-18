#ifdef NDEBUG
#undef NDEBUG
#endif
#include "sl.h"

#include <cassert>
#include <filesystem>
#include <fstream>

int main()
{
    // Programmatic map: layers, cells, solidity, coordinate conversion and animation.
    sl::TileMap *map = sl::create_tilemap(4, 3, 32, 32);
    assert(map);
    assert(!sl::create_tilemap(0, 3, 32, 32));

    sl::Bitmap *image = sl::create_bitmap(64, 32);
    assert(image);
    const sl::Atlas atlas = sl::create_atlas(image, 32, 32);
    assert(atlas.tile_count == 2);
    assert(sl::tilemap_add_tileset(map, atlas) == 0);
    const int layer = sl::tilemap_add_layer(map, "ground");
    assert(layer == 0 && sl::tilemap_layer(map, "ground") == sl::tilemap_layer(map, 0));

    assert(sl::tilemap_set(map, layer, 1, 1, 2 | sl::tile_flip_horizontal));
    assert(!sl::tilemap_set(map, layer, 4, 0, 1));
    assert((sl::tilemap_get(map, layer, 1, 1) & sl::tile_id_mask) == 2);
    assert((sl::tilemap_get(map, layer, 1, 1) & sl::tile_flip_horizontal) != 0);
    assert(sl::tilemap_get(map, layer, -1, 0) == 0);

    sl::tilemap_set_solid(map, 2, true);
    assert(sl::tilemap_is_solid(map, 1, 1));
    assert(!sl::tilemap_is_solid(map, 0, 0));
    assert(sl::tilemap_is_solid(map, -1, 0));
    assert(sl::tilemap_rect_blocked(map, 40.0f, 40.0f, 8.0f, 8.0f));
    assert(!sl::tilemap_rect_blocked(map, 0.0f, 0.0f, 31.0f, 31.0f));
    assert(sl::tilemap_rect_blocked(map, 0.0f, 0.0f, 40.0f, 40.0f));

    int column = 0, row = 0;
    sl::tilemap_world_to_tile(map, 70.0f, 33.0f, column, row);
    assert(column == 2 && row == 1);
    float x = 0.0f, y = 0.0f;
    sl::tilemap_tile_to_world(map, 2, 1, x, y);
    assert(x == 64.0f && y == 32.0f);

    sl::TileInfo *info = sl::tilemap_tile_info(map, 1);
    assert(!info);
    sl::tilemap_set_solid(map, 1, false);
    info = sl::tilemap_tile_info(map, 1);
    assert(info);
    info->properties["kind"] = "grass";
    assert(sl::tilemap_tile_property(map, 1, "kind") == "grass");
    info->animation = {{0, 100}, {1, 100}};
    sl::update_tilemap(map, 0.15f);
    assert(map->animation_time_ms > 100.0);
    sl::destroy_tilemap(map);
    sl::destroy_bitmap(image);

    // Tiled JSON loader.
    const std::filesystem::path directory = std::filesystem::temp_directory_path() / "simlib_tilemap_test";
    std::filesystem::create_directories(directory);
    const std::string image_path = std::string(SIMLIB_SOURCE_DIR) + "/assets/textures/atlas_grid.png";
    {
        std::ofstream file(directory / "map.tmj");
        file << R"({
  "orientation": "orthogonal", "width": 2, "height": 2, "tilewidth": 32, "tileheight": 32,
  "tilesets": [{"firstgid": 1, "image": ")" << image_path << R"(", "tilewidth": 32, "tileheight": 32,
    "tiles": [{"id": 1, "properties": [{"name": "solid", "type": "bool", "value": true}, {"name": "hp", "type": "int", "value": 5}],
               "animation": [{"tileid": 1, "duration": 200}, {"tileid": 2, "duration": 200}]}]}],
  "layers": [
    {"type": "tilelayer", "name": "ground", "width": 2, "height": 2, "data": [1, 2, 3, 2147483652], "parallaxx": 0.5},
    {"type": "group", "name": "g", "layers": [{"type": "objectgroup", "name": "spawns",
       "objects": [{"name": "player", "class": "spawn", "x": 10, "y": 20, "properties": [{"name": "team", "type": "string", "value": "red"}]}]}]}
  ]
})";
    }
    sl::TileMap *loaded = sl::load_tiled_map((directory / "map.tmj").string());
    assert(loaded);
    assert(loaded->width == 2 && loaded->layers.size() == 1 && loaded->tilesets.size() == 1);
    assert(loaded->layers[0].parallax_x == 0.5f);
    assert((sl::tilemap_get(loaded, 0, 1, 1) & sl::tile_flip_horizontal) != 0);
    assert(sl::tilemap_is_solid(loaded, 1, 0));
    assert(sl::tilemap_tile_property(loaded, 2, "hp") == "5");
    assert(loaded->tilesets[0].tiles.at(1).animation.size() == 2);
    const sl::TileObject *spawn = sl::tilemap_find_object(loaded, "player");
    assert(spawn && spawn->type == "spawn" && spawn->x == 10.0f && spawn->properties.at("team") == "red");
    sl::destroy_tilemap(loaded);

    assert(!sl::load_tiled_map((directory / "missing.tmj").string()));
    std::filesystem::remove_all(directory);
    return 0;
}

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

    // Infinite maps: sparse chunks, negative coordinates, bounds and solidity.
    {
        sl::TileMap *infinite = sl::create_infinite_tilemap(16, 16);
        assert(infinite && infinite->infinite);
        assert(!sl::create_infinite_tilemap(16, 16, sl::TileMapOrientation::isometric));
        sl::Bitmap *infinite_image = sl::create_bitmap(96, 16);
        assert(sl::tilemap_add_tileset(infinite, sl::create_atlas(infinite_image, 16, 16)) == 0);
        const int ground = sl::tilemap_add_layer(infinite, "ground");
        assert(sl::tilemap_set(infinite, ground, -1, -1, 5));
        assert(sl::tilemap_set(infinite, ground, 100, 40, 6));
        assert(sl::tilemap_set(infinite, ground, -17, 0, 7));
        assert(sl::tilemap_get(infinite, ground, -1, -1) == 5 && sl::tilemap_get(infinite, ground, 100, 40) == 6);
        assert(sl::tilemap_get(infinite, ground, -17, 0) == 7 && sl::tilemap_get(infinite, ground, 3, 3) == 0);
        assert(infinite->layers[ground].chunks.size() == 3);
        int min_col = 0, min_row = 0, max_col = 0, max_row = 0;
        assert(sl::tilemap_layer_bounds(infinite, ground, min_col, min_row, max_col, max_row));
        assert(min_col == -17 && min_row == -1 && max_col == 100 && max_row == 40);
        assert(!sl::tilemap_is_solid(infinite, 5000, -5000));
        sl::tilemap_set_solid(infinite, 5, true);
        assert(sl::tilemap_is_solid(infinite, -1, -1));
        assert(sl::tilemap_rect_blocked(infinite, -12.0f, -12.0f, 4.0f, 4.0f));
        sl::destroy_tilemap(infinite);
        sl::destroy_bitmap(infinite_image);

        std::ofstream(directory / "infinite.tmj") << R"({"type":"map","orientation":"orthogonal","infinite":true,
            "width":0,"height":0,"tilewidth":16,"tileheight":16,"layers":[
            {"type":"tilelayer","name":"a","chunks":[
              {"x":-16,"y":0,"width":2,"height":2,"data":[3,0,0,4]},
              {"x":32,"y":16,"width":2,"height":1,"data":[0,9]}]}]})";
        sl::TileMap *loaded_infinite = sl::load_tiled_map((directory / "infinite.tmj").string());
        assert(loaded_infinite && loaded_infinite->infinite);
        assert(sl::tilemap_get(loaded_infinite, 0, -16, 0) == 3 && sl::tilemap_get(loaded_infinite, 0, -15, 1) == 4);
        assert(sl::tilemap_get(loaded_infinite, 0, 33, 16) == 9 && sl::tilemap_get(loaded_infinite, 0, 32, 16) == 0);
        sl::destroy_tilemap(loaded_infinite);
    }

    // Hexagonal maps: geometry for both stagger axes and the Tiled loader.
    {
        // Pointy-top: 32x32 bounding box with 16 px side, so rows step 24 px and odd rows shift 16 px.
        sl::TileMap *hex = sl::create_hex_tilemap(4, 4, 32, 32, 16, true, true);
        assert(hex && hex->orientation == sl::TileMapOrientation::hexagonal);
        assert(!sl::create_hex_tilemap(4, 4, 32, 32, 40));
        float hx = 0.0f, hy = 0.0f;
        sl::tilemap_tile_to_world(hex, 1, 0, hx, hy);
        assert(hx == 32.0f && hy == 0.0f);
        sl::tilemap_tile_to_world(hex, 1, 1, hx, hy);
        assert(hx == 48.0f && hy == 24.0f);
        int hc = 0, hr = 0;
        for (int row = 0; row < 4; ++row)
            for (int column = 0; column < 4; ++column)
            {
                sl::tilemap_tile_to_world(hex, column, row, hx, hy);
                sl::tilemap_world_to_tile(hex, hx + 16.0f, hy + 16.0f, hc, hr);
                assert(hc == column && hr == row);
            }
        sl::Bitmap *hex_image = sl::create_bitmap(32, 32);
        assert(sl::tilemap_add_tileset(hex, sl::create_atlas(hex_image, 32, 32)) == 0);
        const int layer_index = sl::tilemap_add_layer(hex, "g");
        sl::tilemap_set(hex, layer_index, 2, 1, 1);
        sl::tilemap_set_solid(hex, 1, true);
        sl::tilemap_tile_to_world(hex, 2, 1, hx, hy);
        assert(sl::tilemap_rect_blocked(hex, hx + 12.0f, hy + 12.0f, 8.0f, 8.0f));
        assert(!sl::tilemap_rect_blocked(hex, 12.0f, 12.0f, 8.0f, 8.0f));
        sl::destroy_tilemap(hex);
        sl::destroy_bitmap(hex_image);

        // Flat-top, even columns shifted.
        sl::TileMap *flat = sl::create_hex_tilemap(4, 4, 32, 32, 16, false, false);
        assert(flat);
        sl::tilemap_tile_to_world(flat, 1, 0, hx, hy);
        assert(hx == 24.0f && hy == 0.0f);
        sl::tilemap_tile_to_world(flat, 0, 0, hx, hy);
        assert(hx == 0.0f && hy == 16.0f);
        sl::destroy_tilemap(flat);

        std::ofstream(directory / "hex.tmj") << R"({"type":"map","orientation":"hexagonal","width":2,"height":2,
            "tilewidth":32,"tileheight":32,"hexsidelength":16,"staggeraxis":"x","staggerindex":"even",
            "layers":[{"type":"tilelayer","name":"a","width":2,"height":2,"data":[1,2,3,4]}]})";
        sl::TileMap *loaded_hex = sl::load_tiled_map((directory / "hex.tmj").string());
        assert(loaded_hex && loaded_hex->orientation == sl::TileMapOrientation::hexagonal);
        assert(!loaded_hex->hex_stagger_y && !loaded_hex->hex_stagger_odd && loaded_hex->hex_side_length == 16);
        sl::destroy_tilemap(loaded_hex);
    }

    // Compressed layer data, .tsx tilesets and image-collection tilesets.
    {
        const auto layer_map = [&](const std::string &layer_json) {
            std::ofstream(directory / "enc.tmj") << R"({"orientation":"orthogonal","width":2,"height":2,"tilewidth":32,"tileheight":32,
                "tilesets":[{"firstgid":1,"image":")" << image_path << R"(","tilewidth":32,"tileheight":32}],
                "layers":[)" << layer_json << "]}";
            return sl::load_tiled_map((directory / "enc.tmj").string());
        };
        const char *encodings[] = {
            R"({"type":"tilelayer","name":"a","width":2,"height":2,"encoding":"base64","data":"AQAAAAIAAAADAAAABAAAgA=="})",
            R"({"type":"tilelayer","name":"a","width":2,"height":2,"encoding":"base64","compression":"zlib","data":"eJxjZGBgYAJiZiBmYWBoAAAA4ACL"})",
            R"({"type":"tilelayer","name":"a","width":2,"height":2,"encoding":"base64","compression":"gzip","data":"H4sIACSH2WoC/2NkYGBgAmJmIGZhYGgAAM9XvUIQAAAA"})"};
        for (const char *layer_json : encodings)
        {
            sl::TileMap *encoded = layer_map(layer_json);
            assert(encoded);
            assert(sl::tilemap_get(encoded, 0, 0, 0) == 1 && sl::tilemap_get(encoded, 0, 1, 0) == 2);
            assert(sl::tilemap_get(encoded, 0, 0, 1) == 3 && (sl::tilemap_get(encoded, 0, 1, 1) & sl::tile_flip_horizontal) != 0);
            sl::destroy_tilemap(encoded);
        }
        // zstd, wrong cell counts and corrupt data are reported, not accepted.
        assert(!layer_map(R"({"type":"tilelayer","name":"a","width":2,"height":2,"encoding":"base64","compression":"zstd","data":"AAAA"})"));
        assert(!layer_map(R"({"type":"tilelayer","name":"a","width":2,"height":2,"encoding":"base64","data":"AQAAAA=="})"));
        assert(!layer_map(R"({"type":"tilelayer","name":"a","width":2,"height":2,"encoding":"base64","compression":"zlib","data":"AQAAAAIAAAADAAAABAAAgA=="})"));
        assert(!layer_map(R"({"type":"tilelayer","name":"a","width":2,"height":2,"encoding":"base64","data":"@@@@"})"));

        // Infinite chunks with compressed data.
        std::ofstream(directory / "chunky.tmj") << R"({"orientation":"orthogonal","infinite":true,"width":0,"height":0,"tilewidth":16,"tileheight":16,
            "layers":[{"type":"tilelayer","name":"a","encoding":"base64","compression":"zlib","chunks":[
              {"x":-16,"y":0,"width":2,"height":2,"data":"eJxjZkAAFiAGAABQAAg="}]}]})";
        sl::TileMap *chunky = sl::load_tiled_map((directory / "chunky.tmj").string());
        assert(chunky && sl::tilemap_get(chunky, 0, -16, 0) == 3 && sl::tilemap_get(chunky, 0, -15, 1) == 4);
        sl::destroy_tilemap(chunky);

        // External .tsx tileset with properties and animation (XML escapes included).
        std::ofstream(directory / "ground.tsx") << R"(<?xml version="1.0" encoding="UTF-8"?>
<!-- a comment -->
<tileset version="1.10" name="ground" tilewidth="32" tileheight="32" tilecount="4" columns="2">
 <image source=")" << image_path << R"(" width="64" height="64"/>
 <tile id="1">
  <properties><property name="solid" type="bool" value="true"/><property name="hp" type="int" value="5"/>
   <property name="label">a &amp; b &lt;c&gt;</property></properties>
  <animation><frame tileid="1" duration="200"/><frame tileid="2" duration="300"/></animation>
 </tile>
</tileset>)";
        std::ofstream(directory / "tsx.tmj") << R"({"orientation":"orthogonal","width":1,"height":1,"tilewidth":32,"tileheight":32,
            "tilesets":[{"firstgid":1,"source":"ground.tsx"}],
            "layers":[{"type":"tilelayer","name":"a","width":1,"height":1,"data":[2]}]})";
        sl::TileMap *with_tsx = sl::load_tiled_map((directory / "tsx.tmj").string());
        assert(with_tsx && with_tsx->tilesets.size() == 1 && with_tsx->tilesets[0].atlas.tile_count >= 3);
        assert(sl::tilemap_is_solid(with_tsx, 1, 0));
        assert(sl::tilemap_tile_property(with_tsx, 2, "hp") == "5");
        assert(sl::tilemap_tile_property(with_tsx, 2, "label") == "a & b <c>");
        assert(with_tsx->tilesets[0].tiles.at(1).animation.size() == 2 &&
               with_tsx->tilesets[0].tiles.at(1).animation[1].duration_ms == 300);
        sl::destroy_tilemap(with_tsx);
        std::ofstream(directory / "bad.tsx") << "<tileset tilewidth=\"32\"><image source=\"x.png\"";
        std::ofstream(directory / "badtsx.tmj") << R"({"orientation":"orthogonal","width":1,"height":1,"tilewidth":32,"tileheight":32,
            "tilesets":[{"firstgid":1,"source":"bad.tsx"}],"layers":[]})";
        assert(!sl::load_tiled_map((directory / "badtsx.tmj").string()));

        // Image-collection tileset, both inline (JSON) and as .tsx.
        std::ofstream(directory / "bag.tsx") << R"(<tileset name="bag" tilewidth="32" tileheight="32" tilecount="2">
 <tile id="0"><image source=")" << image_path << R"(" width="64" height="64"/></tile>
 <tile id="5"><image source=")" << image_path << R"(" width="64" height="64"/></tile>
</tileset>)";
        std::ofstream(directory / "collection.tmj") << R"({"orientation":"orthogonal","width":1,"height":1,"tilewidth":32,"tileheight":32,
            "tilesets":[{"firstgid":1,"source":"bag.tsx"},
              {"firstgid":10,"tilewidth":32,"tileheight":32,"tiles":[{"id":3,"image":")" << image_path << R"("}]}],
            "layers":[{"type":"tilelayer","name":"a","width":1,"height":1,"data":[7]}]})";
        sl::TileMap *collection = sl::load_tiled_map((directory / "collection.tmj").string());
        assert(collection && collection->tilesets.size() == 2);
        assert(collection->tilesets[0].images.size() == 2 && collection->tilesets[0].atlas.tile_count == 6);
        assert(collection->tilesets[1].images.size() == 1 && collection->tilesets[1].images.count(3) == 1);
        assert(collection->tilesets[0].atlas.tile_width > 0);
        sl::destroy_tilemap(collection);
    }

    // Layer opacity/tint, group inheritance, image layers, tile objects (JSON and TMX).
    {
        std::ofstream(directory / "layered.tmj") << R"({"orientation":"orthogonal","width":1,"height":1,"tilewidth":32,"tileheight":32,
            "tilesets":[{"firstgid":1,"image":")" << image_path << R"(","tilewidth":32,"tileheight":32}],
            "layers":[
              {"type":"imagelayer","name":"sky","image":")" << image_path << R"(","repeatx":true,"opacity":0.5},
              {"type":"group","name":"g","visible":true,"opacity":0.5,"offsetx":10,"offsety":4,"tintcolor":"#80ff0000","layers":[
                {"type":"tilelayer","name":"a","width":1,"height":1,"data":[1],"opacity":0.5,"offsetx":5},
                {"type":"objectgroup","name":"o","objects":[{"name":"t","gid":2,"x":8,"y":40,"width":32,"height":32,"rotation":90}]}]},
              {"type":"group","name":"hidden","visible":false,"layers":[
                {"type":"tilelayer","name":"b","width":1,"height":1,"data":[1]}]}]})";
        sl::TileMap *layered = sl::load_tiled_map((directory / "layered.tmj").string());
        assert(layered && layered->layers.size() == 2);
        assert(layered->layers[0].opacity == 0.25f && layered->layers[0].offset_x == 15.0f && layered->layers[0].offset_y == 4.0f);
        assert(layered->layers[0].tint.red == 255 && layered->layers[0].tint.green == 0 && layered->layers[0].tint.alpha == 128);
        assert(layered->layers[0].visible && !layered->layers[1].visible);
        assert(layered->image_layers.size() == 1 && layered->image_layers[0].bitmap && layered->image_layers[0].repeat_x);
        assert(layered->image_layers[0].after_layer == -1 && layered->image_layers[0].opacity == 0.5f);
        assert(layered->object_layers.size() == 1 && layered->object_layers[0].after_layer == 0);
        const sl::TileObject &tile_object = layered->object_layers[0].objects[0];
        assert(tile_object.gid == 2 && tile_object.rotation == 90.0f && tile_object.visible);
        sl::destroy_tilemap(layered);

        std::ofstream(directory / "layered.tmx") << R"(<map orientation="orthogonal" width="1" height="1" tilewidth="32" tileheight="32">
 <tileset firstgid="1" name="t" tilewidth="32" tileheight="32"><image source=")" << image_path << R"(" width="512" height="512"/></tileset>
 <group name="g" opacity="0.5" offsetx="3"><layer name="a" width="1" height="1" opacity="0.5" tintcolor="#00ff00"><data encoding="csv">1</data></layer></group>
 <imagelayer name="sky"><image source=")" << image_path << R"("/></imagelayer>
 <objectgroup name="o"><object id="1" name="t" gid="2" x="0" y="32" width="32" height="32" rotation="45"/></objectgroup>
</map>)";
        sl::TileMap *tmx = sl::load_tiled_map((directory / "layered.tmx").string());
        assert(tmx && tmx->layers.size() == 1 && tmx->layers[0].opacity == 0.25f && tmx->layers[0].offset_x == 3.0f);
        assert(tmx->layers[0].tint.green == 255 && tmx->layers[0].tint.red == 0);
        assert(tmx->image_layers.size() == 1 && tmx->image_layers[0].after_layer == 0 && tmx->image_layers[0].bitmap);
        assert(tmx->object_layers[0].objects[0].gid == 2 && tmx->object_layers[0].objects[0].rotation == 45.0f);
        sl::destroy_tilemap(tmx);
    }

    // Runtime decals.
    {
        sl::TileMap *decal_map = sl::create_tilemap(2, 2, 32, 32);
        assert(decal_map);
        sl::tilemap_add_layer(decal_map, "floor");
        sl::tilemap_add_layer(decal_map, "walls");
        assert(!sl::tilemap_add_decal(decal_map, 0, 0, 0.0f, 0.0f));
        assert(sl::tilemap_add_decal(decal_map, 99, 3, 10.0f, 12.0f, 45.0f, 2.0f, sl::Red, 1.0f, 0.5f));
        assert(decal_map->decals.size() == 1 && decal_map->decals[0].after_layer == 1);
        decal_map->max_decals = 2;
        sl::tilemap_add_decal(decal_map, 0, 4, 0.0f, 0.0f);
        sl::tilemap_add_decal(decal_map, 0, 5, 0.0f, 0.0f);
        assert(decal_map->decals.size() == 2 && decal_map->decals[0].gid == 4);
        sl::tilemap_add_decal(decal_map, 0, 6, 0.0f, 0.0f, 0.0f, 1.0f, sl::White, 0.5f);
        assert(decal_map->decals.size() == 2 && decal_map->decals[1].gid == 6);
        sl::update_tilemap(decal_map, 0.4f);
        assert(decal_map->decals.size() == 2);
        sl::update_tilemap(decal_map, 0.2f);
        assert(decal_map->decals.size() == 1 && decal_map->decals[0].gid == 5);
        sl::tilemap_clear_decals(decal_map);
        assert(decal_map->decals.empty());
        sl::destroy_tilemap(decal_map);
    }

    assert(!sl::load_tiled_map((directory / "missing.tmj").string()));
    std::filesystem::remove_all(directory);
    return 0;
}

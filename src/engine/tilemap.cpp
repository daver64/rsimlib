/** @file
 * @brief Implements tile maps: layers, tile metadata, animation, batched drawing and the Tiled JSON loader.
 */
#include "tilemap.h"

#include "display.h"
#include "draw.h"
#include "error.h"
#include "isometric.h"
#include "json.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace sl
{
    namespace
    {
        constexpr std::uint32_t flip_mask = tile_flip_horizontal | tile_flip_vertical | tile_flip_diagonal;

        /** Index of the tileset owning a GID, or -1. Tilesets are kept sorted by first_gid. */
        int find_tileset(const TileMap *map, std::uint32_t gid)
        {
            for (int index = static_cast<int>(map->tilesets.size()) - 1; index >= 0; --index)
            {
                if (static_cast<std::uint32_t>(map->tilesets[index].first_gid) <= gid)
                    return index;
            }
            return -1;
        }

        /** Resolve a tileset-local tile through its animation at the map's current clock. */
        int animated_tile(const TileSet &set, int local, double clock_ms)
        {
            const auto info = set.tiles.find(local);
            if (info == set.tiles.end() || info->second.animation.empty())
                return local;
            int total = 0;
            for (const TileAnimationFrame &frame : info->second.animation)
                total += std::max(frame.duration_ms, 1);
            int time = static_cast<int>(std::fmod(clock_ms, static_cast<double>(total)));
            for (const TileAnimationFrame &frame : info->second.animation)
            {
                time -= std::max(frame.duration_ms, 1);
                if (time < 0)
                    return frame.tile;
            }
            return local;
        }

        void draw_cell(const TileMap *map, std::uint32_t cell, float x, float bottom_y)
        {
            const std::uint32_t gid = cell & tile_id_mask;
            if (gid == 0)
                return;
            const int set_index = find_tileset(map, gid);
            if (set_index < 0)
                return;
            const TileSet &set = map->tilesets[set_index];
            const int local = animated_tile(set, static_cast<int>(gid) - set.first_gid, map->animation_time_ms);
            int source_x = 0, source_y = 0, width = 0, height = 0;
            if (!atlas_tile_rect(set.atlas, local, source_x, source_y, width, height))
                return;
            draw_sprite_region_flipped(set.atlas.bitmap, source_x, source_y, width, height,
                                       x, bottom_y - static_cast<float>(height),
                                       static_cast<float>(width), static_cast<float>(height),
                                       (cell & tile_flip_horizontal) != 0, (cell & tile_flip_vertical) != 0,
                                       (cell & tile_flip_diagonal) != 0);
        }

        int max_tile_height(const TileMap *map)
        {
            int result = map->tile_height;
            for (const TileSet &set : map->tilesets)
                result = std::max(result, set.atlas.tile_height);
            return result;
        }

        int max_tile_width(const TileMap *map)
        {
            int result = map->tile_width;
            for (const TileSet &set : map->tilesets)
                result = std::max(result, set.atlas.tile_width);
            return result;
        }

        IsometricTransform make_iso(const TileMap *map, float origin_x, float origin_y)
        {
            IsometricTransform iso;
            iso.config.tile_width = static_cast<float>(map->tile_width);
            iso.config.tile_height = static_cast<float>(map->tile_height);
            iso.origin_x = origin_x;
            iso.origin_y = origin_y;
            return iso;
        }
    } // namespace

    TileMap *create_tilemap(int width, int height, int tile_width, int tile_height, TileMapOrientation orientation)
    {
        if (width <= 0 || height <= 0 || tile_width <= 0 || tile_height <= 0)
        {
            detail::set_error("Invalid tile map dimensions");
            return nullptr;
        }
        TileMap *map = new TileMap;
        map->width = width;
        map->height = height;
        map->tile_width = tile_width;
        map->tile_height = tile_height;
        map->orientation = orientation;
        return map;
    }

    void destroy_tilemap(TileMap *map)
    {
        if (!map)
            return;
        for (Bitmap *bitmap : map->owned_bitmaps)
            destroy_bitmap(bitmap);
        delete map;
    }

    int tilemap_add_tileset(TileMap *map, const Atlas &atlas, int first_gid)
    {
        if (!map || !atlas.bitmap)
            return -1;
        if (first_gid <= 0)
        {
            first_gid = 1;
            if (!map->tilesets.empty())
                first_gid = map->tilesets.back().first_gid + map->tilesets.back().atlas.tile_count;
        }
        TileSet set;
        set.atlas = atlas;
        set.first_gid = first_gid;
        map->tilesets.push_back(std::move(set));
        std::stable_sort(map->tilesets.begin(), map->tilesets.end(),
                         [](const TileSet &a, const TileSet &b) { return a.first_gid < b.first_gid; });
        for (std::size_t index = 0; index < map->tilesets.size(); ++index)
        {
            if (map->tilesets[index].first_gid == first_gid)
                return static_cast<int>(index);
        }
        return -1;
    }

    int tilemap_add_layer(TileMap *map, const std::string &name)
    {
        if (!map)
            return -1;
        TileLayer layer;
        layer.name = name;
        layer.width = map->width;
        layer.height = map->height;
        layer.cells.assign(static_cast<std::size_t>(map->width) * map->height, 0);
        map->layers.push_back(std::move(layer));
        return static_cast<int>(map->layers.size()) - 1;
    }

    TileLayer *tilemap_layer(TileMap *map, int layer)
    {
        if (!map || layer < 0 || layer >= static_cast<int>(map->layers.size()))
            return nullptr;
        return &map->layers[layer];
    }

    TileLayer *tilemap_layer(TileMap *map, const std::string &name)
    {
        if (!map)
            return nullptr;
        for (TileLayer &layer : map->layers)
        {
            if (layer.name == name)
                return &layer;
        }
        return nullptr;
    }

    std::uint32_t tilemap_get(const TileMap *map, int layer, int column, int row)
    {
        if (!map || layer < 0 || layer >= static_cast<int>(map->layers.size()))
            return 0;
        const TileLayer &data = map->layers[layer];
        if (column < 0 || row < 0 || column >= data.width || row >= data.height)
            return 0;
        return data.cells[static_cast<std::size_t>(row) * data.width + column];
    }

    bool tilemap_set(TileMap *map, int layer, int column, int row, std::uint32_t cell)
    {
        if (!map || layer < 0 || layer >= static_cast<int>(map->layers.size()))
            return false;
        TileLayer &data = map->layers[layer];
        if (column < 0 || row < 0 || column >= data.width || row >= data.height)
            return false;
        data.cells[static_cast<std::size_t>(row) * data.width + column] = cell;
        return true;
    }

    TileInfo *tilemap_tile_info(TileMap *map, std::uint32_t gid)
    {
        gid &= tile_id_mask;
        if (!map || gid == 0)
            return nullptr;
        const int set_index = find_tileset(map, gid);
        if (set_index < 0)
            return nullptr;
        TileSet &set = map->tilesets[set_index];
        const auto found = set.tiles.find(static_cast<int>(gid) - set.first_gid);
        return found == set.tiles.end() ? nullptr : &found->second;
    }

    void tilemap_set_solid(TileMap *map, std::uint32_t gid, bool solid)
    {
        gid &= tile_id_mask;
        if (!map || gid == 0)
            return;
        const int set_index = find_tileset(map, gid);
        if (set_index < 0)
            return;
        TileSet &set = map->tilesets[set_index];
        set.tiles[static_cast<int>(gid) - set.first_gid].solid = solid;
    }

    std::string tilemap_tile_property(const TileMap *map, std::uint32_t gid, const std::string &key)
    {
        const TileInfo *info = tilemap_tile_info(const_cast<TileMap *>(map), gid);
        if (!info)
            return {};
        const auto found = info->properties.find(key);
        return found == info->properties.end() ? std::string{} : found->second;
    }

    bool tilemap_is_solid(const TileMap *map, int column, int row, int layer)
    {
        if (!map || column < 0 || row < 0 || column >= map->width || row >= map->height)
            return true;
        const int first = layer >= 0 ? layer : 0;
        const int last = layer >= 0 ? layer : static_cast<int>(map->layers.size()) - 1;
        for (int index = first; index <= last; ++index)
        {
            const std::uint32_t cell = tilemap_get(map, index, column, row);
            const TileInfo *info = tilemap_tile_info(const_cast<TileMap *>(map), cell);
            if (info && info->solid)
                return true;
        }
        return false;
    }

    bool tilemap_rect_blocked(const TileMap *map, float x, float y, float width, float height, int layer)
    {
        if (!map || map->tile_width <= 0 || map->tile_height <= 0)
            return false;
        const int first_column = static_cast<int>(std::floor(x / map->tile_width));
        const int last_column = static_cast<int>(std::floor((x + width - 0.001f) / map->tile_width));
        const int first_row = static_cast<int>(std::floor(y / map->tile_height));
        const int last_row = static_cast<int>(std::floor((y + height - 0.001f) / map->tile_height));
        for (int row = first_row; row <= last_row; ++row)
        {
            for (int column = first_column; column <= last_column; ++column)
            {
                if (tilemap_is_solid(map, column, row, layer))
                    return true;
            }
        }
        return false;
    }

    void tilemap_world_to_tile(const TileMap *map, float x, float y, int &column, int &row)
    {
        column = row = 0;
        if (!map || map->tile_width <= 0 || map->tile_height <= 0)
            return;
        if (map->orientation == TileMapOrientation::isometric)
        {
            const IsoGridPoint cell = make_iso(map, 0.0f, 0.0f).screen_to_grid(x, y);
            column = cell.x;
            row = cell.y;
            return;
        }
        column = static_cast<int>(std::floor(x / map->tile_width));
        row = static_cast<int>(std::floor(y / map->tile_height));
    }

    void tilemap_tile_to_world(const TileMap *map, int column, int row, float &x, float &y)
    {
        x = y = 0.0f;
        if (!map)
            return;
        if (map->orientation == TileMapOrientation::isometric)
        {
            make_iso(map, 0.0f, 0.0f).world_to_screen(static_cast<float>(column) + 0.5f,
                                                      static_cast<float>(row) + 0.5f, 0.0f, x, y);
            return;
        }
        x = static_cast<float>(column * map->tile_width);
        y = static_cast<float>(row * map->tile_height);
    }

    void update_tilemap(TileMap *map, float dt_seconds)
    {
        if (map && dt_seconds > 0.0f)
            map->animation_time_ms += static_cast<double>(dt_seconds) * 1000.0;
    }

    void draw_tilemap(const TileMap *map, float camera_x, float camera_y, int view_width, int view_height,
                      int first_layer, int last_layer)
    {
        if (!map || map->layers.empty() || map->tile_width <= 0 || map->tile_height <= 0)
            return;
        if (view_width <= 0)
            view_width = screen_width();
        if (view_height <= 0)
            view_height = screen_height();
        if (view_width <= 0 || view_height <= 0)
            return;

        first_layer = std::max(first_layer, 0);
        last_layer = last_layer < 0 ? static_cast<int>(map->layers.size()) - 1
                                    : std::min(last_layer, static_cast<int>(map->layers.size()) - 1);
        const int overhang_up = std::max(max_tile_height(map) - map->tile_height, 0);
        const int overhang_side = std::max(max_tile_width(map) - map->tile_width, 0);

        for (int index = first_layer; index <= last_layer; ++index)
        {
            const TileLayer &layer = map->layers[index];
            if (!layer.visible || layer.width <= 0 || layer.height <= 0)
                continue;
            // Snap to whole pixels so neighbouring tiles never show seams.
            const float scroll_x = std::floor(camera_x * layer.parallax_x - layer.offset_x);
            const float scroll_y = std::floor(camera_y * layer.parallax_y - layer.offset_y);

            if (map->orientation == TileMapOrientation::isometric)
            {
                const IsometricTransform iso = make_iso(map, -scroll_x, -scroll_y);
                const IsoBounds bounds = iso.visible_bounds(
                    -static_cast<float>(overhang_side), 0.0f,
                    static_cast<float>(view_width + overhang_side), static_cast<float>(view_height + overhang_up));
                const int min_col = std::max(bounds.min_x, 0), max_col = std::min(bounds.max_x, layer.width - 1);
                const int min_row = std::max(bounds.min_y, 0), max_row = std::min(bounds.max_y, layer.height - 1);
                // Painter's order: ascending (column + row) so nearer tiles overdraw farther ones.
                for (int diagonal = min_col + min_row; diagonal <= max_col + max_row; ++diagonal)
                {
                    const int col_start = std::max(min_col, diagonal - max_row);
                    const int col_end = std::min(max_col, diagonal - min_row);
                    for (int column = col_start; column <= col_end; ++column)
                    {
                        const int row = diagonal - column;
                        const std::uint32_t cell = layer.cells[static_cast<std::size_t>(row) * layer.width + column];
                        if ((cell & tile_id_mask) == 0)
                            continue;
                        const IsoPoint top = iso.world_to_screen(static_cast<float>(column), static_cast<float>(row));
                        const int set_index = find_tileset(map, cell & tile_id_mask);
                        const int image_width = set_index >= 0 ? map->tilesets[set_index].atlas.tile_width : map->tile_width;
                        draw_cell(map, cell, std::floor(top.x - image_width * 0.5f),
                                  std::floor(top.y) + static_cast<float>(map->tile_height));
                    }
                }
                continue;
            }

            const int min_col = std::max(static_cast<int>(std::floor(scroll_x / map->tile_width)) - 1, 0);
            const int max_col = std::min(static_cast<int>(std::floor((scroll_x + view_width + overhang_side) / map->tile_width)),
                                         layer.width - 1);
            const int min_row = std::max(static_cast<int>(std::floor(scroll_y / map->tile_height)), 0);
            const int max_row = std::min(static_cast<int>(std::floor((scroll_y + view_height + overhang_up) / map->tile_height)),
                                         layer.height - 1);
            for (int row = min_row; row <= max_row; ++row)
            {
                for (int column = min_col; column <= max_col; ++column)
                {
                    const std::uint32_t cell = layer.cells[static_cast<std::size_t>(row) * layer.width + column];
                    if ((cell & tile_id_mask) == 0)
                        continue;
                    draw_cell(map, cell, static_cast<float>(column * map->tile_width) - scroll_x,
                              static_cast<float>((row + 1) * map->tile_height) - scroll_y);
                }
            }
        }
    }

    const TileObject *tilemap_find_object(const TileMap *map, const std::string &name)
    {
        if (!map)
            return nullptr;
        for (const ObjectLayer &layer : map->object_layers)
        {
            for (const TileObject &object : layer.objects)
            {
                if (object.name == name)
                    return &object;
            }
        }
        return nullptr;
    }

    namespace
    {
        using Json = nlohmann::json;

        bool read_json_file(const std::filesystem::path &path, Json &out)
        {
            std::ifstream file(path);
            if (!file)
            {
                detail::set_error("Unable to open Tiled file: " + path.string());
                return false;
            }
            try
            {
                file >> out;
            }
            catch (const Json::exception &exception)
            {
                detail::set_error("Invalid Tiled JSON in " + path.string() + ": " + exception.what());
                return false;
            }
            return true;
        }

        std::string value_text(const Json &value)
        {
            return value.is_string() ? value.get<std::string>() : value.dump();
        }

        std::map<std::string, std::string> read_properties(const Json &node)
        {
            std::map<std::string, std::string> result;
            const auto found = node.find("properties");
            if (found == node.end() || !found->is_array())
                return result;
            for (const Json &property : *found)
            {
                if (property.contains("name") && property.contains("value"))
                    result[property["name"].get<std::string>()] = value_text(property["value"]);
            }
            return result;
        }

        std::string class_of(const Json &node)
        {
            if (node.contains("class") && node["class"].is_string())
                return node["class"].get<std::string>();
            return node.value("type", std::string{});
        }

        bool load_tileset(TileMap *map, const Json &node, const std::filesystem::path &base_dir, int first_gid,
                          std::string &error)
        {
            Json external;
            const Json *set = &node;
            std::filesystem::path set_dir = base_dir;
            if (node.contains("source"))
            {
                const std::filesystem::path source = base_dir / node["source"].get<std::string>();
                if (source.extension() == ".tsx")
                {
                    error = "XML tilesets (.tsx) are not supported; export the tileset as JSON: " + source.string();
                    return false;
                }
                if (!read_json_file(source, external))
                {
                    error = "Unable to read tileset " + source.string();
                    return false;
                }
                set = &external;
                set_dir = source.parent_path();
            }
            if (!set->contains("image"))
            {
                error = "Image-collection tilesets are not supported";
                return false;
            }
            const std::filesystem::path image_path = set_dir / (*set)["image"].get<std::string>();
            Bitmap *bitmap = load_bitmap(image_path.string());
            if (!bitmap)
            {
                error = "Unable to load tileset image " + image_path.string();
                return false;
            }
            map->owned_bitmaps.push_back(bitmap);
            const Atlas atlas = create_atlas(bitmap, set->value("tilewidth", 0), set->value("tileheight", 0),
                                             set->value("spacing", 0), set->value("margin", 0));
            if (atlas.tile_count <= 0)
            {
                error = "Tileset image does not contain any complete tiles: " + image_path.string();
                return false;
            }
            const int index = tilemap_add_tileset(map, atlas, first_gid);
            TileSet &target = map->tilesets[index];
            if (set->contains("tiles") && (*set)["tiles"].is_array())
            {
                for (const Json &tile : (*set)["tiles"])
                {
                    TileInfo &info = target.tiles[tile.value("id", 0)];
                    info.properties = read_properties(tile);
                    const auto solid = info.properties.find("solid");
                    info.solid = solid != info.properties.end() && solid->second == "true";
                    if (tile.contains("animation") && tile["animation"].is_array())
                    {
                        for (const Json &frame : tile["animation"])
                            info.animation.push_back({frame.value("tileid", 0), frame.value("duration", 100)});
                    }
                }
            }
            return true;
        }

        bool load_layers(TileMap *map, const Json &layers, std::string &error)
        {
            for (const Json &node : layers)
            {
                const std::string kind = node.value("type", std::string{});
                if (kind == "group")
                {
                    if (node.contains("layers") && !load_layers(map, node["layers"], error))
                        return false;
                }
                else if (kind == "tilelayer")
                {
                    if (node.contains("chunks"))
                    {
                        error = "Infinite Tiled maps are not supported";
                        return false;
                    }
                    if (!node.contains("data") || !node["data"].is_array())
                    {
                        error = "Tile layer data must use CSV encoding (uncompressed array)";
                        return false;
                    }
                    TileLayer layer;
                    layer.name = node.value("name", std::string{});
                    layer.width = node.value("width", map->width);
                    layer.height = node.value("height", map->height);
                    layer.visible = node.value("visible", true);
                    layer.offset_x = node.value("offsetx", 0.0f);
                    layer.offset_y = node.value("offsety", 0.0f);
                    layer.parallax_x = node.value("parallaxx", 1.0f);
                    layer.parallax_y = node.value("parallaxy", 1.0f);
                    if (node["data"].size() != static_cast<std::size_t>(layer.width) * layer.height)
                    {
                        error = "Tile layer '" + layer.name + "' has the wrong number of cells";
                        return false;
                    }
                    for (const Json &cell : node["data"])
                        layer.cells.push_back(cell.get<std::uint32_t>());
                    map->layers.push_back(std::move(layer));
                }
                else if (kind == "objectgroup")
                {
                    ObjectLayer layer;
                    layer.name = node.value("name", std::string{});
                    if (node.contains("objects"))
                    {
                        for (const Json &object : node["objects"])
                        {
                            TileObject entry;
                            entry.name = object.value("name", std::string{});
                            entry.type = class_of(object);
                            entry.x = object.value("x", 0.0f);
                            entry.y = object.value("y", 0.0f);
                            entry.width = object.value("width", 0.0f);
                            entry.height = object.value("height", 0.0f);
                            entry.properties = read_properties(object);
                            layer.objects.push_back(std::move(entry));
                        }
                    }
                    map->object_layers.push_back(std::move(layer));
                }
            }
            return true;
        }
    } // namespace

    TileMap *load_tiled_map(const std::string &path)
    {
        Json root;
        const std::filesystem::path map_path(path);
        if (!read_json_file(map_path, root))
            return nullptr;

        TileMap *map = nullptr;
        std::string error;
        try
        {
            const std::string orientation = root.value("orientation", std::string("orthogonal"));
            if (orientation != "orthogonal" && orientation != "isometric")
            {
                detail::set_error("Unsupported Tiled orientation: " + orientation);
                return nullptr;
            }
            map = create_tilemap(root.value("width", 0), root.value("height", 0), root.value("tilewidth", 0),
                                 root.value("tileheight", 0),
                                 orientation == "isometric" ? TileMapOrientation::isometric
                                                            : TileMapOrientation::orthogonal);
            if (!map)
                return nullptr;
            const std::filesystem::path base_dir = map_path.parent_path();
            if (root.contains("tilesets"))
            {
                for (const Json &set : root["tilesets"])
                {
                    if (!load_tileset(map, set, base_dir, set.value("firstgid", 1), error))
                    {
                        detail::set_error(error);
                        destroy_tilemap(map);
                        return nullptr;
                    }
                }
            }
            if (root.contains("layers") && !load_layers(map, root["layers"], error))
            {
                detail::set_error(error);
                destroy_tilemap(map);
                return nullptr;
            }
        }
        catch (const Json::exception &exception)
        {
            detail::set_error(std::string("Invalid Tiled map: ") + exception.what());
            destroy_tilemap(map);
            return nullptr;
        }
        return map;
    }
} // namespace sl

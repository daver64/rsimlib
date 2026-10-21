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
#include <limits>

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
            double total = 0.0;
            for (const TileAnimationFrame &frame : info->second.animation)
                total += std::max(frame.duration_ms, 1);
            double time = std::fmod(clock_ms, total);
            for (const TileAnimationFrame &frame : info->second.animation)
            {
                time -= std::max(frame.duration_ms, 1);
                if (time < 0.0)
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

        int pixel_to_tile(float coordinate, int tile_size)
        {
            if (std::isnan(coordinate))
                return 0;
            const double cell = std::floor(static_cast<double>(coordinate) / tile_size);
            if (cell <= std::numeric_limits<int>::min())
                return std::numeric_limits<int>::min();
            if (cell >= std::numeric_limits<int>::max())
                return std::numeric_limits<int>::max();
            return static_cast<int>(cell);
        }

        std::uint64_t chunk_key(int chunk_x, int chunk_y)
        {
            return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(chunk_x)) << 32) |
                   static_cast<std::uint32_t>(chunk_y);
        }

        int floor_div(int value, int divisor)
        {
            int quotient = value / divisor;
            if ((value % divisor != 0) && ((value < 0) != (divisor < 0)))
                --quotient;
            return quotient;
        }

        std::uint32_t layer_cell(const TileLayer &layer, int column, int row)
        {
            if (layer.infinite)
            {
                const int chunk_x = floor_div(column, tile_chunk_size);
                const int chunk_y = floor_div(row, tile_chunk_size);
                const auto found = layer.chunks.find(chunk_key(chunk_x, chunk_y));
                if (found == layer.chunks.end())
                    return 0;
                return found->second[static_cast<std::size_t>(row - chunk_y * tile_chunk_size) * tile_chunk_size +
                                     (column - chunk_x * tile_chunk_size)];
            }
            if (column < 0 || row < 0 || column >= layer.width || row >= layer.height)
                return 0;
            return layer.cells[static_cast<std::size_t>(row) * layer.width + column];
        }

        bool layer_set_cell(TileLayer &layer, int column, int row, std::uint32_t cell)
        {
            if (layer.infinite)
            {
                const int chunk_x = floor_div(column, tile_chunk_size);
                const int chunk_y = floor_div(row, tile_chunk_size);
                auto found = layer.chunks.find(chunk_key(chunk_x, chunk_y));
                if (found == layer.chunks.end())
                {
                    if (cell == 0)
                        return true;
                    found = layer.chunks.emplace(chunk_key(chunk_x, chunk_y),
                                                 std::vector<std::uint32_t>(static_cast<std::size_t>(tile_chunk_size) * tile_chunk_size, 0)).first;
                }
                found->second[static_cast<std::size_t>(row - chunk_y * tile_chunk_size) * tile_chunk_size +
                              (column - chunk_x * tile_chunk_size)] = cell;
                return true;
            }
            if (column < 0 || row < 0 || column >= layer.width || row >= layer.height)
                return false;
            layer.cells[static_cast<std::size_t>(row) * layer.width + column] = cell;
            return true;
        }

        float hex_column_width(const TileMap *map)
        {
            return map->hex_stagger_y ? static_cast<float>(map->tile_width)
                                      : (static_cast<float>(map->tile_width) + map->hex_side_length) * 0.5f;
        }

        float hex_row_height(const TileMap *map)
        {
            return map->hex_stagger_y ? (static_cast<float>(map->tile_height) + map->hex_side_length) * 0.5f
                                      : static_cast<float>(map->tile_height);
        }

        // Top-left pixel of a hexagon's bounding box.
        void hex_origin(const TileMap *map, int column, int row, float &x, float &y)
        {
            const bool shifted = (((map->hex_stagger_y ? row : column) & 1) != 0) == map->hex_stagger_odd;
            if (map->hex_stagger_y)
            {
                x = static_cast<float>(column) * map->tile_width + (shifted ? map->tile_width * 0.5f : 0.0f);
                y = static_cast<float>(row) * hex_row_height(map);
            }
            else
            {
                x = static_cast<float>(column) * hex_column_width(map);
                y = static_cast<float>(row) * map->tile_height + (shifted ? map->tile_height * 0.5f : 0.0f);
            }
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

    TileMap *create_infinite_tilemap(int tile_width, int tile_height, TileMapOrientation orientation)
    {
        if (orientation == TileMapOrientation::isometric)
        {
            detail::set_error("Infinite isometric tile maps are not supported");
            return nullptr;
        }
        TileMap *map = create_tilemap(1, 1, tile_width, tile_height, orientation);
        if (map)
        {
            map->infinite = true;
            map->width = map->height = 0;
            if (orientation == TileMapOrientation::hexagonal)
                map->hex_side_length = tile_height / 2;
        }
        return map;
    }

    TileMap *create_hex_tilemap(int width, int height, int tile_width, int tile_height, int side_length,
                                bool stagger_y, bool stagger_odd)
    {
        if (side_length < 0 || side_length > (stagger_y ? tile_height : tile_width))
        {
            detail::set_error("Invalid hexagon side length");
            return nullptr;
        }
        TileMap *map = create_tilemap(width, height, tile_width, tile_height, TileMapOrientation::hexagonal);
        if (map)
        {
            map->hex_side_length = side_length;
            map->hex_stagger_y = stagger_y;
            map->hex_stagger_odd = stagger_odd;
        }
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
        layer.infinite = map->infinite;
        if (!layer.infinite)
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
        return layer_cell(map->layers[layer], column, row);
    }

    bool tilemap_layer_bounds(const TileMap *map, int layer, int &min_column, int &min_row, int &max_column, int &max_row)
    {
        min_column = min_row = max_column = max_row = 0;
        if (!map || layer < 0 || layer >= static_cast<int>(map->layers.size()))
            return false;
        const TileLayer &data = map->layers[layer];
        if (!data.infinite)
        {
            max_column = data.width - 1;
            max_row = data.height - 1;
            return data.width > 0 && data.height > 0;
        }
        bool found = false;
        for (const auto &[key, cells] : data.chunks)
        {
            const int chunk_x = static_cast<int>(static_cast<std::uint32_t>(key >> 32));
            const int chunk_y = static_cast<int>(static_cast<std::uint32_t>(key & 0xFFFFFFFFu));
            for (int index = 0; index < tile_chunk_size * tile_chunk_size; ++index)
            {
                if ((cells[index] & tile_id_mask) == 0)
                    continue;
                const int column = chunk_x * tile_chunk_size + index % tile_chunk_size;
                const int row = chunk_y * tile_chunk_size + index / tile_chunk_size;
                if (!found)
                {
                    min_column = max_column = column;
                    min_row = max_row = row;
                    found = true;
                }
                min_column = std::min(min_column, column);
                max_column = std::max(max_column, column);
                min_row = std::min(min_row, row);
                max_row = std::max(max_row, row);
            }
        }
        return found;
    }

    bool tilemap_set(TileMap *map, int layer, int column, int row, std::uint32_t cell)
    {
        if (!map || layer < 0 || layer >= static_cast<int>(map->layers.size()))
            return false;
        return layer_set_cell(map->layers[layer], column, row, cell);
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
        if (!map)
            return true;
        if (!map->infinite && (column < 0 || row < 0 || column >= map->width || row >= map->height))
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
        if (!map || map->tile_width <= 0 || map->tile_height <= 0 || width <= 0.0f || height <= 0.0f ||
            !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) || !std::isfinite(height) ||
            !std::isfinite(x + width) || !std::isfinite(y + height))
            return false;
        const float right = std::nextafter(x + width, -std::numeric_limits<float>::infinity());
        const float bottom = std::nextafter(y + height, -std::numeric_limits<float>::infinity());
        if (map->orientation == TileMapOrientation::hexagonal)
        {
            const float step = std::max(1.0f, std::min(hex_column_width(map), hex_row_height(map)) * 0.5f);
            for (float sy = y;; sy += step)
            {
                const float py = std::min(sy, bottom);
                for (float sx = x;; sx += step)
                {
                    int column = 0, row = 0;
                    tilemap_world_to_tile(map, std::min(sx, right), py, column, row);
                    if (tilemap_is_solid(map, column, row, layer))
                        return true;
                    if (sx >= right)
                        break;
                }
                if (sy >= bottom)
                    break;
            }
            return false;
        }
        const int first_column = pixel_to_tile(x, map->tile_width);
        const int last_column = pixel_to_tile(right, map->tile_width);
        const int first_row = pixel_to_tile(y, map->tile_height);
        const int last_row = pixel_to_tile(bottom, map->tile_height);
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
        if (map->orientation == TileMapOrientation::hexagonal)
        {
            const int guess_column = pixel_to_tile(x, std::max(1, static_cast<int>(hex_column_width(map))));
            const int guess_row = pixel_to_tile(y, std::max(1, static_cast<int>(hex_row_height(map))));
            float best = std::numeric_limits<float>::max();
            for (int test_row = guess_row - 1; test_row <= guess_row + 1; ++test_row)
            {
                for (int test_column = guess_column - 1; test_column <= guess_column + 1; ++test_column)
                {
                    float origin_x = 0.0f, origin_y = 0.0f;
                    hex_origin(map, test_column, test_row, origin_x, origin_y);
                    const float dx = x - (origin_x + map->tile_width * 0.5f);
                    const float dy = y - (origin_y + map->tile_height * 0.5f);
                    const float distance = dx * dx + dy * dy;
                    if (distance < best)
                    {
                        best = distance;
                        column = test_column;
                        row = test_row;
                    }
                }
            }
            return;
        }
        column = pixel_to_tile(x, map->tile_width);
        row = pixel_to_tile(y, map->tile_height);
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
        if (map->orientation == TileMapOrientation::hexagonal)
        {
            hex_origin(map, column, row, x, y);
            return;
        }
        x = static_cast<float>(column) * map->tile_width;
        y = static_cast<float>(row) * map->tile_height;
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
            if (!layer.visible || (!layer.infinite && (layer.width <= 0 || layer.height <= 0)))
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

            if (map->orientation == TileMapOrientation::hexagonal)
            {
                const float column_width = hex_column_width(map);
                const float row_height = hex_row_height(map);
                int min_col = static_cast<int>(std::floor(scroll_x / column_width)) - 1;
                int max_col = static_cast<int>(std::floor((scroll_x + view_width + overhang_side + map->tile_width) / column_width)) + 1;
                int min_row = static_cast<int>(std::floor(scroll_y / row_height)) - 1;
                int max_row = static_cast<int>(std::floor((scroll_y + view_height + overhang_up + map->tile_height) / row_height)) + 1;
                if (!layer.infinite)
                {
                    min_col = std::max(min_col, 0);
                    min_row = std::max(min_row, 0);
                    max_col = std::min(max_col, layer.width - 1);
                    max_row = std::min(max_row, layer.height - 1);
                }
                for (int row = min_row; row <= max_row; ++row)
                {
                    for (int column = min_col; column <= max_col; ++column)
                    {
                        const std::uint32_t cell = layer_cell(layer, column, row);
                        if ((cell & tile_id_mask) == 0)
                            continue;
                        float origin_x = 0.0f, origin_y = 0.0f;
                        hex_origin(map, column, row, origin_x, origin_y);
                        draw_cell(map, cell, origin_x - scroll_x, origin_y + static_cast<float>(map->tile_height) - scroll_y);
                    }
                }
                continue;
            }

            int min_col = static_cast<int>(std::floor(scroll_x / map->tile_width)) - 1;
            int max_col = static_cast<int>(std::floor((scroll_x + view_width + overhang_side) / map->tile_width));
            int min_row = static_cast<int>(std::floor(scroll_y / map->tile_height));
            int max_row = static_cast<int>(std::floor((scroll_y + view_height + overhang_up) / map->tile_height));
            if (!layer.infinite)
            {
                min_col = std::max(min_col, 0);
                min_row = std::max(min_row, 0);
                max_col = std::min(max_col, layer.width - 1);
                max_row = std::min(max_row, layer.height - 1);
            }
            for (int row = min_row; row <= max_row; ++row)
            {
                for (int column = min_col; column <= max_col; ++column)
                {
                    const std::uint32_t cell = layer_cell(layer, column, row);
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
                        if (map->orientation == TileMapOrientation::isometric)
                        {
                            error = "Infinite isometric Tiled maps are not supported";
                            return false;
                        }
                        map->infinite = true;
                        TileLayer layer;
                        layer.name = node.value("name", std::string{});
                        layer.infinite = true;
                        layer.visible = node.value("visible", true);
                        layer.offset_x = node.value("offsetx", 0.0f);
                        layer.offset_y = node.value("offsety", 0.0f);
                        layer.parallax_x = node.value("parallaxx", 1.0f);
                        layer.parallax_y = node.value("parallaxy", 1.0f);
                        for (const Json &chunk : node["chunks"])
                        {
                            if (!chunk.contains("data") || !chunk["data"].is_array())
                            {
                                error = "Tile layer data must use CSV encoding (uncompressed array)";
                                return false;
                            }
                            const int start_x = chunk.value("x", 0);
                            const int start_y = chunk.value("y", 0);
                            const int chunk_width = chunk.value("width", 0);
                            if (chunk_width <= 0 || chunk["data"].size() % static_cast<std::size_t>(chunk_width) != 0)
                            {
                                error = "Tile layer '" + layer.name + "' has an invalid chunk";
                                return false;
                            }
                            int index = 0;
                            for (const Json &cell : chunk["data"])
                            {
                                const std::uint32_t value = cell.get<std::uint32_t>();
                                if (value != 0)
                                    layer_set_cell(layer, start_x + index % chunk_width, start_y + index / chunk_width, value);
                                ++index;
                            }
                        }
                        map->layers.push_back(std::move(layer));
                        continue;
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
            if (orientation != "orthogonal" && orientation != "isometric" && orientation != "hexagonal")
            {
                detail::set_error("Unsupported Tiled orientation: " + orientation);
                return nullptr;
            }
            const bool infinite = root.value("infinite", false);
            map = create_tilemap(std::max(root.value("width", 0), infinite ? 1 : 0),
                                 std::max(root.value("height", 0), infinite ? 1 : 0), root.value("tilewidth", 0),
                                 root.value("tileheight", 0),
                                 orientation == "isometric"   ? TileMapOrientation::isometric
                                 : orientation == "hexagonal" ? TileMapOrientation::hexagonal
                                                              : TileMapOrientation::orthogonal);
            if (!map)
                return nullptr;
            map->infinite = infinite;
            if (orientation == "hexagonal")
            {
                map->hex_stagger_y = root.value("staggeraxis", std::string("y")) != "x";
                map->hex_stagger_odd = root.value("staggerindex", std::string("odd")) != "even";
                map->hex_side_length = root.value("hexsidelength", 0);
                if (map->hex_side_length < 0 ||
                    map->hex_side_length > (map->hex_stagger_y ? map->tile_height : map->tile_width))
                {
                    detail::set_error("Invalid Tiled hexsidelength");
                    destroy_tilemap(map);
                    return nullptr;
                }
            }
            if (infinite && orientation == "isometric")
            {
                detail::set_error("Infinite isometric Tiled maps are not supported");
                destroy_tilemap(map);
                return nullptr;
            }
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

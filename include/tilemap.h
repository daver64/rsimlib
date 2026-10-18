#pragma once

#include "atlas.h"

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace sl
{
    /** Flip flags stored in the top bits of a layer cell, matching the Tiled GID flag layout. */
    constexpr std::uint32_t tile_flip_horizontal = 0x80000000u;
    /** Vertical flip flag. */
    constexpr std::uint32_t tile_flip_vertical = 0x40000000u;
    /** Diagonal (transpose) flip flag, applied before the horizontal and vertical flips. */
    constexpr std::uint32_t tile_flip_diagonal = 0x20000000u;
    /** Mask selecting the tile id (GID) from a layer cell. */
    constexpr std::uint32_t tile_id_mask = 0x0FFFFFFFu;

    /** One frame of an animated tile; tile is a tile index local to the owning tileset. */
    struct TileAnimationFrame
    {
        /** Tileset-local tile index shown during this frame. */
        int tile = 0;
        /** Frame length in milliseconds. */
        int duration_ms = 100;
    };

    /** Optional per-tile metadata, keyed by tileset-local tile index. */
    struct TileInfo
    {
        /** True when the tile blocks movement in tilemap_rect_blocked(). */
        bool solid = false;
        /** Arbitrary named properties; non-string Tiled values are stored as their JSON text. */
        std::map<std::string, std::string> properties;
        /** Animation frames; empty for static tiles. */
        std::vector<TileAnimationFrame> animation;
    };

    /** A tile atlas plus the first global tile id it covers. */
    struct TileSet
    {
        /** Tile grid; the bitmap is owned by the map only if it was loaded by load_tiled_map(). */
        Atlas atlas;
        /** Global id of this tileset's tile 0. Ids below this belong to earlier tilesets. */
        int first_gid = 1;
        /** Metadata for individual tiles. */
        std::unordered_map<int, TileInfo> tiles;
    };

    /** A grid of tile cells; each cell is a GID plus optional flip flags, 0 meaning empty. */
    struct TileLayer
    {
        /** Layer name. */
        std::string name;
        /** Width in tiles. */
        int width = 0;
        /** Height in tiles. */
        int height = 0;
        /** Row-major cells. */
        std::vector<std::uint32_t> cells;
        /** Hidden layers are skipped by draw_tilemap(). */
        bool visible = true;
        /** Pixel offset applied to the layer. */
        float offset_x = 0.0f;
        /** Vertical pixel offset applied to the layer. */
        float offset_y = 0.0f;
        /** Camera scroll multiplier; values below 1 give a distant-background effect. */
        float parallax_x = 1.0f;
        /** Vertical camera scroll multiplier. */
        float parallax_y = 1.0f;
    };

    /** A point, rectangle or tile placed in an object layer (spawn points, triggers). */
    struct TileObject
    {
        /** Object name. */
        std::string name;
        /** Object class/type string. */
        std::string type;
        /** Position in map pixels. */
        float x = 0.0f;
        /** Vertical position in map pixels. */
        float y = 0.0f;
        /** Width in pixels; 0 for points. */
        float width = 0.0f;
        /** Height in pixels; 0 for points. */
        float height = 0.0f;
        /** Custom properties. */
        std::map<std::string, std::string> properties;
    };

    /** A named list of objects. */
    struct ObjectLayer
    {
        /** Layer name. */
        std::string name;
        /** Objects in the layer. */
        std::vector<TileObject> objects;
    };

    /** How cells are projected to the screen. */
    enum class TileMapOrientation
    {
        orthogonal,
        isometric
    };

    /** A multi-layer tile map. Create with create_tilemap() or load_tiled_map(); free with destroy_tilemap(). */
    struct TileMap
    {
        /** Cell projection. */
        TileMapOrientation orientation = TileMapOrientation::orthogonal;
        /** Map width in tiles. */
        int width = 0;
        /** Map height in tiles. */
        int height = 0;
        /** Logical cell width in pixels (the diamond width for isometric maps). */
        int tile_width = 0;
        /** Logical cell height in pixels (the diamond height for isometric maps). */
        int tile_height = 0;
        /** Tilesets ordered by first_gid. */
        std::vector<TileSet> tilesets;
        /** Tile layers, drawn in order. */
        std::vector<TileLayer> layers;
        /** Object layers. */
        std::vector<ObjectLayer> object_layers;
        /** Bitmaps destroyed together with the map. */
        std::vector<Bitmap *> owned_bitmaps;
        /** Animation clock advanced by update_tilemap(). */
        double animation_time_ms = 0.0;
    };

    /** Create an empty map; returns nullptr for non-positive dimensions. */
    TileMap *create_tilemap(int width, int height, int tile_width, int tile_height,
                            TileMapOrientation orientation = TileMapOrientation::orthogonal);
    /** Destroy a map and any bitmaps it owns. */
    void destroy_tilemap(TileMap *map);

    /** Append a tileset. A first_gid of 0 continues after the previous tileset. Returns its index. */
    int tilemap_add_tileset(TileMap *map, const Atlas &atlas, int first_gid = 0);
    /** Append an empty layer sized to the map. Returns its index. */
    int tilemap_add_layer(TileMap *map, const std::string &name);
    /** Return a layer by index, or nullptr. */
    TileLayer *tilemap_layer(TileMap *map, int layer);
    /** Return a layer by name, or nullptr. */
    TileLayer *tilemap_layer(TileMap *map, const std::string &name);
    /** Return the raw cell (GID plus flip flags) or 0 when out of range. */
    std::uint32_t tilemap_get(const TileMap *map, int layer, int column, int row);
    /** Set a raw cell; returns false when out of range. */
    bool tilemap_set(TileMap *map, int layer, int column, int row, std::uint32_t cell);

    /** Return the tileset metadata for a GID, or nullptr when the tile has none. */
    TileInfo *tilemap_tile_info(TileMap *map, std::uint32_t gid);
    /** Mark a tile (by GID) solid or not, creating its metadata if needed. */
    void tilemap_set_solid(TileMap *map, std::uint32_t gid, bool solid);
    /** Return a tile property, or an empty string when absent. */
    std::string tilemap_tile_property(const TileMap *map, std::uint32_t gid, const std::string &key);
    /** Return true if the cell holds a solid tile on any layer (or the given layer when layer >= 0). Out of bounds is solid. */
    bool tilemap_is_solid(const TileMap *map, int column, int row, int layer = -1);
    /** Return true if a rectangle in map pixels overlaps a solid tile (orthogonal maps). */
    bool tilemap_rect_blocked(const TileMap *map, float x, float y, float width, float height, int layer = -1);

    /** Convert map pixels to a cell coordinate (orthogonal). Isometric maps use the diamond projection with origin (0, 0). */
    void tilemap_world_to_tile(const TileMap *map, float x, float y, int &column, int &row);
    /** Return the pixel position of a cell's top-left corner (orthogonal) or the diamond centre (isometric). */
    void tilemap_tile_to_world(const TileMap *map, int column, int row, float &x, float &y);

    /** Advance animated tiles by dt seconds. */
    void update_tilemap(TileMap *map, float dt_seconds);

    /**
     * Draw visible layers [first_layer, last_layer] (last_layer < 0 means all). The camera is the map pixel shown at the
     * top-left of the viewport; for isometric maps it is the screen-space offset of cell (0, 0)'s diamond origin.
     * A view size of 0 uses the current screen size.
     */
    void draw_tilemap(const TileMap *map, float camera_x, float camera_y, int view_width = 0, int view_height = 0,
                      int first_layer = 0, int last_layer = -1);

    /**
     * Load a Tiled JSON map (.tmj/.json) with embedded or external JSON (.tsj) tilesets, CSV layer data, flips,
     * tile properties, animations and object layers. Returns nullptr and sets the error on failure.
     */
    TileMap *load_tiled_map(const std::string &path);
    /** Return the first object with the given name across all object layers, or nullptr. */
    const TileObject *tilemap_find_object(const TileMap *map, const std::string &name);
} // namespace sl

#pragma once

#include "draw.h"

namespace sl
{

    /** A bitmap sliced into a uniform grid of equally sized tiles. */
    struct Atlas
    {
        Bitmap *bitmap = nullptr;
        int tile_width = 0;
        int tile_height = 0;
        int spacing = 0;
        int margin = 0;
        int columns = 0;
        int rows = 0;
        int tile_count = 0;
    };

    /** Slice a bitmap into a tile grid; the atlas does not take ownership of the bitmap. */
    Atlas create_atlas(Bitmap *bitmap, int tile_width, int tile_height, int spacing = 0, int margin = 0);
    /** Compute a tile's source rectangle within the atlas bitmap; returns false for an out-of-range index. */
    bool atlas_tile_rect(const Atlas &atlas, int tile_index, int &x, int &y, int &width, int &height);
    /** Draw one atlas tile at the destination position, skipping fully transparent pixels. */
    void atlas_blit(const Atlas &atlas, Bitmap *destination, int tile_index, int destinationX, int destinationY);
    /** Draw one atlas tile scaled into a destination rectangle. */
    void atlas_stretch_blit(const Atlas &atlas, Bitmap *destination, int tile_index, int destinationX, int destinationY, int destinationWidth, int destinationHeight);

} // namespace sl

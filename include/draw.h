#pragma once

#include <SDL2/SDL_stdinc.h>

#include <cstdint>
#include <string>
#include <vector>

namespace simlib::draw {

/** An RGBA colour with 8-bit channels. */
struct Colour {
    Uint8 red;
    Uint8 green;
    Uint8 blue;
    Uint8 alpha = 255;
};

enum class BitmapKind {
    /** A bitmap that can retain pixel data in system memory. */
    Bitmap,
    /** The display framebuffer managed by the display subsystem. */
    Screen
};

/**
 * A bitmap may retain pixels in RAM, on the GPU, or both. Pixel data is RGBA8.
 * The pixel origin and all primitive coordinates are in the top-left corner.
 */
struct Bitmap {
    int width = 0;
    int height = 0;
    std::vector<Uint8> pixels;
    std::uint32_t gpu_texture = 0;
    bool ram_dirty = false;
    bool gpu_dirty = false;
    BitmapKind kind = BitmapKind::Bitmap;
};

/** The current display framebuffer, owned by the display subsystem. */
extern Bitmap* screen;

/** Create a RAM-backed bitmap, or return nullptr for invalid dimensions. */
Bitmap* create_bitmap(int width, int height);
/** Create a GPU-backed bitmap, or return nullptr if allocation fails. */
Bitmap* create_video_bitmap(int width, int height);
/** Load an image file into a bitmap, or return nullptr on failure. */
Bitmap* load_bitmap(const std::string& path);
/** Save a bitmap as an uncompressed PNG, appending .png when no extension is supplied. */
bool save_bitmap(Bitmap* bitmap, const std::string& path);
/** Destroy a bitmap created by this module. */
void destroy_bitmap(Bitmap* bitmap);

/** Acquire a bitmap for CPU access. */
bool acquire_bitmap(Bitmap* bitmap);
/** Release a bitmap acquired for CPU access. */
bool release_bitmap(Bitmap* bitmap);

/** Fill a bitmap with one colour. */
void clear_to_colour(Bitmap* bitmap, Colour colour);
/** Set one pixel if its coordinates are inside the bitmap. */
void putpixel(Bitmap* bitmap, int x, int y, Colour colour);
/** Read one pixel, returning a zero colour for invalid coordinates. */
Colour getpixel(Bitmap* bitmap, int x, int y);
/** Draw an outline circle. */
void circle(Bitmap* bitmap, int x, int y, int radius, Colour colour);
/** Draw a filled circle. */
void circlefill(Bitmap* bitmap, int x, int y, int radius, Colour colour);
/** Draw an outline rectangle using inclusive corner coordinates. */
void rect(Bitmap* bitmap, int left, int top, int right, int bottom, Colour colour);
/** Draw a filled rectangle using inclusive corner coordinates. */
void rectfill(Bitmap* bitmap, int left, int top, int right, int bottom, Colour colour);
/** Draw an outline ellipse. */
void ellipse(Bitmap* bitmap, int x, int y, int radiusX, int radiusY, Colour colour);
/** Draw a filled ellipse. */
void ellipsefill(Bitmap* bitmap, int x, int y, int radiusX, int radiusY, Colour colour);
/** Draw an outline triangle. */
void triangle(Bitmap* bitmap, int x1, int y1, int x2, int y2, int x3, int y3, Colour colour);
/** Draw a filled triangle. */
void trianglefill(Bitmap* bitmap, int x1, int y1, int x2, int y2, int x3, int y3, Colour colour);
/** Draw a line between two points. */
void line(Bitmap* bitmap, int x1, int y1, int x2, int y2, Colour colour);

/** Draw a textured circle outline on the screen bitmap. */
void circle(Bitmap* bitmap, int x, int y, int radius, Bitmap* texture);
/** Draw a textured filled circle on the screen bitmap. */
void circlefill(Bitmap* bitmap, int x, int y, int radius, Bitmap* texture);
/** Draw a textured rectangle outline on the screen bitmap. */
void rect(Bitmap* bitmap, int left, int top, int right, int bottom, Bitmap* texture);
/** Draw a textured filled rectangle on the screen bitmap. */
void rectfill(Bitmap* bitmap, int left, int top, int right, int bottom, Bitmap* texture);
/** Draw a textured ellipse outline on the screen bitmap. */
void ellipse(Bitmap* bitmap, int x, int y, int radiusX, int radiusY, Bitmap* texture);
/** Draw a textured filled ellipse on the screen bitmap. */
void ellipsefill(Bitmap* bitmap, int x, int y, int radiusX, int radiusY, Bitmap* texture);
/** Draw a textured triangle outline on the screen bitmap. */
void triangle(Bitmap* bitmap, int x1, int y1, int x2, int y2, int x3, int y3, Bitmap* texture);
/** Draw a textured filled triangle on the screen bitmap. */
void trianglefill(Bitmap* bitmap, int x1, int y1, int x2, int y2, int x3, int y3, Bitmap* texture);
/** Copy pixels while treating fully transparent source pixels as transparent. */
void masked_blit(Bitmap* source, Bitmap* destination, int sourceX, int sourceY, int destinationX, int destinationY, int width, int height);
/** Scale and copy a bitmap region into a destination region. */
void stretch_blit(Bitmap* source, Bitmap* destination, int sourceX, int sourceY, int sourceWidth, int sourceHeight, int destinationX, int destinationY, int destinationWidth, int destinationHeight);
/** Create a bitmap containing a copied rectangular region. */
Bitmap* create_sub_bitmap(Bitmap* parent, int x, int y, int width, int height);
/** Copy a rectangular bitmap region without scaling. */
void blit(Bitmap* source, Bitmap* destination, int sourceX, int sourceY, int destinationX, int destinationY, int width, int height);

/** Synchronize a bitmap's RAM pixels to its GPU texture. */
bool upload_bitmap(Bitmap* bitmap);
/** Synchronize a bitmap's GPU texture to RAM pixels. */
bool download_bitmap(Bitmap* bitmap);
/** Draw a bitmap at the supplied top-left screen position. */
void draw_sprite(Bitmap* bitmap, int x, int y);
/** Draw a horizontally flipped bitmap. */
void draw_sprite_h_flip(Bitmap* bitmap, int x, int y);
/** Draw a vertically flipped bitmap. */
void draw_sprite_v_flip(Bitmap* bitmap, int x, int y);

namespace detail {

/** Create the display-owned screen bitmap. */
void initialise_screen(int width, int height);
/** Resize the display-owned screen bitmap. */
void resize_screen(int width, int height);
/** Destroy the display-owned screen bitmap. */
void destroy_screen();

} // namespace detail

} // namespace simlib::draw
#pragma once

#include <SDL2/SDL_stdinc.h>

#include <cstdint>
#include <string>
#include <vector>

namespace rvoid::draw {

/** An RGBA colour with 8-bit channels. */
struct Colour {
    Uint8 red;
    Uint8 green;
    Uint8 blue;
    Uint8 alpha = 255;
};

enum class TextureKind {
    /** A bitmap that can retain pixel data in system memory. */
    Bitmap,
    /** The display framebuffer managed by the display subsystem. */
    Screen
};

/**
 * A bitmap may retain pixels in RAM, on the GPU, or both. Pixel data is RGBA8.
 * The pixel origin and all primitive coordinates are in the top-left corner.
 */
struct Texture {
    int width = 0;
    int height = 0;
    std::vector<Uint8> pixels;
    std::uint32_t gpu_texture = 0;
    bool ram_dirty = false;
    bool gpu_dirty = false;
    TextureKind kind = TextureKind::Bitmap;
};

/** The current display framebuffer, owned by the display subsystem. */
extern Texture* screen;

/** Create a RAM-backed bitmap, or return nullptr for invalid dimensions. */
Texture* create_bitmap(int width, int height);
/** Create a GPU-backed bitmap, or return nullptr if allocation fails. */
Texture* create_video_bitmap(int width, int height);
/** Load an image file into a bitmap, or return nullptr on failure. */
Texture* load_bitmap(const std::string& path);
/** Save a bitmap as an uncompressed PNG, appending .png when no extension is supplied. */
bool save_bitmap(Texture* bitmap, const std::string& path);
/** Destroy a bitmap created by this module. */
void destroy_bitmap(Texture* bitmap);

/** Acquire a bitmap for CPU access. */
bool acquire_bitmap(Texture* bitmap);
/** Release a bitmap acquired for CPU access. */
bool release_bitmap(Texture* bitmap);

/** Fill a bitmap with one colour. */
void clear_to_colour(Texture* bitmap, Colour colour);
/** Set one pixel if its coordinates are inside the bitmap. */
void putpixel(Texture* bitmap, int x, int y, Colour colour);
/** Read one pixel, returning a zero colour for invalid coordinates. */
Colour getpixel(Texture* bitmap, int x, int y);
/** Draw an outline circle. */
void circle(Texture* bitmap, int x, int y, int radius, Colour colour);
/** Draw a filled circle. */
void circlefill(Texture* bitmap, int x, int y, int radius, Colour colour);
/** Draw an outline rectangle using inclusive corner coordinates. */
void rect(Texture* bitmap, int left, int top, int right, int bottom, Colour colour);
/** Draw a filled rectangle using inclusive corner coordinates. */
void rectfill(Texture* bitmap, int left, int top, int right, int bottom, Colour colour);
/** Draw an outline ellipse. */
void ellipse(Texture* bitmap, int x, int y, int radiusX, int radiusY, Colour colour);
/** Draw a filled ellipse. */
void ellipsefill(Texture* bitmap, int x, int y, int radiusX, int radiusY, Colour colour);
/** Draw an outline triangle. */
void triangle(Texture* bitmap, int x1, int y1, int x2, int y2, int x3, int y3, Colour colour);
/** Draw a filled triangle. */
void trianglefill(Texture* bitmap, int x1, int y1, int x2, int y2, int x3, int y3, Colour colour);

void circle(Texture* bitmap, int x, int y, int radius, Texture* texture);
void circlefill(Texture* bitmap, int x, int y, int radius, Texture* texture);
void rect(Texture* bitmap, int left, int top, int right, int bottom, Texture* texture);
void rectfill(Texture* bitmap, int left, int top, int right, int bottom, Texture* texture);
void ellipse(Texture* bitmap, int x, int y, int radiusX, int radiusY, Texture* texture);
void ellipsefill(Texture* bitmap, int x, int y, int radiusX, int radiusY, Texture* texture);
void triangle(Texture* bitmap, int x1, int y1, int x2, int y2, int x3, int y3, Texture* texture);
void trianglefill(Texture* bitmap, int x1, int y1, int x2, int y2, int x3, int y3, Texture* texture);
void blit(Texture* source, Texture* destination, int sourceX, int sourceY, int destinationX, int destinationY, int width, int height);

/** Synchronize a bitmap's RAM pixels to its GPU texture. */
bool upload_bitmap(Texture* bitmap);
/** Synchronize a bitmap's GPU texture to RAM pixels. */
bool download_bitmap(Texture* bitmap);
/** Draw a bitmap at the supplied top-left screen position. */
void draw_sprite(Texture* bitmap, int x, int y);

namespace detail {

void initialise_screen(int width, int height);
void resize_screen(int width, int height);
void destroy_screen();

} // namespace detail

} // namespace rvoid::draw
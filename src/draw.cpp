#include "draw.h"

#include "display.h"
#include "error.h"
#include "resource.h"

#include <SDL2/SDL_image.h>
#include <SDL2/SDL_opengl.h>
#include <png.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <cstdio>

namespace simlib {
Bitmap* screen = nullptr;

namespace {

constexpr std::size_t bytes_per_pixel = 4;

/** Check whether a bitmap has usable dimensions. */
bool is_valid(const Bitmap* bitmap) {
	return bitmap && bitmap->width > 0 && bitmap->height > 0;
}

/** Check whether a bitmap is the display-owned screen. */
bool is_screen(const Bitmap* bitmap) {
	return bitmap && bitmap->kind == BitmapKind::Screen;
}

/** Return the byte offset of one RGBA pixel. */
std::size_t pixel_offset(const Bitmap& bitmap, int x, int y) {
	return (static_cast<std::size_t>(y) * bitmap.width + x) * bytes_per_pixel;
}

/** Ensure a bitmap has an allocated OpenGL texture. */
bool ensure_gpu_texture(Bitmap* bitmap) {
	if (!is_valid(bitmap)) {
		return false;
	}
	if (bitmap->gpu_texture == 0) {
		GLuint texture = 0;
		glGenTextures(1, &texture);
		if (texture == 0) {
			return false;
		}
		bitmap->gpu_texture = texture;
		glBindTexture(GL_TEXTURE_2D, bitmap->gpu_texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap->width, bitmap->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		bitmap->gpu_dirty = !bitmap->pixels.empty();
	}
	return true;
}

/** Ensure a bitmap has current CPU-side pixel storage. */
bool ensure_ram_pixels(Bitmap* bitmap) {
	if (!is_valid(bitmap)) {
		return false;
	}
	if (is_screen(bitmap)) {
		return download_bitmap(bitmap);
	}
	if (bitmap->pixels.empty()) {
		bitmap->pixels.resize(static_cast<std::size_t>(bitmap->width) * bitmap->height * bytes_per_pixel);
		if (bitmap->gpu_texture != 0 && !bitmap->gpu_dirty) {
			return download_bitmap(bitmap);
		}
	}
	return true;
}

/** Push the 2D top-left-origin screen projection. */
void set_projection() {
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(
		0.0,
		static_cast<double>(screen_width()),
		static_cast<double>(screen_height()),
		0.0,
		-1.0,
		1.0
	);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
}

/** Restore the projection and modelview stacks. */
void restore_projection() {
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

/** Draw a bitmap region as a scaled and optionally flipped quad. */
void draw_textured_quad(Bitmap* bitmap, int sourceX, int sourceY, int width, int height, float x, float y, int destinationWidth = -1, int destinationHeight = -1, bool flipHorizontal = false, bool flipVertical = false) {
	if (!upload_bitmap(bitmap)) {
		return;
	}

	set_projection();
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindTexture(GL_TEXTURE_2D, bitmap->gpu_texture);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	const float leftTexture = static_cast<float>(sourceX) / bitmap->width;
	const float topTexture = static_cast<float>(sourceY) / bitmap->height;
	const float rightTexture = static_cast<float>(sourceX + width) / bitmap->width;
	const float bottomTexture = static_cast<float>(sourceY + height) / bitmap->height;
	const float right = x + static_cast<float>(destinationWidth < 0 ? width : destinationWidth);
	const float bottom = y + static_cast<float>(destinationHeight < 0 ? height : destinationHeight);
	const float textureLeft = flipHorizontal ? rightTexture : leftTexture;
	const float textureRight = flipHorizontal ? leftTexture : rightTexture;
	const float textureTop = flipVertical ? bottomTexture : topTexture;
	const float textureBottom = flipVertical ? topTexture : bottomTexture;
	glBegin(GL_QUADS);
	glTexCoord2f(textureLeft, textureTop); glVertex2f(x, y);
	glTexCoord2f(textureRight, textureTop); glVertex2f(right, y);
	glTexCoord2f(textureRight, textureBottom); glVertex2f(right, bottom);
	glTexCoord2f(textureLeft, textureBottom); glVertex2f(x, bottom);
	glEnd();

	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	restore_projection();
}

constexpr float pi = 3.14159265358979323846f;

/** Begin an untextured screen primitive. */
void begin_screen_plain(Colour colour) {
	set_projection();
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4ub(colour.red, colour.green, colour.blue, colour.alpha);
}

/** Begin a textured screen primitive. */
bool begin_screen_texture(Bitmap* texture) {
	if (!texture || is_screen(texture) || !upload_bitmap(texture)) {
		return false;
	}
	set_projection();
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindTexture(GL_TEXTURE_2D, texture->gpu_texture);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	return true;
}

/** End a screen primitive and restore OpenGL state. */
void end_screen_shape(bool textured) {
	glDisable(GL_BLEND);
	if (textured) {
		glDisable(GL_TEXTURE_2D);
	}
	restore_projection();
}

/** Rasterize a line into a bitmap using integer coordinates. */
void draw_line(Bitmap* bitmap, int x1, int y1, int x2, int y2, Colour colour) {
	const int deltaX = std::abs(x2 - x1);
	const int stepX = x1 < x2 ? 1 : -1;
	const int deltaY = -std::abs(y2 - y1);
	const int stepY = y1 < y2 ? 1 : -1;
	int error = deltaX + deltaY;
	for (;;) {
		putpixel(bitmap, x1, y1, colour);
		if (x1 == x2 && y1 == y2) {
			return;
		}
		const int doubledError = error * 2;
		if (doubledError >= deltaY) {
			error += deltaY;
			x1 += stepX;
		}
		if (doubledError <= deltaX) {
			error += deltaX;
			y1 += stepY;
		}
	}
}

/** Render a plain or textured ellipse directly to the screen. */
void draw_screen_ellipse(float x, float y, float radiusX, float radiusY, bool filled, Bitmap* texture, Colour colour) {
	const int segments = std::max(16, std::min(256, static_cast<int>(std::max(radiusX, radiusY) * 2.0f)));
	const bool textured = texture != nullptr;
	if (textured ? !begin_screen_texture(texture) : (begin_screen_plain(colour), false)) {
		return;
	}
	glBegin(filled ? GL_TRIANGLE_FAN : GL_LINE_LOOP);
	if (filled) {
		if (textured) {
			glTexCoord2f(0.5f, 0.5f);
		}
		glVertex2f(x, y);
	}
	for (int index = 0; index <= (filled ? segments : segments - 1); ++index) {
		const float angle = 2.0f * pi * index / segments;
		const float u = 0.5f + 0.5f * std::cos(angle);
		const float v = 0.5f + 0.5f * std::sin(angle);
		if (textured) {
			glTexCoord2f(u, v);
		}
		glVertex2f(x + radiusX * std::cos(angle), y + radiusY * std::sin(angle));
	}
	glEnd();
	end_screen_shape(textured);
}

/** Render a plain or textured triangle directly to the screen. */
void draw_screen_triangle(float x1, float y1, float x2, float y2, float x3, float y3, bool filled, Bitmap* texture, Colour colour) {
	const bool textured = texture != nullptr;
	if (textured ? !begin_screen_texture(texture) : (begin_screen_plain(colour), false)) {
		return;
	}
	glBegin(filled ? GL_TRIANGLES : GL_LINE_LOOP);
	if (textured) glTexCoord2f(0.0f, 0.0f);
	glVertex2f(x1, y1);
	if (textured) glTexCoord2f(1.0f, 0.0f);
	glVertex2f(x2, y2);
	if (textured) glTexCoord2f(0.5f, 1.0f);
	glVertex2f(x3, y3);
	glEnd();
	end_screen_shape(textured);
}

/** Render a plain or textured rectangle directly to the screen. */
void draw_screen_rect(float left, float top, float right, float bottom, bool filled, Bitmap* texture, Colour colour) {
	const bool textured = texture != nullptr;
	if (textured ? !begin_screen_texture(texture) : (begin_screen_plain(colour), false)) {
		return;
	}
	glBegin(filled ? GL_QUADS : GL_LINE_LOOP);
	if (textured) glTexCoord2f(0.0f, 0.0f);
	glVertex2f(left, top);
	if (textured) glTexCoord2f(1.0f, 0.0f);
	glVertex2f(right, top);
	if (textured) glTexCoord2f(1.0f, 1.0f);
	glVertex2f(right, bottom);
	if (textured) glTexCoord2f(0.0f, 1.0f);
	glVertex2f(left, bottom);
	glEnd();
	end_screen_shape(textured);
}

} // namespace

/** Create a RAM-backed bitmap. */
Bitmap* create_bitmap(int width, int height) {
	if (width <= 0 || height <= 0) {
		simlib::detail::set_error("Bitmap dimensions must be positive");
		return nullptr;
	}
	Bitmap* bitmap = new Bitmap;
	bitmap->width = width;
	bitmap->height = height;
	bitmap->pixels.resize(static_cast<std::size_t>(width) * height * bytes_per_pixel);
	bitmap->ram_dirty = true;
	return bitmap;
}

/** Create a GPU-backed bitmap. */
Bitmap* create_video_bitmap(int width, int height) {
	Bitmap* bitmap = create_bitmap(width, height);
	if (!bitmap || !upload_bitmap(bitmap)) {
		destroy_bitmap(bitmap);
		return nullptr;
	}
	bitmap->pixels.clear();
	bitmap->pixels.shrink_to_fit();
	bitmap->ram_dirty = false;
	return bitmap;
}

/** Load an image file into a bitmap. */
Bitmap* load_bitmap_from_surface(SDL_Surface* loaded) {
	if (!loaded) {
		return nullptr;
	}

	SDL_Surface* rgba = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
	SDL_FreeSurface(loaded);
	if (!rgba) {
		simlib::detail::set_error(SDL_GetError());
		return nullptr;
	}

	Bitmap* bitmap = create_bitmap(rgba->w, rgba->h);
	if (bitmap) {
		const std::size_t rowBytes = static_cast<std::size_t>(rgba->w) * bytes_per_pixel;
		const auto* sourcePixels = static_cast<const Uint8*>(rgba->pixels);
		for (int y = 0; y < rgba->h; ++y) {
			std::memcpy(bitmap->pixels.data() + static_cast<std::size_t>(y) * rowBytes, sourcePixels + static_cast<std::size_t>(y) * rgba->pitch, rowBytes);
		}
	}
	SDL_FreeSurface(rgba);
	return bitmap;
}

Bitmap* load_bitmap(const std::string& path) {
	SDL_Surface* loaded = IMG_Load(path.c_str());
	if (!loaded) simlib::detail::set_error(IMG_GetError());
	return load_bitmap_from_surface(loaded);
}

Bitmap* load_bitmap_from_memory(const std::uint8_t* data, std::size_t size) {
	if (!data || size == 0 || size > std::numeric_limits<int>::max()) return nullptr;
	SDL_RWops* rw = SDL_RWFromConstMem(data, static_cast<int>(size));
	if (!rw) {
		simlib::detail::set_error(SDL_GetError());
		return nullptr;
	}
	SDL_Surface* loaded = IMG_Load_RW(rw, 1);
	if (!loaded) simlib::detail::set_error(IMG_GetError());
	return load_bitmap_from_surface(loaded);
}

Bitmap* load_bitmap(const Archive& archive, const std::string& name) {
	const auto bytes = archive.read(name);
	return load_bitmap_from_memory(bytes.data(), bytes.size());
}

/** Save a bitmap as an uncompressed PNG. */
bool save_bitmap(Bitmap* bitmap, const std::string& path) {
	if (!bitmap || path.empty() || !acquire_bitmap(bitmap)) {
		return false;
	}

	std::filesystem::path outputPath(path);
	if (outputPath.extension().empty()) {
		outputPath += ".png";
	}

	FILE* file = std::fopen(outputPath.string().c_str(), "wb");
	if (!file) {
		simlib::detail::set_error("Unable to open bitmap output file");
		return false;
	}

	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	png_infop info = png ? png_create_info_struct(png) : nullptr;
	if (!png || !info) {
		png_destroy_write_struct(png ? &png : nullptr, info ? &info : nullptr);
		std::fclose(file);
		return false;
	}

	if (setjmp(png_jmpbuf(png))) {
		png_destroy_write_struct(&png, &info);
		std::fclose(file);
		return false;
	}

	png_init_io(png, file);
	png_set_compression_level(png, 0);
	png_set_filter(png, 0, PNG_FILTER_NONE);
	png_set_IHDR(
		png,
		info,
		static_cast<png_uint_32>(bitmap->width),
		static_cast<png_uint_32>(bitmap->height),
		8,
		PNG_COLOR_TYPE_RGBA,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_BASE,
		PNG_FILTER_TYPE_BASE
	);

	const std::size_t rowBytes = static_cast<std::size_t>(bitmap->width) * bytes_per_pixel;
	png_write_info(png, info);
	for (int y = 0; y < bitmap->height; ++y) {
		png_write_row(png, bitmap->pixels.data() + static_cast<std::size_t>(y) * rowBytes);
	}
	png_write_end(png, info);

	png_destroy_write_struct(&png, &info);
	std::fclose(file);
	return true;
}

/** Destroy a bitmap and its GPU resources. */
void destroy_bitmap(Bitmap* bitmap) {
	if (!bitmap || is_screen(bitmap)) {
		return;
	}
	if (bitmap->gpu_texture != 0) {
		const GLuint texture = bitmap->gpu_texture;
		glDeleteTextures(1, &texture);
	}
	delete bitmap;
}

/** Make a bitmap's pixels available in RAM. */
bool acquire_bitmap(Bitmap* bitmap) {
	if (is_screen(bitmap)) {
		return download_bitmap(bitmap);
	}
	return ensure_ram_pixels(bitmap);
}

/** Upload a bitmap's modified RAM pixels to the GPU. */
bool release_bitmap(Bitmap* bitmap) {
	if (is_screen(bitmap)) {
		return false;
	}
	return upload_bitmap(bitmap);
}

/** Fill a bitmap with one colour. */
void clear_to_colour(Bitmap* bitmap, Colour colour) {
	if (is_screen(bitmap)) {
		glClearColor(
			static_cast<float>(colour.red) / 255.0f,
			static_cast<float>(colour.green) / 255.0f,
			static_cast<float>(colour.blue) / 255.0f,
			static_cast<float>(colour.alpha) / 255.0f
		);
		glClear(GL_COLOR_BUFFER_BIT);
		return;
	}
	if (!ensure_ram_pixels(bitmap)) {
		return;
	}
	for (int y = 0; y < bitmap->height; ++y) {
		for (int x = 0; x < bitmap->width; ++x) {
			putpixel(bitmap, x, y, colour);
		}
	}
}

/** Set one pixel in a bitmap. */
void putpixel(Bitmap* bitmap, int x, int y, Colour colour) {
	if (is_screen(bitmap)) {
		if (x < 0 || x >= bitmap->width || y < 0 || y >= bitmap->height) {
			return;
		}
		set_projection();
		glDisable(GL_TEXTURE_2D);
		glColor4ub(colour.red, colour.green, colour.blue, colour.alpha);
		glBegin(GL_POINTS);
		glVertex2i(x, y);
		glEnd();
		restore_projection();
		return;
	}
	if (!ensure_ram_pixels(bitmap) || x < 0 || x >= bitmap->width || y < 0 || y >= bitmap->height) {
		return;
	}
	const std::size_t offset = pixel_offset(*bitmap, x, y);
	bitmap->pixels[offset] = colour.red;
	bitmap->pixels[offset + 1] = colour.green;
	bitmap->pixels[offset + 2] = colour.blue;
	bitmap->pixels[offset + 3] = colour.alpha;
	bitmap->ram_dirty = true;
}

/** Read one pixel from a bitmap. */
Colour getpixel(Bitmap* bitmap, int x, int y) {
	if (!ensure_ram_pixels(bitmap) || x < 0 || x >= bitmap->width || y < 0 || y >= bitmap->height) {
		return {};
	}
	const std::size_t offset = pixel_offset(*bitmap, x, y);
	return {bitmap->pixels[offset], bitmap->pixels[offset + 1], bitmap->pixels[offset + 2], bitmap->pixels[offset + 3]};
}

/** Draw an outline circle. */
void circle(Bitmap* bitmap, float x, float y, float radius, Colour colour) {
	if (!bitmap || radius < 0.0f) return;
	if (is_screen(bitmap)) {
		draw_screen_ellipse(x, y, radius, radius, false, nullptr, colour);
		return;
	}
	const int ix = static_cast<int>(std::lround(x));
	const int iy = static_cast<int>(std::lround(y));
	const int iradius = static_cast<int>(std::lround(radius));
	for (int degrees = 0; degrees < 360; ++degrees) {
		const float angle = degrees * pi / 180.0f;
		putpixel(bitmap, ix + static_cast<int>(std::lround(iradius * std::cos(angle))), iy + static_cast<int>(std::lround(iradius * std::sin(angle))), colour);
	}
}

/** Draw a filled circle. */
void circlefill(Bitmap* bitmap, float x, float y, float radius, Colour colour) {
	if (!bitmap || radius < 0.0f) return;
	if (is_screen(bitmap)) {
		draw_screen_ellipse(x, y, radius, radius, true, nullptr, colour);
		return;
	}
	const int ix = static_cast<int>(std::lround(x));
	const int iy = static_cast<int>(std::lround(y));
	const int iradius = static_cast<int>(std::lround(radius));
	for (int offsetY = -iradius; offsetY <= iradius; ++offsetY) {
		const int halfWidth = static_cast<int>(std::sqrt(iradius * iradius - offsetY * offsetY));
		for (int offsetX = -halfWidth; offsetX <= halfWidth; ++offsetX) putpixel(bitmap, ix + offsetX, iy + offsetY, colour);
	}
}

/** Draw an outline rectangle. */
void rect(Bitmap* bitmap, float left, float top, float right, float bottom, Colour colour) {
	if (is_screen(bitmap)) {
		draw_screen_rect(left, top, right, bottom, false, nullptr, colour);
		return;
	}
	const int ileft = static_cast<int>(std::lround(left));
	const int itop = static_cast<int>(std::lround(top));
	const int iright = static_cast<int>(std::lround(right));
	const int ibottom = static_cast<int>(std::lround(bottom));
	draw_line(bitmap, ileft, itop, iright, itop, colour);
	draw_line(bitmap, iright, itop, iright, ibottom, colour);
	draw_line(bitmap, iright, ibottom, ileft, ibottom, colour);
	draw_line(bitmap, ileft, ibottom, ileft, itop, colour);
}

/** Draw a filled rectangle. */
void rectfill(Bitmap* bitmap, float left, float top, float right, float bottom, Colour colour) {
	if (is_screen(bitmap)) {
		left = std::clamp(left, 0.0f, static_cast<float>(bitmap->width));
		right = std::clamp(right, 0.0f, static_cast<float>(bitmap->width));
		top = std::clamp(top, 0.0f, static_cast<float>(bitmap->height));
		bottom = std::clamp(bottom, 0.0f, static_cast<float>(bitmap->height));
		set_projection();
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4ub(colour.red, colour.green, colour.blue, colour.alpha);
		glBegin(GL_QUADS);
		glVertex2f(left, top);
		glVertex2f(right, top);
		glVertex2f(right, bottom);
		glVertex2f(left, bottom);
		glEnd();
		glDisable(GL_BLEND);
		restore_projection();
		return;
	}
	if (!ensure_ram_pixels(bitmap)) {
		return;
	}
	const int ileft = std::clamp(static_cast<int>(std::lround(left)), 0, bitmap->width);
	const int iright = std::clamp(static_cast<int>(std::lround(right)), 0, bitmap->width);
	const int itop = std::clamp(static_cast<int>(std::lround(top)), 0, bitmap->height);
	const int ibottom = std::clamp(static_cast<int>(std::lround(bottom)), 0, bitmap->height);
	for (int y = itop; y < ibottom; ++y) {
		for (int x = ileft; x < iright; ++x) {
			putpixel(bitmap, x, y, colour);
		}
	}
}

/** Draw an outline ellipse. */
void ellipse(Bitmap* bitmap, float x, float y, float radiusX, float radiusY, Colour colour) {
	if (!bitmap || radiusX < 0.0f || radiusY < 0.0f) return;
	if (is_screen(bitmap)) {
		draw_screen_ellipse(x, y, radiusX, radiusY, false, nullptr, colour);
		return;
	}
	const int ix = static_cast<int>(std::lround(x));
	const int iy = static_cast<int>(std::lround(y));
	for (int degrees = 0; degrees < 360; ++degrees) {
		const float angle = degrees * pi / 180.0f;
		putpixel(bitmap, ix + static_cast<int>(std::lround(radiusX * std::cos(angle))), iy + static_cast<int>(std::lround(radiusY * std::sin(angle))), colour);
	}
}

/** Draw a filled ellipse. */
void ellipsefill(Bitmap* bitmap, float x, float y, float radiusX, float radiusY, Colour colour) {
	if (!bitmap || radiusX < 0.0f || radiusY < 0.0f) return;
	if (is_screen(bitmap)) {
		draw_screen_ellipse(x, y, radiusX, radiusY, true, nullptr, colour);
		return;
	}
	const int ix = static_cast<int>(std::lround(x));
	const int iy = static_cast<int>(std::lround(y));
	const int iradiusY = static_cast<int>(std::lround(radiusY));
	for (int offsetY = -iradiusY; offsetY <= iradiusY; ++offsetY) {
		const float ratio = iradiusY == 0 ? 0.0f : static_cast<float>(offsetY) / iradiusY;
		const int halfWidth = static_cast<int>(std::sqrt(std::max(0.0f, 1.0f - ratio * ratio)) * radiusX);
		for (int offsetX = -halfWidth; offsetX <= halfWidth; ++offsetX) putpixel(bitmap, ix + offsetX, iy + offsetY, colour);
	}
}

/** Draw an outline triangle. */
void triangle(Bitmap* bitmap, float x1, float y1, float x2, float y2, float x3, float y3, Colour colour) {
	if (is_screen(bitmap)) {
		draw_screen_triangle(x1, y1, x2, y2, x3, y3, false, nullptr, colour);
		return;
	}
	draw_line(bitmap, static_cast<int>(std::lround(x1)), static_cast<int>(std::lround(y1)), static_cast<int>(std::lround(x2)), static_cast<int>(std::lround(y2)), colour);
	draw_line(bitmap, static_cast<int>(std::lround(x2)), static_cast<int>(std::lround(y2)), static_cast<int>(std::lround(x3)), static_cast<int>(std::lround(y3)), colour);
	draw_line(bitmap, static_cast<int>(std::lround(x3)), static_cast<int>(std::lround(y3)), static_cast<int>(std::lround(x1)), static_cast<int>(std::lround(y1)), colour);
}

/** Draw a filled triangle. */
void trianglefill(Bitmap* bitmap, float x1, float y1, float x2, float y2, float x3, float y3, Colour colour) {
	if (is_screen(bitmap)) {
		draw_screen_triangle(x1, y1, x2, y2, x3, y3, true, nullptr, colour);
		return;
	}
	const int ix1 = static_cast<int>(std::lround(x1));
	const int iy1 = static_cast<int>(std::lround(y1));
	const int ix2 = static_cast<int>(std::lround(x2));
	const int iy2 = static_cast<int>(std::lround(y2));
	const int ix3 = static_cast<int>(std::lround(x3));
	const int iy3 = static_cast<int>(std::lround(y3));
	const int minimumX = std::min({ix1, ix2, ix3});
	const int maximumX = std::max({ix1, ix2, ix3});
	const int minimumY = std::min({iy1, iy2, iy3});
	const int maximumY = std::max({iy1, iy2, iy3});
	const int area = (ix2 - ix1) * (iy3 - iy1) - (iy2 - iy1) * (ix3 - ix1);
	if (area == 0) return;
	for (int y = minimumY; y <= maximumY; ++y) for (int x = minimumX; x <= maximumX; ++x) {
		const int a = (ix2 - ix1) * (y - iy1) - (iy2 - iy1) * (x - ix1);
		const int b = (ix3 - ix2) * (y - iy2) - (iy3 - iy2) * (x - ix2);
		const int c = (ix1 - ix3) * (y - iy3) - (iy1 - iy3) * (x - ix3);
		if ((a >= 0 && b >= 0 && c >= 0) || (a <= 0 && b <= 0 && c <= 0)) putpixel(bitmap, x, y, colour);
	}
}

/** Draw a line between two points. */
void line(Bitmap* bitmap, float x1, float y1, float x2, float y2, Colour colour) {
	if (!bitmap) {
		return;
	}
	draw_line(bitmap, static_cast<int>(std::lround(x1)), static_cast<int>(std::lround(y1)), static_cast<int>(std::lround(x2)), static_cast<int>(std::lround(y2)), colour);
}

/** Draw a textured circle outline. */
void circle(Bitmap* bitmap, float x, float y, float radius, Bitmap* texture) { if (is_screen(bitmap) && radius >= 0.0f) draw_screen_ellipse(x, y, radius, radius, false, texture, {}); }
/** Draw a textured filled circle. */
void circlefill(Bitmap* bitmap, float x, float y, float radius, Bitmap* texture) { if (is_screen(bitmap) && radius >= 0.0f) draw_screen_ellipse(x, y, radius, radius, true, texture, {}); }
/** Draw a textured rectangle outline. */
void rect(Bitmap* bitmap, float left, float top, float right, float bottom, Bitmap* texture) { if (is_screen(bitmap)) draw_screen_rect(left, top, right, bottom, false, texture, {}); }
/** Draw a textured filled rectangle. */
void rectfill(Bitmap* bitmap, float left, float top, float right, float bottom, Bitmap* texture) { if (is_screen(bitmap)) draw_screen_rect(left, top, right, bottom, true, texture, {}); }
/** Draw a textured ellipse outline. */
void ellipse(Bitmap* bitmap, float x, float y, float radiusX, float radiusY, Bitmap* texture) { if (is_screen(bitmap) && radiusX >= 0.0f && radiusY >= 0.0f) draw_screen_ellipse(x, y, radiusX, radiusY, false, texture, {}); }
/** Draw a textured filled ellipse. */
void ellipsefill(Bitmap* bitmap, float x, float y, float radiusX, float radiusY, Bitmap* texture) { if (is_screen(bitmap) && radiusX >= 0.0f && radiusY >= 0.0f) draw_screen_ellipse(x, y, radiusX, radiusY, true, texture, {}); }
/** Draw a textured triangle outline. */
void triangle(Bitmap* bitmap, float x1, float y1, float x2, float y2, float x3, float y3, Bitmap* texture) { if (is_screen(bitmap)) draw_screen_triangle(x1, y1, x2, y2, x3, y3, false, texture, {}); }
/** Draw a textured filled triangle. */
void trianglefill(Bitmap* bitmap, float x1, float y1, float x2, float y2, float x3, float y3, Bitmap* texture) { if (is_screen(bitmap)) draw_screen_triangle(x1, y1, x2, y2, x3, y3, true, texture, {}); }

/** Copy a rectangular bitmap region without scaling. */
void blit(Bitmap* source, Bitmap* destination, int sourceX, int sourceY, float destinationX, float destinationY, int width, int height) {
	if (is_screen(destination)) {
		if (!source || is_screen(source)) {
			return;
		}
		const int clippedSourceX = std::max(sourceX, 0);
		const int clippedSourceY = std::max(sourceY, 0);
		const int clippedWidth = std::min(width - (clippedSourceX - sourceX), source->width - clippedSourceX);
		const int clippedHeight = std::min(height - (clippedSourceY - sourceY), source->height - clippedSourceY);
		if (clippedWidth > 0 && clippedHeight > 0) {
			draw_textured_quad(source, clippedSourceX, clippedSourceY, clippedWidth, clippedHeight, destinationX + static_cast<float>(clippedSourceX - sourceX), destinationY + static_cast<float>(clippedSourceY - sourceY));
		}
		return;
	}
	if (!ensure_ram_pixels(source) || !ensure_ram_pixels(destination) || width <= 0 || height <= 0) {
		return;
	}
	const int idestinationX = static_cast<int>(std::lround(destinationX));
	const int idestinationY = static_cast<int>(std::lround(destinationY));
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const int fromX = sourceX + x;
			const int fromY = sourceY + y;
			const int toX = idestinationX + x;
			const int toY = idestinationY + y;
			if (fromX >= 0 && fromX < source->width && fromY >= 0 && fromY < source->height && toX >= 0 && toX < destination->width && toY >= 0 && toY < destination->height) {
				const std::size_t sourceOffset = pixel_offset(*source, fromX, fromY);
				const std::size_t destinationOffset = pixel_offset(*destination, toX, toY);
				std::memcpy(destination->pixels.data() + destinationOffset, source->pixels.data() + sourceOffset, bytes_per_pixel);
			}
		}
	}
	destination->ram_dirty = true;
}

/** Copy a bitmap region while skipping transparent pixels. */
void masked_blit(Bitmap* source, Bitmap* destination, int sourceX, int sourceY, float destinationX, float destinationY, int width, int height) {
	if (!source || !destination || width <= 0 || height <= 0 || !ensure_ram_pixels(source)) {
		return;
	}
	const int idestinationX = static_cast<int>(std::lround(destinationX));
	const int idestinationY = static_cast<int>(std::lround(destinationY));
	if (is_screen(destination)) {
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				const int sourcePixelX = sourceX + x;
				const int sourcePixelY = sourceY + y;
				if (sourcePixelX < 0 || sourcePixelX >= source->width || sourcePixelY < 0 || sourcePixelY >= source->height) continue;
				const std::size_t offset = pixel_offset(*source, sourcePixelX, sourcePixelY);
				if (source->pixels[offset + 3] != 0) putpixel(destination, idestinationX + x, idestinationY + y, {source->pixels[offset], source->pixels[offset + 1], source->pixels[offset + 2], source->pixels[offset + 3]});
			}
		}
		return;
	}
	if (!ensure_ram_pixels(destination)) return;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const int sourcePixelX = sourceX + x;
			const int sourcePixelY = sourceY + y;
			const int destinationPixelX = idestinationX + x;
			const int destinationPixelY = idestinationY + y;
			if (sourcePixelX < 0 || sourcePixelX >= source->width || sourcePixelY < 0 || sourcePixelY >= source->height || destinationPixelX < 0 || destinationPixelX >= destination->width || destinationPixelY < 0 || destinationPixelY >= destination->height) continue;
			const std::size_t sourceOffset = pixel_offset(*source, sourcePixelX, sourcePixelY);
			if (source->pixels[sourceOffset + 3] != 0) std::memcpy(destination->pixels.data() + pixel_offset(*destination, destinationPixelX, destinationPixelY), source->pixels.data() + sourceOffset, bytes_per_pixel);
		}
	}
	destination->ram_dirty = true;
}

/** Scale and copy a bitmap region. */
void stretch_blit(Bitmap* source, Bitmap* destination, int sourceX, int sourceY, int sourceWidth, int sourceHeight, float destinationX, float destinationY, int destinationWidth, int destinationHeight) {
	if (!source || !destination || sourceWidth <= 0 || sourceHeight <= 0 || destinationWidth <= 0 || destinationHeight <= 0) return;
	if (is_screen(destination)) {
		draw_textured_quad(source, sourceX, sourceY, sourceWidth, sourceHeight, destinationX, destinationY, destinationWidth, destinationHeight);
		return;
	}
	if (!ensure_ram_pixels(source) || !ensure_ram_pixels(destination)) return;
	const int idestinationX = static_cast<int>(std::lround(destinationX));
	const int idestinationY = static_cast<int>(std::lround(destinationY));
	for (int y = 0; y < destinationHeight; ++y) {
		for (int x = 0; x < destinationWidth; ++x) {
			const int sourcePixelX = sourceX + x * sourceWidth / destinationWidth;
			const int sourcePixelY = sourceY + y * sourceHeight / destinationHeight;
			const int destinationPixelX = idestinationX + x;
			const int destinationPixelY = idestinationY + y;
			if (sourcePixelX < 0 || sourcePixelX >= source->width || sourcePixelY < 0 || sourcePixelY >= source->height || destinationPixelX < 0 || destinationPixelX >= destination->width || destinationPixelY < 0 || destinationPixelY >= destination->height) continue;
			std::memcpy(destination->pixels.data() + pixel_offset(*destination, destinationPixelX, destinationPixelY), source->pixels.data() + pixel_offset(*source, sourcePixelX, sourcePixelY), bytes_per_pixel);
		}
	}
	destination->ram_dirty = true;
}

/** Create a bitmap containing a copied rectangular region. */
Bitmap* create_sub_bitmap(Bitmap* parent, int x, int y, int width, int height) {
	if (!parent || width <= 0 || height <= 0 || !ensure_ram_pixels(parent)) return nullptr;
	Bitmap* bitmap = create_bitmap(width, height);
	if (!bitmap) return nullptr;
	blit(parent, bitmap, x, y, 0, 0, width, height);
	return bitmap;
}

/** Upload a bitmap's RAM pixels to its GPU texture. */
bool upload_bitmap(Bitmap* bitmap) {
	if (is_screen(bitmap)) {
		return false;
	}
	if (!ensure_gpu_texture(bitmap)) {
		return false;
	}
	if (!bitmap->ram_dirty || bitmap->pixels.empty()) {
		return true;
	}
	glBindTexture(GL_TEXTURE_2D, bitmap->gpu_texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, bitmap->width, bitmap->height, GL_RGBA, GL_UNSIGNED_BYTE, bitmap->pixels.data());
	bitmap->ram_dirty = false;
	bitmap->gpu_dirty = false;
	return true;
}

/** Download a bitmap's GPU texture into RAM pixels. */
bool download_bitmap(Bitmap* bitmap) {
	if (!is_valid(bitmap)) {
		return false;
	}
	bitmap->pixels.resize(static_cast<std::size_t>(bitmap->width) * bitmap->height * bytes_per_pixel);
	if (is_screen(bitmap)) {
		glReadPixels(0, 0, bitmap->width, bitmap->height, GL_RGBA, GL_UNSIGNED_BYTE, bitmap->pixels.data());
		const std::size_t rowBytes = static_cast<std::size_t>(bitmap->width) * bytes_per_pixel;
		std::vector<Uint8> row(rowBytes);
		for (int top = 0, bottom = bitmap->height - 1; top < bottom; ++top, --bottom) {
			std::memcpy(row.data(), bitmap->pixels.data() + static_cast<std::size_t>(top) * rowBytes, rowBytes);
			std::memcpy(bitmap->pixels.data() + static_cast<std::size_t>(top) * rowBytes, bitmap->pixels.data() + static_cast<std::size_t>(bottom) * rowBytes, rowBytes);
			std::memcpy(bitmap->pixels.data() + static_cast<std::size_t>(bottom) * rowBytes, row.data(), rowBytes);
		}
		bitmap->ram_dirty = false;
		return true;
	}
	if (bitmap->gpu_texture == 0) {
		return false;
	}
	glBindTexture(GL_TEXTURE_2D, bitmap->gpu_texture);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap->pixels.data());
	bitmap->ram_dirty = false;
	bitmap->gpu_dirty = false;
	return true;
}

/** Draw a bitmap at a screen position. */
void draw_sprite(Bitmap* bitmap, float x, float y) {
	if (!bitmap || is_screen(bitmap) || screen_width() <= 0 || screen_height() <= 0) {
		return;
	}
	draw_textured_quad(bitmap, 0, 0, bitmap->width, bitmap->height, x, y);
}

/** Draw a horizontally flipped bitmap. */
void draw_sprite_h_flip(Bitmap* bitmap, float x, float y) {
	if (bitmap && !is_screen(bitmap) && screen_width() > 0 && screen_height() > 0) draw_textured_quad(bitmap, 0, 0, bitmap->width, bitmap->height, x, y, -1, -1, true, false);
}

/** Draw a vertically flipped bitmap. */
void draw_sprite_v_flip(Bitmap* bitmap, float x, float y) {
	if (bitmap && !is_screen(bitmap) && screen_width() > 0 && screen_height() > 0) draw_textured_quad(bitmap, 0, 0, bitmap->width, bitmap->height, x, y, -1, -1, false, true);
}

namespace detail {

/** Create the display-owned screen bitmap. */
void initialise_screen(int width, int height) {
	destroy_screen();
	screen = new Bitmap;
	screen->width = width;
	screen->height = height;
	screen->kind = BitmapKind::Screen;
}

/** Resize the display-owned screen bitmap. */
void resize_screen(int width, int height) {
	if (!screen) {
		initialise_screen(width, height);
		return;
	}
	screen->width = width;
	screen->height = height;
	screen->pixels.clear();
	screen->ram_dirty = false;
}

/** Destroy the display-owned screen bitmap. */
void destroy_screen() {
	delete screen;
	screen = nullptr;
}

} // namespace detail

} // namespace simlib

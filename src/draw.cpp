#include "draw.h"

#include "display.h"
#include "error.h"

#include <SDL2/SDL_image.h>
#include <SDL2/SDL_opengl.h>
#include <png.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <cstdio>

namespace simlib::draw {
Bitmap* screen = nullptr;

namespace {

constexpr std::size_t bytes_per_pixel = 4;

bool is_valid(const Bitmap* bitmap) {
	return bitmap && bitmap->width > 0 && bitmap->height > 0;
}

bool is_screen(const Bitmap* bitmap) {
	return bitmap && bitmap->kind == BitmapKind::Screen;
}

std::size_t pixel_offset(const Bitmap& bitmap, int x, int y) {
	return (static_cast<std::size_t>(y) * bitmap.width + x) * bytes_per_pixel;
}

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

void set_projection() {
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(
		0.0,
		static_cast<double>(display::screen_width()),
		static_cast<double>(display::screen_height()),
		0.0,
		-1.0,
		1.0
	);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
}

void restore_projection() {
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

void draw_textured_quad(Bitmap* bitmap, int sourceX, int sourceY, int width, int height, int x, int y, int destinationWidth = -1, int destinationHeight = -1, bool flipHorizontal = false, bool flipVertical = false) {
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
	const float right = static_cast<float>(x + (destinationWidth < 0 ? width : destinationWidth));
	const float bottom = static_cast<float>(y + (destinationHeight < 0 ? height : destinationHeight));
	const float textureLeft = flipHorizontal ? rightTexture : leftTexture;
	const float textureRight = flipHorizontal ? leftTexture : rightTexture;
	const float textureTop = flipVertical ? bottomTexture : topTexture;
	const float textureBottom = flipVertical ? topTexture : bottomTexture;
	glBegin(GL_QUADS);
	glTexCoord2f(textureLeft, textureTop); glVertex2f(static_cast<float>(x), static_cast<float>(y));
	glTexCoord2f(textureRight, textureTop); glVertex2f(right, static_cast<float>(y));
	glTexCoord2f(textureRight, textureBottom); glVertex2f(right, bottom);
	glTexCoord2f(textureLeft, textureBottom); glVertex2f(static_cast<float>(x), bottom);
	glEnd();

	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	restore_projection();
}

constexpr float pi = 3.14159265358979323846f;

void begin_screen_plain(Colour colour) {
	set_projection();
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4ub(colour.red, colour.green, colour.blue, colour.alpha);
}

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

void end_screen_shape(bool textured) {
	glDisable(GL_BLEND);
	if (textured) {
		glDisable(GL_TEXTURE_2D);
	}
	restore_projection();
}

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

void draw_screen_ellipse(int x, int y, int radiusX, int radiusY, bool filled, Bitmap* texture, Colour colour) {
	const int segments = std::max(16, std::min(256, std::max(radiusX, radiusY) * 2));
	const bool textured = texture != nullptr;
	if (textured ? !begin_screen_texture(texture) : (begin_screen_plain(colour), false)) {
		return;
	}
	glBegin(filled ? GL_TRIANGLE_FAN : GL_LINE_LOOP);
	if (filled) {
		if (textured) {
			glTexCoord2f(0.5f, 0.5f);
		}
		glVertex2i(x, y);
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

void draw_screen_triangle(int x1, int y1, int x2, int y2, int x3, int y3, bool filled, Bitmap* texture, Colour colour) {
	const bool textured = texture != nullptr;
	if (textured ? !begin_screen_texture(texture) : (begin_screen_plain(colour), false)) {
		return;
	}
	glBegin(filled ? GL_TRIANGLES : GL_LINE_LOOP);
	if (textured) glTexCoord2f(0.0f, 0.0f);
	glVertex2i(x1, y1);
	if (textured) glTexCoord2f(1.0f, 0.0f);
	glVertex2i(x2, y2);
	if (textured) glTexCoord2f(0.5f, 1.0f);
	glVertex2i(x3, y3);
	glEnd();
	end_screen_shape(textured);
}

void draw_screen_rect(int left, int top, int right, int bottom, bool filled, Bitmap* texture, Colour colour) {
	const bool textured = texture != nullptr;
	if (textured ? !begin_screen_texture(texture) : (begin_screen_plain(colour), false)) {
		return;
	}
	glBegin(filled ? GL_QUADS : GL_LINE_LOOP);
	if (textured) glTexCoord2f(0.0f, 0.0f);
	glVertex2i(left, top);
	if (textured) glTexCoord2f(1.0f, 0.0f);
	glVertex2i(right, top);
	if (textured) glTexCoord2f(1.0f, 1.0f);
	glVertex2i(right, bottom);
	if (textured) glTexCoord2f(0.0f, 1.0f);
	glVertex2i(left, bottom);
	glEnd();
	end_screen_shape(textured);
}

} // namespace

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

Bitmap* load_bitmap(const std::string& path) {
	SDL_Surface* loaded = IMG_Load(path.c_str());
	if (!loaded) {
		simlib::detail::set_error(IMG_GetError());
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

bool acquire_bitmap(Bitmap* bitmap) {
	if (is_screen(bitmap)) {
		return download_bitmap(bitmap);
	}
	return ensure_ram_pixels(bitmap);
}

bool release_bitmap(Bitmap* bitmap) {
	if (is_screen(bitmap)) {
		return false;
	}
	return upload_bitmap(bitmap);
}

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

Colour getpixel(Bitmap* bitmap, int x, int y) {
	if (!ensure_ram_pixels(bitmap) || x < 0 || x >= bitmap->width || y < 0 || y >= bitmap->height) {
		return {};
	}
	const std::size_t offset = pixel_offset(*bitmap, x, y);
	return {bitmap->pixels[offset], bitmap->pixels[offset + 1], bitmap->pixels[offset + 2], bitmap->pixels[offset + 3]};
}

void circle(Bitmap* bitmap, int x, int y, int radius, Colour colour) {
	if (!bitmap || radius < 0) return;
	if (is_screen(bitmap)) {
		draw_screen_ellipse(x, y, radius, radius, false, nullptr, colour);
		return;
	}
	for (int degrees = 0; degrees < 360; ++degrees) {
		const float angle = degrees * pi / 180.0f;
		putpixel(bitmap, x + static_cast<int>(std::lround(radius * std::cos(angle))), y + static_cast<int>(std::lround(radius * std::sin(angle))), colour);
	}
}

void circlefill(Bitmap* bitmap, int x, int y, int radius, Colour colour) {
	if (!bitmap || radius < 0) return;
	if (is_screen(bitmap)) {
		draw_screen_ellipse(x, y, radius, radius, true, nullptr, colour);
		return;
	}
	for (int offsetY = -radius; offsetY <= radius; ++offsetY) {
		const int halfWidth = static_cast<int>(std::sqrt(radius * radius - offsetY * offsetY));
		for (int offsetX = -halfWidth; offsetX <= halfWidth; ++offsetX) putpixel(bitmap, x + offsetX, y + offsetY, colour);
	}
}

void rect(Bitmap* bitmap, int left, int top, int right, int bottom, Colour colour) {
	if (is_screen(bitmap)) {
		draw_screen_rect(left, top, right, bottom, false, nullptr, colour);
		return;
	}
	draw_line(bitmap, left, top, right, top, colour);
	draw_line(bitmap, right, top, right, bottom, colour);
	draw_line(bitmap, right, bottom, left, bottom, colour);
	draw_line(bitmap, left, bottom, left, top, colour);
}

void rectfill(Bitmap* bitmap, int left, int top, int right, int bottom, Colour colour) {
	if (is_screen(bitmap)) {
		left = std::clamp(left, 0, bitmap->width);
		right = std::clamp(right, 0, bitmap->width);
		top = std::clamp(top, 0, bitmap->height);
		bottom = std::clamp(bottom, 0, bitmap->height);
		set_projection();
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4ub(colour.red, colour.green, colour.blue, colour.alpha);
		glBegin(GL_QUADS);
		glVertex2i(left, top);
		glVertex2i(right, top);
		glVertex2i(right, bottom);
		glVertex2i(left, bottom);
		glEnd();
		glDisable(GL_BLEND);
		restore_projection();
		return;
	}
	if (!ensure_ram_pixels(bitmap)) {
		return;
	}
	left = std::clamp(left, 0, bitmap->width);
	right = std::clamp(right, 0, bitmap->width);
	top = std::clamp(top, 0, bitmap->height);
	bottom = std::clamp(bottom, 0, bitmap->height);
	for (int y = top; y < bottom; ++y) {
		for (int x = left; x < right; ++x) {
			putpixel(bitmap, x, y, colour);
		}
	}
}

void ellipse(Bitmap* bitmap, int x, int y, int radiusX, int radiusY, Colour colour) {
	if (!bitmap || radiusX < 0 || radiusY < 0) return;
	if (is_screen(bitmap)) {
		draw_screen_ellipse(x, y, radiusX, radiusY, false, nullptr, colour);
		return;
	}
	for (int degrees = 0; degrees < 360; ++degrees) {
		const float angle = degrees * pi / 180.0f;
		putpixel(bitmap, x + static_cast<int>(std::lround(radiusX * std::cos(angle))), y + static_cast<int>(std::lround(radiusY * std::sin(angle))), colour);
	}
}

void ellipsefill(Bitmap* bitmap, int x, int y, int radiusX, int radiusY, Colour colour) {
	if (!bitmap || radiusX < 0 || radiusY < 0) return;
	if (is_screen(bitmap)) {
		draw_screen_ellipse(x, y, radiusX, radiusY, true, nullptr, colour);
		return;
	}
	for (int offsetY = -radiusY; offsetY <= radiusY; ++offsetY) {
		const float ratio = radiusY == 0 ? 0.0f : static_cast<float>(offsetY) / radiusY;
		const int halfWidth = static_cast<int>(std::sqrt(std::max(0.0f, 1.0f - ratio * ratio)) * radiusX);
		for (int offsetX = -halfWidth; offsetX <= halfWidth; ++offsetX) putpixel(bitmap, x + offsetX, y + offsetY, colour);
	}
}

void triangle(Bitmap* bitmap, int x1, int y1, int x2, int y2, int x3, int y3, Colour colour) {
	if (is_screen(bitmap)) {
		draw_screen_triangle(x1, y1, x2, y2, x3, y3, false, nullptr, colour);
		return;
	}
	draw_line(bitmap, x1, y1, x2, y2, colour);
	draw_line(bitmap, x2, y2, x3, y3, colour);
	draw_line(bitmap, x3, y3, x1, y1, colour);
}

void trianglefill(Bitmap* bitmap, int x1, int y1, int x2, int y2, int x3, int y3, Colour colour) {
	if (is_screen(bitmap)) {
		draw_screen_triangle(x1, y1, x2, y2, x3, y3, true, nullptr, colour);
		return;
	}
	const int minimumX = std::min({x1, x2, x3});
	const int maximumX = std::max({x1, x2, x3});
	const int minimumY = std::min({y1, y2, y3});
	const int maximumY = std::max({y1, y2, y3});
	const int area = (x2 - x1) * (y3 - y1) - (y2 - y1) * (x3 - x1);
	if (area == 0) return;
	for (int y = minimumY; y <= maximumY; ++y) for (int x = minimumX; x <= maximumX; ++x) {
		const int a = (x2 - x1) * (y - y1) - (y2 - y1) * (x - x1);
		const int b = (x3 - x2) * (y - y2) - (y3 - y2) * (x - x2);
		const int c = (x1 - x3) * (y - y3) - (y1 - y3) * (x - x3);
		if ((a >= 0 && b >= 0 && c >= 0) || (a <= 0 && b <= 0 && c <= 0)) putpixel(bitmap, x, y, colour);
	}
}

void line(Bitmap* bitmap, int x1, int y1, int x2, int y2, Colour colour) {
	if (bitmap) {
		draw_line(bitmap, x1, y1, x2, y2, colour);
	}
}

void circle(Bitmap* bitmap, int x, int y, int radius, Bitmap* texture) { if (is_screen(bitmap) && radius >= 0) draw_screen_ellipse(x, y, radius, radius, false, texture, {}); }
void circlefill(Bitmap* bitmap, int x, int y, int radius, Bitmap* texture) { if (is_screen(bitmap) && radius >= 0) draw_screen_ellipse(x, y, radius, radius, true, texture, {}); }
void rect(Bitmap* bitmap, int left, int top, int right, int bottom, Bitmap* texture) { if (is_screen(bitmap)) draw_screen_rect(left, top, right, bottom, false, texture, {}); }
void rectfill(Bitmap* bitmap, int left, int top, int right, int bottom, Bitmap* texture) { if (is_screen(bitmap)) draw_screen_rect(left, top, right, bottom, true, texture, {}); }
void ellipse(Bitmap* bitmap, int x, int y, int radiusX, int radiusY, Bitmap* texture) { if (is_screen(bitmap) && radiusX >= 0 && radiusY >= 0) draw_screen_ellipse(x, y, radiusX, radiusY, false, texture, {}); }
void ellipsefill(Bitmap* bitmap, int x, int y, int radiusX, int radiusY, Bitmap* texture) { if (is_screen(bitmap) && radiusX >= 0 && radiusY >= 0) draw_screen_ellipse(x, y, radiusX, radiusY, true, texture, {}); }
void triangle(Bitmap* bitmap, int x1, int y1, int x2, int y2, int x3, int y3, Bitmap* texture) { if (is_screen(bitmap)) draw_screen_triangle(x1, y1, x2, y2, x3, y3, false, texture, {}); }
void trianglefill(Bitmap* bitmap, int x1, int y1, int x2, int y2, int x3, int y3, Bitmap* texture) { if (is_screen(bitmap)) draw_screen_triangle(x1, y1, x2, y2, x3, y3, true, texture, {}); }

void blit(Bitmap* source, Bitmap* destination, int sourceX, int sourceY, int destinationX, int destinationY, int width, int height) {
	if (is_screen(destination)) {
		if (!source || is_screen(source)) {
			return;
		}
		const int clippedSourceX = std::max(sourceX, 0);
		const int clippedSourceY = std::max(sourceY, 0);
		const int clippedWidth = std::min(width - (clippedSourceX - sourceX), source->width - clippedSourceX);
		const int clippedHeight = std::min(height - (clippedSourceY - sourceY), source->height - clippedSourceY);
		if (clippedWidth > 0 && clippedHeight > 0) {
			draw_textured_quad(source, clippedSourceX, clippedSourceY, clippedWidth, clippedHeight, destinationX + (clippedSourceX - sourceX), destinationY + (clippedSourceY - sourceY));
		}
		return;
	}
	if (!ensure_ram_pixels(source) || !ensure_ram_pixels(destination) || width <= 0 || height <= 0) {
		return;
	}
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const int fromX = sourceX + x;
			const int fromY = sourceY + y;
			const int toX = destinationX + x;
			const int toY = destinationY + y;
			if (fromX >= 0 && fromX < source->width && fromY >= 0 && fromY < source->height && toX >= 0 && toX < destination->width && toY >= 0 && toY < destination->height) {
				const std::size_t sourceOffset = pixel_offset(*source, fromX, fromY);
				const std::size_t destinationOffset = pixel_offset(*destination, toX, toY);
				std::memcpy(destination->pixels.data() + destinationOffset, source->pixels.data() + sourceOffset, bytes_per_pixel);
			}
		}
	}
	destination->ram_dirty = true;
}

void masked_blit(Bitmap* source, Bitmap* destination, int sourceX, int sourceY, int destinationX, int destinationY, int width, int height) {
	if (!source || !destination || width <= 0 || height <= 0 || !ensure_ram_pixels(source)) {
		return;
	}
	if (is_screen(destination)) {
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				const int sourcePixelX = sourceX + x;
				const int sourcePixelY = sourceY + y;
				if (sourcePixelX < 0 || sourcePixelX >= source->width || sourcePixelY < 0 || sourcePixelY >= source->height) continue;
				const std::size_t offset = pixel_offset(*source, sourcePixelX, sourcePixelY);
				if (source->pixels[offset + 3] != 0) putpixel(destination, destinationX + x, destinationY + y, {source->pixels[offset], source->pixels[offset + 1], source->pixels[offset + 2], source->pixels[offset + 3]});
			}
		}
		return;
	}
	if (!ensure_ram_pixels(destination)) return;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const int sourcePixelX = sourceX + x;
			const int sourcePixelY = sourceY + y;
			const int destinationPixelX = destinationX + x;
			const int destinationPixelY = destinationY + y;
			if (sourcePixelX < 0 || sourcePixelX >= source->width || sourcePixelY < 0 || sourcePixelY >= source->height || destinationPixelX < 0 || destinationPixelX >= destination->width || destinationPixelY < 0 || destinationPixelY >= destination->height) continue;
			const std::size_t sourceOffset = pixel_offset(*source, sourcePixelX, sourcePixelY);
			if (source->pixels[sourceOffset + 3] != 0) std::memcpy(destination->pixels.data() + pixel_offset(*destination, destinationPixelX, destinationPixelY), source->pixels.data() + sourceOffset, bytes_per_pixel);
		}
	}
	destination->ram_dirty = true;
}

void stretch_blit(Bitmap* source, Bitmap* destination, int sourceX, int sourceY, int sourceWidth, int sourceHeight, int destinationX, int destinationY, int destinationWidth, int destinationHeight) {
	if (!source || !destination || sourceWidth <= 0 || sourceHeight <= 0 || destinationWidth <= 0 || destinationHeight <= 0) return;
	if (is_screen(destination)) {
		draw_textured_quad(source, sourceX, sourceY, sourceWidth, sourceHeight, destinationX, destinationY, destinationWidth, destinationHeight);
		return;
	}
	if (!ensure_ram_pixels(source) || !ensure_ram_pixels(destination)) return;
	for (int y = 0; y < destinationHeight; ++y) {
		for (int x = 0; x < destinationWidth; ++x) {
			const int sourcePixelX = sourceX + x * sourceWidth / destinationWidth;
			const int sourcePixelY = sourceY + y * sourceHeight / destinationHeight;
			const int destinationPixelX = destinationX + x;
			const int destinationPixelY = destinationY + y;
			if (sourcePixelX < 0 || sourcePixelX >= source->width || sourcePixelY < 0 || sourcePixelY >= source->height || destinationPixelX < 0 || destinationPixelX >= destination->width || destinationPixelY < 0 || destinationPixelY >= destination->height) continue;
			std::memcpy(destination->pixels.data() + pixel_offset(*destination, destinationPixelX, destinationPixelY), source->pixels.data() + pixel_offset(*source, sourcePixelX, sourcePixelY), bytes_per_pixel);
		}
	}
	destination->ram_dirty = true;
}

Bitmap* create_sub_bitmap(Bitmap* parent, int x, int y, int width, int height) {
	if (!parent || width <= 0 || height <= 0 || !ensure_ram_pixels(parent)) return nullptr;
	Bitmap* bitmap = create_bitmap(width, height);
	if (!bitmap) return nullptr;
	blit(parent, bitmap, x, y, 0, 0, width, height);
	return bitmap;
}

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

void draw_sprite(Bitmap* bitmap, int x, int y) {
	if (!bitmap || is_screen(bitmap) || display::screen_width() <= 0 || display::screen_height() <= 0) {
		return;
	}
	draw_textured_quad(bitmap, 0, 0, bitmap->width, bitmap->height, x, y);
}

void draw_sprite_h_flip(Bitmap* bitmap, int x, int y) {
	if (bitmap && !is_screen(bitmap) && display::screen_width() > 0 && display::screen_height() > 0) draw_textured_quad(bitmap, 0, 0, bitmap->width, bitmap->height, x, y, -1, -1, true, false);
}

void draw_sprite_v_flip(Bitmap* bitmap, int x, int y) {
	if (bitmap && !is_screen(bitmap) && display::screen_width() > 0 && display::screen_height() > 0) draw_textured_quad(bitmap, 0, 0, bitmap->width, bitmap->height, x, y, -1, -1, false, true);
}

namespace detail {

void initialise_screen(int width, int height) {
	destroy_screen();
	screen = new Bitmap;
	screen->width = width;
	screen->height = height;
	screen->kind = BitmapKind::Screen;
}

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

void destroy_screen() {
	delete screen;
	screen = nullptr;
}

} // namespace detail

} // namespace simlib::draw

#include "draw_test.h"

#include "draw.h"

namespace {

simlib::draw::Bitmap* checker_texture = nullptr;
simlib::draw::Bitmap* ram_bitmap = nullptr;

void build_checker_texture() {
	checker_texture = simlib::draw::create_bitmap(32, 32);
	if (!checker_texture) {
		return;
	}

	for (int y = 0; y < checker_texture->height; ++y) {
		for (int x = 0; x < checker_texture->width; ++x) {
			const bool light = ((x / 8) + (y / 8)) % 2 == 0;
			simlib::draw::putpixel(
				checker_texture,
				x,
				y,
				light ? simlib::draw::Colour{245, 201, 81} : simlib::draw::Colour{35, 128, 180}
			);
		}
	}
}

void build_ram_bitmap() {
	ram_bitmap = simlib::draw::create_bitmap(130, 110);
	if (!ram_bitmap) {
		return;
	}

	simlib::draw::clear_to_colour(ram_bitmap, {31, 42, 56});
	simlib::draw::rect(ram_bitmap, 2, 2, 127, 107, {235, 235, 235});
	simlib::draw::circlefill(ram_bitmap, 35, 36, 22, {226, 92, 80});
	simlib::draw::ellipsefill(ram_bitmap, 94, 37, 28, 16, {77, 182, 112});
	simlib::draw::trianglefill(ram_bitmap, 20, 94, 65, 56, 110, 94, {104, 125, 219});
}

} // namespace

namespace draw_test {

void initialise() {
	build_checker_texture();
	build_ram_bitmap();
}

void render() {
	using namespace simlib::draw;

	if (!screen) {
		return;
	}

	rect(screen, 24, 24, 164, 108, {235, 235, 235});
	rectfill(screen, 32, 32, 156, 100, {50, 94, 132});
	circle(screen, 214, 66, 38, {244, 203, 93});
	circlefill(screen, 300, 66, 38, {213, 83, 96});
	ellipse(screen, 404, 66, 56, 32, {107, 200, 146});
	ellipsefill(screen, 530, 66, 56, 32, {76, 145, 209});
	triangle(screen, 642, 26, 706, 104, 578, 104, {232, 232, 232});
	trianglefill(screen, 770, 26, 834, 104, 706, 104, {196, 111, 219});

	if (checker_texture) {
		rect(screen, 24, 142, 164, 226, checker_texture);
		rectfill(screen, 188, 142, 328, 226, checker_texture);
		circle(screen, 396, 184, 42, checker_texture);
		circlefill(screen, 500, 184, 42, checker_texture);
		ellipse(screen, 624, 184, 62, 34, checker_texture);
		ellipsefill(screen, 762, 184, 62, 34, checker_texture);
		triangle(screen, 44, 270, 120, 348, 20, 348, checker_texture);
		trianglefill(screen, 188, 270, 264, 348, 164, 348, checker_texture);
	}

	if (ram_bitmap) {
		draw_sprite(ram_bitmap, 344, 264);
	}
}

void shutdown() {
	simlib::draw::destroy_bitmap(ram_bitmap);
	ram_bitmap = nullptr;
	simlib::draw::destroy_bitmap(checker_texture);
	checker_texture = nullptr;
}

} // namespace draw_test

#include "graphics_fx_test.h"

#include "draw.h"
#include "graphics_fx.h"

#include <algorithm>

namespace
{

	simlib::Bitmap *scene = nullptr;
	simlib::Bloom bloom;

	/** Build the shader-effect test scene. */
	void draw_scene()
	{
		using namespace simlib;

		clear_to_colour(scene, {8, 10, 20});

		// The white and saturated shapes give bloom highlights a clear silhouette.
		for (int index = 0; index < 7; ++index)
		{
			const int x = 55 + index * 112;
			circlefill(scene, x, 110, 38, {255, 232, 138});
			circle(scene, x, 110, 48, {255, 255, 255});
		}

		for (int index = 0; index < 5; ++index)
		{
			const int x = 100 + index * 150;
			const int y = 270 + (index % 2) * 24;
			trianglefill(scene, x, y - 58, x + 60, y + 54, x - 60, y + 54, {92, 212, 255});
			circlefill(scene, x, y, 12, {255, 255, 255});
		}

		rectfill(scene, 35, 450, 765, 485, {218, 56, 108});
		rect(scene, 35, 450, 765, 485, {255, 255, 255});
		for (int x = 70; x < 750; x += 54)
		{
			circlefill(scene, x, 468, 7, {255, 250, 180});
		}
	}

} // namespace

namespace graphics_fx_test
{

	void initialise()
	{
		scene = simlib::create_video_bitmap(800, 600);
		if (!scene)
		{
			return;
		}

		bloom.set_threshold(0.55f);
		bloom.set_intensity(1.7f);
		bloom.set_radius(2.0f);
		if (!bloom.initialise())
		{
			return;
		}
		draw_scene();
	}

	void render()
	{
		if (!scene)
		{
			return;
		}

		if (bloom.is_valid())
		{
			bloom.apply(scene);
		}
		else
		{
			simlib::draw_sprite(scene, 0, 0);
		}
	}

	void shutdown()
	{
		simlib::destroy_bitmap(scene);
		scene = nullptr;
		bloom.shutdown();
	}

} // namespace graphics_fx_test
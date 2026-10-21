#include "camera2d.h"
#include "collision2d.h"
#include "tween.h"
#include "pixel_scale.h"
#include "animation2d.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace
{
    bool near(float a, float b, float eps = 0.001f) { return std::abs(a - b) < eps; }
} // namespace

int main()
{
    // Camera: transforms round-trip with zoom and rotation.
    {
        sl::Camera2D camera;
        camera.viewport_width = 800;
        camera.viewport_height = 600;
        camera.x = 100.0f;
        camera.y = 50.0f;
        float sx = 0.0f, sy = 0.0f;
        camera.world_to_screen(100.0f, 50.0f, sx, sy);
        assert(near(sx, 400.0f) && near(sy, 300.0f));

        camera.zoom = 2.0f;
        camera.rotation = 30.0f;
        float wx = 0.0f, wy = 0.0f;
        camera.world_to_screen(130.0f, 90.0f, sx, sy);
        camera.screen_to_world(sx, sy, wx, wy);
        assert(near(wx, 130.0f) && near(wy, 90.0f));

        camera.rotation = 90.0f;
        camera.zoom = 1.0f;
        // A clockwise camera rotation makes a point to the right of the camera appear above it... check by round trip only.
        camera.world_to_screen(camera.x + 10.0f, camera.y, sx, sy);
        camera.screen_to_world(sx, sy, wx, wy);
        assert(near(wx, camera.x + 10.0f) && near(wy, camera.y));
    }

    // Camera: bounds keep the view inside the world and centre it when the world is smaller than the view.
    {
        sl::Camera2D camera;
        camera.viewport_width = 200;
        camera.viewport_height = 100;
        camera.set_bounds(0.0f, 0.0f, 1000.0f, 500.0f);
        camera.follow(-500.0f, -500.0f, 0.016f);
        assert(near(camera.x, 100.0f) && near(camera.y, 50.0f));
        camera.follow(5000.0f, 5000.0f, 0.016f);
        assert(near(camera.x, 900.0f) && near(camera.y, 450.0f));
        camera.set_bounds(0.0f, 0.0f, 100.0f, 50.0f);
        assert(near(camera.x, 50.0f) && near(camera.y, 25.0f));
    }

    // Camera: smooth follow approaches the target, and shake fades out.
    {
        sl::Camera2D camera;
        camera.viewport_width = 100;
        camera.viewport_height = 100;
        for (int i = 0; i < 600; ++i)
            camera.follow(100.0f, 0.0f, 1.0f / 60.0f, 5.0f);
        assert(near(camera.x, 100.0f, 0.05f));
        camera.shake(10.0f, 0.5f);
        assert(camera.shaking());
        for (int i = 0; i < 60; ++i)
            camera.update(1.0f / 60.0f);
        assert(!camera.shaking());
        float sx = 0.0f, sy = 0.0f;
        camera.world_to_screen(camera.x, camera.y, sx, sy);
        assert(near(sx, 50.0f) && near(sy, 50.0f));
    }

    // Collision: rectangles, circles and mixed.
    {
        const sl::Rect2D a{0, 0, 10, 10}, b{8, 3, 10, 10}, far_rect{50, 50, 5, 5};
        assert(sl::rects_overlap(a, b) && !sl::rects_overlap(a, far_rect));
        const sl::Collision2D hit = sl::collide_rects(a, b);
        assert(hit.hit && near(hit.depth, 2.0f) && near(hit.normal.x, -1.0f));
        assert(sl::circles_overlap({0, 0, 5}, {8, 0, 5}) && !sl::circles_overlap({0, 0, 5}, {20, 0, 5}));
        const sl::Collision2D circles = sl::collide_circles({0, 0, 5}, {8, 0, 5});
        assert(circles.hit && near(circles.depth, 2.0f) && near(circles.normal.x, -1.0f));
        const sl::Collision2D circle_rect = sl::collide_circle_rect({12, 5, 3}, a);
        assert(circle_rect.hit && near(circle_rect.depth, 1.0f) && near(circle_rect.normal.x, 1.0f));
        assert(!sl::collide_circle_rect({20, 5, 3}, a).hit);
        assert(sl::collide_circle_rect({5, 5, 1}, a).hit);
    }

    // Collision: SAT on convex polygons.
    {
        const std::vector<sl::Vec2D> square_a = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
        const std::vector<sl::Vec2D> square_b = {{8, 2}, {18, 2}, {18, 12}, {8, 12}};
        const std::vector<sl::Vec2D> apart = {{30, 30}, {40, 30}, {40, 40}};
        const sl::Collision2D hit = sl::collide_polygons(square_a, square_b);
        assert(hit.hit && near(hit.depth, 2.0f) && near(hit.normal.x, -1.0f));
        assert(!sl::collide_polygons(square_a, apart).hit);
    }

    // Collision: ray casts.
    {
        const sl::Rect2D box{10, -5, 10, 10};
        const sl::RayHit2D hit = sl::raycast_rect(0, 0, 100, 0, box);
        assert(hit.hit && near(hit.t, 0.1f) && near(hit.point.x, 10.0f) && near(hit.normal.x, -1.0f));
        assert(!sl::raycast_rect(0, 20, 100, 0, box).hit);
        assert(!sl::raycast_rect(0, 0, 5, 0, box).hit);
        const sl::RayHit2D reverse = sl::raycast_rect(100, 0, -100, 0, box);
        assert(reverse.hit && near(reverse.point.x, 20.0f) && near(reverse.normal.x, 1.0f));
        const sl::RayHit2D circle = sl::raycast_circle(0, 0, 100, 0, {50, 0, 10});
        assert(circle.hit && near(circle.point.x, 40.0f) && near(circle.normal.x, -1.0f));
        assert(!sl::raycast_circle(0, 0, 100, 0, {50, 30, 10}).hit);
    }

    // Collision: spatial hash returns only nearby ids, deduplicated.
    {
        sl::SpatialHash2D hash(32.0f);
        hash.insert(1, {0, 0, 10, 10});
        hash.insert(2, {100, 100, 10, 10});
        hash.insert(3, {20, 20, 40, 40});
        const std::vector<int> near_origin = hash.query({0, 0, 30, 30});
        assert(near_origin.size() == 2 && near_origin[0] == 1 && near_origin[1] == 3);
        const std::vector<int> negative = hash.query({-500, -500, 10, 10});
        assert(negative.empty());
        hash.clear();
        assert(hash.query({0, 0, 1000, 1000}).empty());
    }

    // Tween and easing.
    {
        for (int type = 0; type <= static_cast<int>(sl::Ease::bounce_out); ++type)
        {
            const sl::Ease ease = static_cast<sl::Ease>(type);
            assert(near(sl::ease(ease, 0.0f), 0.0f) && near(sl::ease(ease, 1.0f), 1.0f));
        }
        sl::Tween tween;
        tween.from = 10.0f;
        tween.to = 20.0f;
        tween.duration = 1.0f;
        tween.delay = 0.5f;
        assert(near(tween.value(), 10.0f));
        assert(!tween.update(0.5f) && near(tween.value(), 10.0f));
        assert(!tween.update(0.5f) && near(tween.value(), 15.0f));
        assert(tween.update(0.6f) && near(tween.value(), 20.0f));

        sl::Tween ping;
        ping.duration = 1.0f;
        ping.ping_pong = true;
        ping.update(1.5f);
        assert(near(ping.value(), 0.5f) && !ping.finished());
    }

    // Scheduler: one-shot and repeating callbacks, cancellation.
    {
        sl::Scheduler scheduler;
        int once = 0, repeated = 0;
        scheduler.after(1.0f, [&] { ++once; });
        const int id = scheduler.every(0.5f, [&] { ++repeated; });
        scheduler.update(0.6f);
        assert(once == 0 && repeated == 1);
        scheduler.update(0.6f);
        assert(once == 1 && repeated == 2);
        scheduler.cancel(id);
        scheduler.update(2.0f);
        assert(repeated == 2 && scheduler.pending() == 0);
    }

    // Pixel scaling: integer, aspect-fit and stretch placement and coordinate conversion.
    {
        const sl::ScaleFit integer = sl::compute_scale_fit(320, 180, 1000, 700, sl::ScaleMode::integer);
        assert(integer.width == 960 && integer.height == 540 && integer.x == 20 && integer.y == 80);
        assert(near(integer.scale_x, 3.0f) && near(integer.scale_y, 3.0f));
        float gx = 0.0f, gy = 0.0f;
        integer.window_to_game(20.0f + 30.0f, 80.0f + 60.0f, gx, gy);
        assert(near(gx, 10.0f) && near(gy, 20.0f));
        assert(integer.contains(500.0f, 300.0f) && !integer.contains(5.0f, 300.0f));
        // A window smaller than the image still gets scale 1 rather than 0.
        assert(sl::compute_scale_fit(320, 180, 200, 100, sl::ScaleMode::integer).scale_x == 1.0f);
        const sl::ScaleFit fit = sl::compute_scale_fit(320, 180, 1000, 700, sl::ScaleMode::aspect_fit);
        assert(fit.width == 1000 && fit.height == 563 && fit.x == 0);
        const sl::ScaleFit stretch = sl::compute_scale_fit(320, 180, 1000, 700, sl::ScaleMode::stretch);
        assert(stretch.width == 1000 && stretch.height == 700 && stretch.x == 0 && stretch.y == 0);
        assert(sl::compute_scale_fit(0, 180, 1000, 700).width == 0);
    }

    // Animation: JSON import (frame tags, durations, hash form) and playback.
    {
        const char *json = R"({"frames":[
            {"filename":"a","frame":{"x":0,"y":0,"w":16,"h":16},"duration":100},
            {"filename":"b","frame":{"x":16,"y":0,"w":16,"h":16},"duration":200},
            {"filename":"c","frame":{"x":32,"y":0,"w":16,"h":16},"duration":100},
            {"filename":"d","frame":{"x":48,"y":0,"w":16,"h":16},"duration":100}],
            "meta":{"frameTags":[{"name":"walk","from":0,"to":2,"direction":"forward"},
                                  {"name":"bounce","from":0,"to":2,"direction":"pingpong"},
                                  {"name":"back","from":1,"to":3,"direction":"reverse"}]}})";
        sl::SpriteSheet sheet;
        std::string error;
        assert(sl::parse_sprite_sheet_json(json, sheet, &error));
        assert(sheet.frames.size() == 4 && sheet.frames[1].x == 16 && near(sheet.frames[1].duration, 0.2f));
        assert(sheet.clips.size() == 3 && sheet.find_clip("bounce")->direction == sl::PlayDirection::ping_pong);
        assert(!sl::parse_sprite_sheet_json("{}", sheet, &error) && !error.empty());

        assert(sl::parse_sprite_sheet_json(json, sheet));
        sl::Animator animator(&sheet);
        assert(!animator.play("missing"));
        std::vector<std::string> events;
        animator.on_event = [&](const std::string &name) { events.push_back(name); };
        assert(sheet.add_event("walk", 2, "footstep"));
        assert(animator.play("walk") && animator.frame_index() == 0);
        animator.update(0.11f);
        assert(animator.frame_index() == 1);
        animator.update(0.2f);
        assert(animator.frame_index() == 2 && events.size() == 1 && events[0] == "footstep");
        animator.update(0.1f);
        assert(animator.frame_index() == 0);

        // Ping-pong visits 0,1,2,1,0.
        animator.play("bounce");
        std::vector<int> visited;
        for (int i = 0; i < 5; ++i)
        {
            visited.push_back(animator.frame_index());
            animator.update(animator.frame()->duration + 0.001f);
        }
        assert((visited == std::vector<int>{0, 1, 2, 1, 0}));

        // Reverse starts at the last frame; a non-looping clip finishes and holds.
        assert(sheet.add_clip("once", 0, 1, false));
        bool finished = false;
        animator.on_finished = [&](const std::string &) { finished = true; };
        animator.play("back");
        assert(animator.frame_index() == 3);
        animator.play("once");
        animator.update(0.11f);
        animator.update(0.21f);
        assert(finished && animator.finished() && animator.frame_index() == 1);
        animator.update(1.0f);
        assert(animator.frame_index() == 1);
    }

    std::cout << "camera_collision_test passed\n";
    return 0;
}

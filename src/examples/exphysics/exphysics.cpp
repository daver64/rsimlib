#include "sl.h"

#include <algorithm>
#include <cmath>

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        return -1;
    }

    sl::PhysicsWorld *world = sl::create_physics_world({0.0f, 980.0f});
    sl::PhysicsBody *floor = sl::create_physics_body(world, sl::BodyType::static_body, {400.0f, 560.0f});
    sl::PhysicsBody *box = sl::create_physics_body(world, sl::BodyType::dynamic_body, {400.0f, 120.0f});
    sl::PhysicsBody *ball = sl::create_physics_body(world, sl::BodyType::dynamic_body, {470.0f, 60.0f});
    sl::PhysicsBody *triangle = sl::create_physics_body(world, sl::BodyType::dynamic_body, {330.0f, 80.0f});
    const std::vector<sl::Vec2> triangle_vertices = {{-34.0f, 28.0f}, {34.0f, 28.0f}, {0.0f, -36.0f}};
    if (!world || !floor || !box || !ball || !triangle ||
        !sl::add_box_fixture(floor, 700.0f, 32.0f) ||
        !sl::add_box_fixture(box, 64.0f, 64.0f, 1.0f, 0.4f, 0.35f) ||
        !sl::add_circle_fixture(ball, 24.0f, 1.0f, 0.3f, 0.6f) ||
        !sl::add_polygon_fixture(triangle, triangle_vertices, 1.0f, 0.3f, 0.35f))
    {
        sl::destroy_physics_body(triangle);
        sl::destroy_physics_body(ball);
        sl::destroy_physics_body(box);
        sl::destroy_physics_body(floor);
        sl::destroy_physics_world(world);
        sl::shutdown();
        return -1;
    }

    sl::set_fps(60);
    bool running = true;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
            {
                running = false;
            }
            sl::display_handle_event(event);
        }

        sl::step_physics_world(world, std::min(0.05f, static_cast<float>(sl::get_frame_time()) / 1000.0f));
        const std::vector<sl::PhysicsContact> contacts = sl::poll_physics_contacts(world);
        const sl::Vec2 box_position = sl::physics_body_position(box);
        const sl::Vec2 ball_position = sl::physics_body_position(ball);
        const sl::Vec2 triangle_position = sl::physics_body_position(triangle);
        const float triangle_angle = sl::physics_body_angle(triangle);
        const float cosine = std::cos(triangle_angle);
        const float sine = std::sin(triangle_angle);
        auto transformed_triangle_point = [&](sl::Vec2 point) {
            return sl::Vec2{
                triangle_position.x + point.x * cosine - point.y * sine,
                triangle_position.y + point.x * sine + point.y * cosine};
        };
        const sl::Vec2 triangle_a = transformed_triangle_point(triangle_vertices[0]);
        const sl::Vec2 triangle_b = transformed_triangle_point(triangle_vertices[1]);
        const sl::Vec2 triangle_c = transformed_triangle_point(triangle_vertices[2]);

        sl::clear_to_colour(sl::screen, {28, 34, 46});
        sl::gprintf_center(32, {220, 230, 240}, "Box2D physics - press Escape to exit");
        sl::gprintf_center(56, {170, 190, 210}, "Contact events this frame: %d", static_cast<int>(contacts.size()));
        sl::rectfill(sl::screen, 50.0f, 544.0f, 750.0f, 576.0f, {100, 112, 132});
        sl::rectfill(sl::screen, box_position.x - 32.0f, box_position.y - 32.0f,
                     box_position.x + 32.0f, box_position.y + 32.0f, {232, 110, 84});
        sl::circlefill(sl::screen, ball_position.x, ball_position.y, 24.0f, {92, 180, 240});
        sl::trianglefill(sl::screen, triangle_a.x, triangle_a.y, triangle_b.x, triangle_b.y,
                 triangle_c.x, triangle_c.y, {210, 150, 72});
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::destroy_physics_body(ball);
    sl::destroy_physics_body(box);
    sl::destroy_physics_body(floor);
    sl::destroy_physics_body(triangle);
    sl::destroy_physics_world(world);
    sl::shutdown();
    return 0;
}

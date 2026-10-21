#include "sl.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

int main(int argc, char *argv[])
{
    sl::set_text_panel_style({true});
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 960, 600))
    {
        return -1;
    }

    sl::PhysicsWorld *world = sl::create_physics_world({0.0f, 980.0f});
    sl::PhysicsBody *ground = sl::create_physics_body(world, sl::BodyType::static_body, {480.0f, 580.0f});
    sl::add_box_fixture(ground, 960.0f, 40.0f);

    // Rope bridge: planks pinned end to end between two posts.
    const int planks = 8;
    const float plank_width = 44.0f, bridge_y = 250.0f, bridge_x = 60.0f;
    sl::PhysicsBody *left_post = sl::create_physics_body(world, sl::BodyType::static_body, {bridge_x - 10.0f, bridge_y});
    sl::add_box_fixture(left_post, 20.0f, 30.0f);
    sl::PhysicsBody *previous = left_post;
    for (int i = 0; i < planks; ++i)
    {
        const float x = bridge_x + plank_width * (i + 0.5f);
        sl::PhysicsBody *plank = sl::create_physics_body(world, sl::BodyType::dynamic_body, {x, bridge_y});
        sl::add_box_fixture(plank, plank_width - 2.0f, 10.0f, 1.0f, 0.5f);
        sl::create_revolute_joint(world, previous, plank, {bridge_x + plank_width * i, bridge_y});
        previous = plank;
    }
    sl::PhysicsBody *right_post = sl::create_physics_body(world, sl::BodyType::static_body,
                                                          {bridge_x + plank_width * planks + 10.0f, bridge_y});
    sl::add_box_fixture(right_post, 20.0f, 30.0f);
    sl::create_revolute_joint(world, previous, right_post, {bridge_x + plank_width * planks, bridge_y});

    // Springy pendulum hanging from a static anchor.
    sl::PhysicsBody *hook = sl::create_physics_body(world, sl::BodyType::static_body, {620.0f, 60.0f});
    sl::add_circle_fixture(hook, 6.0f);
    sl::PhysicsBody *bob = sl::create_physics_body(world, sl::BodyType::dynamic_body, {720.0f, 60.0f});
    sl::add_circle_fixture(bob, 22.0f, 1.0f, 0.3f, 0.2f);
    sl::create_distance_joint(world, hook, bob, {620.0f, 60.0f}, {720.0f, 60.0f}, 2.0f, 0.3f);

    // Car: two wheel joints give suspension, and the motor drives the wheels.
    sl::PhysicsBody *chassis = sl::create_physics_body(world, sl::BodyType::dynamic_body, {120.0f, 480.0f});
    sl::add_box_fixture(chassis, 110.0f, 24.0f, 1.0f, 0.3f);
    std::vector<sl::PhysicsJoint *> motors;
    for (const float offset : {-40.0f, 40.0f})
    {
        sl::PhysicsBody *wheel = sl::create_physics_body(world, sl::BodyType::dynamic_body, {120.0f + offset, 505.0f});
        sl::add_circle_fixture(wheel, 18.0f, 1.0f, 1.2f);
        sl::PhysicsJoint *joint = sl::create_wheel_joint(world, chassis, wheel, {120.0f + offset, 505.0f}, {0.0f, 1.0f});
        sl::set_joint_motor(joint, 6.0f, 60000.0f);
        motors.push_back(joint);
    }

    // Boxes to throw around with the mouse.
    for (int i = 0; i < 5; ++i)
    {
        sl::PhysicsBody *box = sl::create_physics_body(world, sl::BodyType::dynamic_body, {520.0f + i * 70.0f, 400.0f - i * 40.0f});
        sl::add_box_fixture(box, 50.0f, 50.0f, 1.0f, 0.4f, 0.2f);
    }

    sl::PhysicsJoint *grab = nullptr;
    bool running = true;
    unsigned frame = 0;
    sl::set_fps(60);
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            sl::display_handle_event(event);
        }

        // Drag any dynamic body under the cursor with a mouse joint.
        const sl::Vec2 mouse{static_cast<float>(sl::mouse_x()), static_cast<float>(sl::mouse_y())};
        const bool held = (sl::mouse_buttons() & SDL_BUTTON_LMASK) != 0;
        if (held && !grab)
        {
            for (sl::PhysicsBody *body : sl::physics_query_point(world, mouse))
            {
                if ((grab = sl::create_mouse_joint(world, body, mouse)))
                    break;
            }
        }
        else if (held && grab)
            sl::set_mouse_joint_target(grab, mouse);
        else if (!held && grab)
        {
            sl::destroy_physics_joint(grab);
            grab = nullptr;
        }

        sl::step_physics_world(world, std::min(0.05f, static_cast<float>(sl::get_frame_time()) / 1000.0f));

        sl::clear_to_colour(sl::screen, {28, 34, 46});
        sl::PhysicsDebugDraw options;
        options.centers_of_mass = true;
        sl::debug_draw_physics(world, options);
        sl::gprintf(10, 10, {255, 240, 170}, "Joints: bridge (revolute), pendulum (spring), car (wheel). Drag with the left mouse button.");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();

        ++frame;
        const char *frames = std::getenv("RSIMLIB_FRAMES");
        if (frames && frame >= static_cast<unsigned>(std::max(1, std::atoi(frames))))
            running = false;
        const char *screenshot = std::getenv("RSIMLIB_SCREENSHOT");
        if (screenshot && frame == 90u && !sl::save_bitmap(sl::screen, screenshot))
            std::fprintf(stderr, "Screenshot save failed: %s\n", sl::last_error().c_str());
    }

    sl::destroy_physics_world(world);
    sl::wait_for_graphics();
    sl::shutdown();
    return 0;
}

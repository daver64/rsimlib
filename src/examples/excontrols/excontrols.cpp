#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

// Control panels in 2D: a light-aircraft instrument panel and a factory process cabinet, drawn flat with
// draw_control_panel() and driven by the mouse. Drag the knobs and sliders, click the toggles and buttons. A small
// simulation behind each panel turns the controls into needle and meter readings, and the lamps glow.
//   Mouse: operate the controls   B: bloom   Esc: exit
namespace
{
    float approach(float value, float target, float rate, float dt)
    {
        return value + (target - value) * (1.0f - std::exp(-rate * dt));
    }
} // namespace

int main(int argc, char **argv)
{
    if (!sl::configure_graphics_backend_from_args(argc, argv))
        return 1;
    constexpr int screen_w = 1120;
    constexpr int screen_h = 600;
    if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, screen_w, screen_h))
    {
        std::fprintf(stderr, "Graphics initialization failed: %s\n", sl::last_error().c_str());
        return 1;
    }
    sl::set_window_title("simlib - Control panels (excontrols)");
    sl::set_fps(60);
    sl::Bitmap *scene = sl::create_render_target(screen_w, screen_h);
    sl::Bloom bloom;
    const bool bloom_ready = bloom.initialise() && scene;
    bloom.set_threshold(0.55f);
    bloom.set_intensity(0.9f);
    bloom.set_radius(1.6f);
    bool use_bloom = bloom_ready;

    sl::ControlPanel cockpit = sl::make_cockpit_panel();
    sl::ControlPanel cabinet = sl::make_cabinet_panel();
    const float cockpit_x = 400.0f, cockpit_y = 70.0f, cockpit_scale = 1.0f;
    const float cabinet_x = 24.0f, cabinet_y = 70.0f, cabinet_scale = 1.1f;

    std::string last_event = "Click or drag something";
    cockpit.on_change = [&](int id, float value)
    {
        last_event = "cockpit  " + cockpit.control(id).label + " = " + std::to_string(value).substr(0, 5);
    };
    cabinet.on_change = [&](int id, float value)
    {
        last_event = "cabinet  " + cabinet.control(id).label + " = " + std::to_string(value).substr(0, 5);
    };

    const int c_speed = cockpit.find("AIRSPEED"), c_alt = cockpit.find("ALTITUDE"), c_rpm = cockpit.find("RPM"), c_fuel = cockpit.find("FUEL"),
              c_temp = cockpit.find("TEMP"), c_thr = cockpit.find("THR"), c_master = cockpit.find("MASTER"), c_pump = cockpit.find("PUMP"),
              c_start = cockpit.find("START"), c_gear = cockpit.find("GEAR"), c_warn = cockpit.find("WARN"), c_locked = cockpit.find("LOCKED"),
              c_ready = cockpit.find("READY");
    const int k_press = cabinet.find("PRESSURE"), k_temp = cabinet.find("TEMP"), k_a = cabinet.find("TANK A"), k_b = cabinet.find("TANK B"),
              k_v1 = cabinet.find("VALVE 1"), k_v2 = cabinet.find("VALVE 2"), k_speed = cabinet.find("SPEED"), k_p1 = cabinet.find("PUMP 1"),
              k_p2 = cabinet.find("PUMP 2"), k_start = cabinet.find("START"), k_stop = cabinet.find("STOP"), k_estop = cabinet.find("E-STOP"),
              k_run = cabinet.find("RUN"), k_warn = cabinet.find("WARN"), k_fault = cabinet.find("FAULT");

    float rpm = 0.0f, altitude = 0.0f, fuel = 0.8f, engine_temp = 0.1f;
    bool engine = false;
    float pressure = 0.0f, process_temp = 20.0f, tank_a = 0.9f, tank_b = 0.1f;
    bool running_process = false;

    bool running = true;
    std::uint32_t last_time = sl::time_ms();
    while (running)
    {
        const std::uint32_t now = sl::time_ms();
        const float dt = std::clamp(static_cast<float>(now - last_time) * 0.001f, 0.001f, 0.1f);
        last_time = now;
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            else if (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::letter_b && bloom_ready)
                use_bloom = !use_bloom;
            sl::display_handle_event(event);
        }

        const float mx = static_cast<float>(sl::mouse_x()), my = static_cast<float>(sl::mouse_y());
        const bool down = sl::mouse_button_down(sl::MouseButton::left);
        cockpit.update(dt, sl::control_pointer_2d(cockpit_x, cockpit_y, cockpit_scale, mx, my, down));
        cabinet.update(dt, sl::control_pointer_2d(cabinet_x, cabinet_y, cabinet_scale, mx, my, down));

        // Aircraft: MASTER and PUMP arm the engine, START spins it up, the throttle sets rpm.
        const bool armed = cockpit.on(c_master) && cockpit.on(c_pump);
        if (armed && cockpit.on(c_start) && fuel > 0.0f)
            engine = true;
        if (!armed || fuel <= 0.0f)
            engine = false;
        const float throttle = cockpit.value(c_thr);
        rpm = approach(rpm, engine ? 700.0f + 2200.0f * throttle : 0.0f, 1.2f, dt);
        const float speed = std::max(0.0f, (rpm - 700.0f) * 0.16f + (cockpit.on(c_gear) ? 0.0f : 20.0f) * (rpm > 900.0f ? 1.0f : 0.0f));
        altitude = std::clamp(altitude + (rpm - 1800.0f) * 0.05f * dt, 0.0f, 10000.0f);
        if (engine)
            fuel = std::max(0.0f, fuel - (0.003f + 0.01f * throttle) * dt);
        engine_temp = approach(engine_temp, engine ? 0.35f + 0.6f * throttle : 0.1f, 0.4f, dt);
        cockpit.set_value(c_rpm, rpm);
        cockpit.set_value(c_speed, speed);
        cockpit.set_value(c_alt, altitude);
        cockpit.set_value(c_fuel, fuel);
        cockpit.set_value(c_temp, engine_temp);
        cockpit.set_on(c_ready, armed);
        cockpit.set_on(c_locked, cockpit.on(c_gear));
        cockpit.set_on(c_warn, fuel < 0.2f || engine_temp > 0.85f);

        // Cabinet: the pumps and START run the process; STOP or the E-STOP end it.
        const bool estop = cabinet.on(k_estop);
        const bool pumps = cabinet.on(k_p1) || cabinet.on(k_p2);
        if (cabinet.on(k_start) && pumps && !estop)
            running_process = true;
        if (cabinet.on(k_stop) || estop || !pumps)
            running_process = false;
        const float flow = running_process ? cabinet.value(k_speed) * (cabinet.on(k_p1) + cabinet.on(k_p2)) * 0.5f : 0.0f;
        const float open = 0.15f + 0.85f * cabinet.value(k_v1) * cabinet.value(k_v2) * 4.0f;
        pressure = approach(pressure, std::min(10.0f, 10.0f * flow * (1.4f - std::min(open, 1.0f))), 1.5f, dt);
        process_temp = approach(process_temp, 20.0f + 130.0f * flow, 0.3f, dt);
        tank_a = std::clamp(tank_a - flow * 0.06f * dt, 0.0f, 1.0f);
        tank_b = std::clamp(tank_b + flow * 0.06f * dt, 0.0f, 1.0f);
        cabinet.set_value(k_press, pressure);
        cabinet.set_value(k_temp, process_temp);
        cabinet.set_value(k_a, tank_a);
        cabinet.set_value(k_b, tank_b);
        cabinet.set_on(k_run, running_process);
        cabinet.set_on(k_warn, pressure > 7.0f);
        cabinet.set_on(k_fault, estop || pressure > 9.5f);

        sl::Bitmap *target = (use_bloom && scene) ? scene : sl::screen;
        if (target != sl::screen)
            sl::begin_render_target(scene);
        sl::clear_to_colour(target, {20, 22, 28});
        sl::draw_control_panel(target, cabinet, cabinet_x, cabinet_y, cabinet_scale);
        sl::draw_control_panel(target, cockpit, cockpit_x, cockpit_y, cockpit_scale);
        if (target != sl::screen)
            sl::end_render_target();
        if (target != sl::screen)
            bloom.apply(scene, 0, 0, screen_w, screen_h);

        sl::gprintf(24, 20, {232, 236, 244}, "excontrols - the same panel model drawn flat in 2D");
        sl::gprintf(24, 44, {150, 160, 180}, "Cabinet: PUMP then START. Cockpit: MASTER, PUMP, START, then raise THR.");
        sl::gprintf(400, 380, {170, 185, 205}, "Last change: %s", last_event.c_str());
        sl::gprintf(400, 404, {150, 160, 180}, "Drag knobs and sliders, click toggles and buttons. GEAR latches, START and STOP are momentary.");
        sl::gprintf(400, 428, {150, 160, 180}, "[B] Bloom %s   [Esc] Quit", use_bloom ? "on" : "off");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    bloom.shutdown();
    sl::shutdown();
    return 0;
}

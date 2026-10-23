#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

// Control panels in 3D: a cockpit and a factory cabinet built from props3d parts. The mouse is turned into a ray and
// mapped onto each panel's plane with ControlPanelView3D::pick(), so knobs, sliders, toggles and buttons work in the
// scene. Lamps and lit meter segments are emissive, bloom through Hdr and light the surfaces around them.
//   1: pilot view   2: cabinet   3: overview   L: room light   Mouse: operate the controls   Esc: exit
namespace
{
    float approach(float value, float target, float rate, float dt)
    {
        return value + (target - value) * (1.0f - std::exp(-rate * dt));
    }

    struct Station
    {
        sl::ControlAssembly assembly;
        std::unique_ptr<sl::ControlPanelView3D> view;
        sl::Mesh *body = nullptr;
        glm::mat4 model{1.0f};
        glm::mat4 panel_world{1.0f};
    };
} // namespace

int main(int argc, char **argv)
{
    sl::set_text_panel_style({true});
    if (!sl::configure_graphics_backend_from_args(argc, argv))
        return 1;
    constexpr int screen_w = 1100;
    constexpr int screen_h = 680;
    if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, screen_w, screen_h))
    {
        std::fprintf(stderr, "Graphics initialization failed: %s\n", sl::last_error().c_str());
        return 1;
    }
    sl::set_window_title("simlib - 3D control panels (excockpit)");
    sl::set_fps(60);
    sl::Surface3D *surface = sl::create_surface_3d(screen_w, screen_h, 4);
    sl::Mesh *floor = sl::create_plane_mesh(30.0f, 30.0f, {70, 72, 78, 255});
    sl::Hdr hdr;
    if (!surface || !floor || !hdr.initialise())
    {
        std::fprintf(stderr, "3D unavailable: %s\n", surface ? hdr.error().c_str() : sl::last_error().c_str());
        sl::shutdown();
        return 1;
    }

    Station stations[2] = {{sl::make_cockpit()}, {sl::make_control_cabinet()}};
    stations[0].model = glm::translate(glm::mat4(1.0f), glm::vec3(-1.8f, 0.0f, 0.0f)) * glm::rotate(glm::mat4(1.0f), 0.3f, glm::vec3(0.0f, 1.0f, 0.0f));
    stations[1].model = glm::translate(glm::mat4(1.0f), glm::vec3(1.5f, 0.0f, 0.2f)) * glm::rotate(glm::mat4(1.0f), -0.3f, glm::vec3(0.0f, 1.0f, 0.0f));
    for (Station &s : stations)
    {
        s.view = std::make_unique<sl::ControlPanelView3D>(s.assembly.panel, s.assembly.unit);
        s.body = sl::create_mesh(s.assembly.body.vertices, s.assembly.body.indices);
        s.panel_world = s.model * s.assembly.panel_pose;
        if (!s.body || !s.view->valid())
        {
            std::fprintf(stderr, "Could not create the panel meshes: %s\n", sl::last_error().c_str());
            sl::shutdown();
            return 1;
        }
    }
    sl::ControlPanel &cockpit = stations[0].assembly.panel;
    sl::ControlPanel &cabinet = stations[1].assembly.panel;

    std::string last_event = "Click or drag something";
    cockpit.on_change = [&](int id, float value) { last_event = "cockpit  " + cockpit.control(id).label + " = " + std::to_string(value).substr(0, 5); };
    cabinet.on_change = [&](int id, float value) { last_event = "cabinet  " + cabinet.control(id).label + " = " + std::to_string(value).substr(0, 5); };

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

    // Camera presets, eased between.
    struct View
    {
        glm::vec3 eye, target;
    };
    const glm::vec3 pilot_eye = glm::vec3(stations[0].model * glm::vec4(0.0f, 1.22f, 0.7f, 1.0f));
    const glm::vec3 pilot_at = glm::vec3(stations[0].panel_world * glm::vec4(0.0f, -0.03f, 0.0f, 1.0f));
    const glm::vec3 cabinet_at = glm::vec3(stations[1].panel_world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    const glm::vec3 cabinet_normal = glm::normalize(glm::vec3(stations[1].panel_world * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));
    const View views[3] = {{pilot_eye, pilot_at}, {cabinet_at + cabinet_normal * 1.15f, cabinet_at}, {{0.0f, 2.3f, 4.4f}, {0.0f, 0.95f, 0.0f}}};
    View view = views[2];
    int view_index = 2;
    bool room_light = true, running = true;
    std::uint32_t last_time = sl::time_ms();

    while (running)
    {
        const std::uint32_t now = sl::time_ms();
        const float dt = std::clamp(static_cast<float>(now - last_time) * 0.001f, 0.001f, 0.1f);
        last_time = now;
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit)
                running = false;
            else if (event.type() == sl::Event::Type::key_down)
            {
                const auto key = event.key();
                if (key == sl::Event::Key::escape)
                    running = false;
                else if (key == sl::Event::Key::digit_1)
                    view_index = 0;
                else if (key == sl::Event::Key::digit_2)
                    view_index = 1;
                else if (key == sl::Event::Key::digit_3)
                    view_index = 2;
                else if (key == sl::Event::Key::letter_l)
                    room_light = !room_light;
            }
            sl::display_handle_event(event);
        }
        const float ease = 1.0f - std::exp(-5.0f * dt);
        view.eye = glm::mix(view.eye, views[view_index].eye, ease);
        view.target = glm::mix(view.target, views[view_index].target, ease);
        sl::Camera camera;
        camera.position = view.eye;
        camera.target = view.target;
        camera.field_of_view = view_index == 0 ? 55.0f : 50.0f;
        camera.near_plane = 0.05f;

        // Mouse to panel: the panel being dragged keeps the pointer, otherwise the nearest one under the mouse.
        const float mx = static_cast<float>(sl::mouse_x()), my = static_cast<float>(sl::mouse_y());
        const bool down = sl::mouse_button_down(sl::MouseButton::left);
        int owner = -1;
        sl::Pointer pointers[2];
        float best = 1e9f;
        for (int i = 0; i < 2; ++i)
        {
            Station &s = stations[i];
            const bool hit = s.view->pick(camera, screen_w, screen_h, mx, my, s.panel_world, &pointers[i]);
            pointers[i].down = down;
            if (s.assembly.panel.active() >= 0)
                owner = i, best = -1.0f;
            else if (hit && pointers[i].x >= 0 && pointers[i].y >= 0 && pointers[i].x <= s.assembly.panel.width() &&
                     pointers[i].y <= s.assembly.panel.height() && best >= 0.0f)
            {
                const float distance = glm::length(glm::vec3(s.panel_world[3]) - camera.position);
                if (distance < best)
                    best = distance, owner = i;
            }
        }
        for (int i = 0; i < 2; ++i)
            stations[i].assembly.panel.update(dt, i == owner ? pointers[i] : sl::Pointer{-1000.0f, -1000.0f, false});

        // The same little simulations as excontrols.
        const bool armed = cockpit.on(c_master) && cockpit.on(c_pump);
        if (armed && cockpit.on(c_start) && fuel > 0.0f)
            engine = true;
        if (!armed || fuel <= 0.0f)
            engine = false;
        const float throttle = cockpit.value(c_thr);
        rpm = approach(rpm, engine ? 700.0f + 2200.0f * throttle : 0.0f, 1.2f, dt);
        const float speed = std::max(0.0f, (rpm - 700.0f) * 0.16f);
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

        // A dim hangar: a weak overhead light per station, plus a small light for every lit lamp.
        std::vector<sl::LocalLight3D> lights;
        if (room_light)
        {
            for (const Station &s : stations)
            {
                sl::LocalLight3D lamp;
                lamp.position = glm::vec3(s.model * glm::vec4(0.0f, 2.6f, 1.0f, 1.0f));
                lamp.colour = {1.0f, 0.93f, 0.8f};
                lamp.intensity = 5.0f;
                lamp.range = 7.0f;
                lights.push_back(lamp);
            }
        }
        for (const Station &s : stations)
        {
            const std::vector<sl::LocalLight3D> glow = s.view->lights(s.assembly.panel, s.panel_world);
            lights.insert(lights.end(), glow.begin(), glow.end());
        }
        sl::Light3D ambient;
        ambient.direction = glm::normalize(glm::vec3(-0.3f, -1.0f, -0.4f));
        ambient.colour = {0.35f, 0.4f, 0.55f};
        ambient.ambient = room_light ? 0.07f : 0.03f;
        sl::set_light_3d(ambient);
        sl::set_local_lights_3d(lights);
        hdr.set_bloom_threshold(1.6f);
        hdr.set_bloom_intensity(0.9f);

        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({4, 5, 9, 255});
            sl::draw_mesh(floor, glm::mat4(1.0f));
            for (const Station &s : stations)
            {
                sl::draw_mesh(s.body, s.model);
                s.view->draw(s.assembly.panel, s.panel_world);
            }
            sl::end_surface_3d();
        }
        sl::clear_to_colour(sl::screen, {0, 0, 0});
        hdr.apply(sl::surface_3d_bitmap(surface), 0, 0, screen_w, screen_h);
        sl::gprintf(16, 12, {255, 255, 255}, "excockpit - 3D control panels   %s", last_event.c_str());
        sl::gprintf_center(screen_h - 24, {200, 210, 230}, "[1] Pilot  [2] Cabinet  [3] Overview  [L] Room light  [Mouse] Operate controls");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    for (Station &s : stations)
    {
        s.view.reset();
        sl::destroy_mesh(s.body);
    }
    hdr.shutdown();
    sl::destroy_mesh(floor);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}

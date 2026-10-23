#ifdef NDEBUG
#undef NDEBUG
#endif
#include "sl.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

namespace
{
    bool near(float a, float b, float eps = 1e-3f)
    {
        return std::abs(a - b) < eps;
    }

    void check_mesh(const sl::MeshData &data, float max_extent)
    {
        assert(!data.vertices.empty());
        assert(data.indices.size() % 3 == 0 && data.indices.size() < 20000);
        glm::vec3 lo{1e9f}, hi{-1e9f};
        for (const sl::Vertex3D &v : data.vertices)
        {
            const glm::vec3 p{v.position[0], v.position[1], v.position[2]};
            const glm::vec3 n{v.normal[0], v.normal[1], v.normal[2]};
            assert(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
            assert(std::abs(glm::length(n) - 1.0f) < 1e-3f);
            lo = glm::min(lo, p);
            hi = glm::max(hi, p);
        }
        for (const std::uint32_t index : data.indices)
            assert(index < data.vertices.size());
        assert(std::max({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z}) <= max_extent);
    }

    void parts()
    {
        check_mesh(sl::make_panel_data(0.6f, 0.4f), 0.7f);
        check_mesh(sl::make_dial_data(0.1f), 0.3f);
        check_mesh(sl::make_needle_data(0.1f), 0.3f);
        check_mesh(sl::make_knob_base_data(0.04f), 0.2f);
        check_mesh(sl::make_knob_data(0.04f, 0.04f), 0.2f);
        check_mesh(sl::make_toggle_base_data(0.04f), 0.2f);
        check_mesh(sl::make_toggle_lever_data(0.04f), 0.2f);
        check_mesh(sl::make_button_base_data(0.03f), 0.2f);
        check_mesh(sl::make_button_cap_data(0.03f), 0.2f);
        check_mesh(sl::make_lamp_bezel_data(0.015f), 0.2f);
        check_mesh(sl::make_lamp_lens_data(0.015f), 0.2f);
        check_mesh(sl::make_slider_track_data(0.2f, 0.04f), 0.4f);
        check_mesh(sl::make_slider_cap_data(0.04f), 0.2f);
        check_mesh(sl::make_meter_housing_data(0.03f, 0.2f), 0.4f);
        check_mesh(sl::make_meter_segment_data(0.026f, 0.014f), 0.2f);

        // A needle points along +Y and the dial face lies on the z = 0 plane.
        const sl::MeshData needle = sl::make_needle_data(0.1f);
        float max_y = -1e9f;
        for (const sl::Vertex3D &v : needle.vertices)
            max_y = std::max(max_y, v.position[1]);
        assert(max_y > 0.08f);
        for (const sl::Vertex3D &v : sl::make_dial_data(0.1f).vertices)
            assert(v.position[2] >= -1e-4f);
    }

    void assemblies()
    {
        const sl::ControlAssembly cockpit = sl::make_cockpit();
        const sl::ControlAssembly cabinet = sl::make_control_cabinet();
        check_mesh(cockpit.body, 4.0f);
        check_mesh(cabinet.body, 4.0f);
        assert(cockpit.panel.find("AIRSPEED") >= 0 && cockpit.panel.find("THR") >= 0 && cockpit.panel.find("WARN") >= 0);
        assert(cabinet.panel.find("E-STOP") >= 0 && cabinet.panel.find("PRESSURE") >= 0);
        assert(cockpit.panel.find("NOPE") == -1);
        for (const glm::mat4 &m : {cockpit.panel_pose, cabinet.panel_pose})
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r)
                    assert(std::isfinite(m[c][r]));
        // Every control lies inside its panel.
        for (const sl::ControlPanel *panel : {&cockpit.panel, &cabinet.panel})
            for (int i = 0; i < panel->count(); ++i)
            {
                const sl::Control &c = panel->control(i);
                assert(c.x - 0.5f * c.width > 0.0f && c.x + 0.5f * c.width < panel->width());
                assert(c.y - 0.5f * c.height > 0.0f && c.y + 0.5f * c.height < panel->height());
            }
        // The cockpit panel is tilted back and faces the pilot at +Z.
        const glm::vec3 normal = glm::normalize(glm::vec3(cockpit.panel_pose * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));
        assert(normal.z > 0.9f && normal.y > 0.2f);
    }

    void logic()
    {
        sl::ControlPanel panel(200.0f, 200.0f);
        const int toggle = panel.add_toggle(30.0f, 30.0f, 20.0f, false, "T");
        const int momentary = panel.add_button(80.0f, 30.0f, 24.0f, {255, 0, 0, 255}, "M", false);
        const int latching = panel.add_button(130.0f, 30.0f, 24.0f, {255, 0, 0, 255}, "L", true);
        const int knob = panel.add_knob(30.0f, 100.0f, 40.0f, 0.0f, 10.0f, 5.0f, "K");
        const int slider = panel.add_slider(100.0f, 100.0f, 80.0f, 0.0f, 100.0f, 0.0f, true, "S");
        const int hslider = panel.add_slider(100.0f, 180.0f, 80.0f, 0.0f, 100.0f, 0.0f, false, "H");
        const int dial = panel.add_dial(160.0f, 100.0f, 50.0f, 0.0f, 100.0f, "D");
        const int lamp = panel.add_lamp(160.0f, 160.0f, 16.0f, {0, 255, 0, 255}, false, "LAMP", true);
        const int meter = panel.add_meter(180.0f, 40.0f, 10.0f, 60.0f, 0.0f, 1.0f, "M");

        int changes = 0;
        int last_id = -1;
        panel.on_change = [&](int id, float)
        {
            ++changes;
            last_id = id;
        };

        assert(panel.hit_test(30.0f, 30.0f) == toggle);
        assert(panel.hit_test(160.0f, 100.0f) == -1); // dials are not interactive
        assert(panel.hit_test(1.0f, 199.0f) == -1);

        // Toggles flip on press, not on release.
        panel.update(0.016f, {30.0f, 30.0f, true});
        assert(panel.on(toggle) && changes == 1 && last_id == toggle);
        panel.update(0.016f, {30.0f, 30.0f, true});
        assert(panel.on(toggle) && changes == 1);
        panel.update(0.016f, {30.0f, 30.0f, false});
        assert(panel.on(toggle));
        panel.update(0.016f, {30.0f, 30.0f, true});
        assert(!panel.on(toggle));
        panel.update(0.016f, {30.0f, 30.0f, false});

        // A momentary button is on only while held, and the pointer may leave it.
        panel.update(0.016f, {80.0f, 30.0f, true});
        assert(panel.on(momentary) && panel.active() == momentary);
        panel.update(0.016f, {190.0f, 190.0f, true});
        assert(panel.on(momentary));
        panel.update(0.016f, {190.0f, 190.0f, false});
        assert(!panel.on(momentary) && panel.active() == -1);

        // A latching button stays on.
        panel.update(0.016f, {130.0f, 30.0f, true});
        panel.update(0.016f, {130.0f, 30.0f, false});
        assert(panel.on(latching));

        // Dragging a knob up raises it and is clamped; down lowers it.
        panel.update(0.016f, {30.0f, 100.0f, true});
        panel.update(0.016f, {30.0f, 80.0f, true});
        const float raised = panel.value(knob);
        assert(raised > 5.0f);
        panel.update(0.016f, {30.0f, -500.0f, true});
        assert(near(panel.value(knob), 10.0f));
        panel.update(0.016f, {30.0f, 600.0f, true});
        assert(near(panel.value(knob), 0.0f));
        panel.update(0.016f, {30.0f, 600.0f, false});

        // A slider jumps to the pointer along its track: the top of a vertical track is the maximum.
        panel.update(0.016f, {100.0f, 60.0f, true});
        assert(near(panel.value(slider), 100.0f));
        panel.update(0.016f, {100.0f, 100.0f, true});
        assert(near(panel.value(slider), 50.0f));
        panel.update(0.016f, {100.0f, 100.0f, false});
        panel.update(0.016f, {140.0f, 180.0f, true});
        assert(near(panel.value(hslider), 100.0f));
        panel.update(0.016f, {140.0f, 180.0f, false});

        // Programmatic values clamp, do not call back, and displays ease toward them.
        const int before = changes;
        panel.set_value(dial, 250.0f);
        assert(near(panel.value(dial), 100.0f) && changes == before);
        panel.update(0.016f, {0.0f, 0.0f, false});
        assert(panel.control(dial).display > 0.0f && panel.control(dial).display < 100.0f);
        for (int i = 0; i < 200; ++i)
            panel.update(0.016f, {0.0f, 0.0f, false});
        assert(near(panel.control(dial).display, 100.0f, 0.05f));
        assert(near(panel.control(dial).angle(), 135.0f * 3.14159265f / 180.0f, 1e-3f));

        // Lamps are dark when off, and a blinking lamp flickers when on.
        assert(panel.glow(lamp) == 0.0f);
        panel.set_on(lamp, true);
        float lo = 1.0f, hi = 0.0f;
        for (int i = 0; i < 100; ++i)
        {
            panel.update(0.016f, {0.0f, 0.0f, false});
            lo = std::min(lo, panel.glow(lamp));
            hi = std::max(hi, panel.glow(lamp));
        }
        assert(hi > 0.9f && lo < 0.3f);

        // Meter segments run green, amber, red.
        assert(sl::meter_segment_colour(0, 12).green > sl::meter_segment_colour(0, 12).red);
        assert(sl::meter_segment_colour(11, 12).red > sl::meter_segment_colour(11, 12).green);
        (void)meter;

        // 2D pointer mapping.
        const sl::Pointer p = sl::control_pointer_2d(100.0f, 50.0f, 2.0f, 140.0f, 90.0f, true);
        assert(near(p.x, 20.0f) && near(p.y, 20.0f) && p.down);
    }

    void picking_ray()
    {
        sl::Camera camera;
        camera.position = {0.0f, 0.0f, 5.0f};
        camera.target = {0.0f, 0.0f, 0.0f};
        const sl::Ray3D centre = sl::screen_ray(camera, 800, 600, 400.0f, 300.0f);
        assert(near(centre.direction.z, -1.0f) && near(centre.direction.x, 0.0f) && near(centre.direction.y, 0.0f));
        // Right of centre bends right, above centre bends up, and the edge matches the field of view.
        const sl::Ray3D right = sl::screen_ray(camera, 800, 600, 800.0f, 300.0f);
        assert(right.direction.x > 0.1f);
        const sl::Ray3D top = sl::screen_ray(camera, 800, 600, 400.0f, 0.0f);
        assert(top.direction.y > 0.0f);
        const float expected = std::tan(glm::radians(camera.field_of_view * 0.5f));
        assert(near(top.direction.y / -top.direction.z, expected));
    }
} // namespace

int main()
{
    parts();
    assemblies();
    logic();
    picking_ray();
    std::printf("controls_test passed\n");
    return 0;
}

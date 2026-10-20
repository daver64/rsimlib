#include "sl.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    std::string model_path;
    std::string animation_path;
    char default_backend[] = "--gl";
    std::vector<char *> backend_arguments{argv[0], default_backend};
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument(argv[i]);
        if (argument == "--gl" || argument == "--vulkan")
            backend_arguments.push_back(argv[i]);
        else if (argument == "--d3d12")
        {
            std::fprintf(stderr, "exmodel3d requires the OpenGL or Vulkan backend.\n");
            return 2;
        }
        else if (argument == "--animations" && i + 1 < argc)
            animation_path = argv[++i];
        else if (model_path.empty())
            model_path = argument;
        else
        {
            std::fprintf(stderr, "Unexpected argument: %s\n", argument.c_str());
            return 2;
        }
    }
    if (model_path.empty())
    {
        std::fprintf(stderr, "Usage: exmodel3d <model.ac|flightgear-model.xml> [--animations file.json] [--gl|--vulkan]\n");
        return 2;
    }
    if (!sl::configure_graphics_backend_from_args(static_cast<int>(backend_arguments.size()), backend_arguments.data()) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 960, 640))
    {
        std::fprintf(stderr, "Graphics initialization failed: %s\n", sl::last_error().c_str());
        return 1;
    }
    sl::set_window_title("simlib - AC3D / FlightGear Model Viewer");
    sl::Surface3D *surface = sl::create_surface_3d(960, 640, 4);
    if (!surface)
    {
        std::fprintf(stderr, "3D surface unavailable: %s\n", sl::last_error().c_str());
        sl::shutdown();
        return 1;
    }

    std::string extension = std::filesystem::path(model_path).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const std::string ac_path = model_path;
    if (extension == ".ac")
    {
        // A FlightGear .ac has no visibility or light rules of its own; they live in the sibling .xml.
        std::filesystem::path sibling = model_path;
        sibling.replace_extension(".xml");
        if (std::filesystem::exists(sibling))
        {
            std::printf("Using FlightGear description %s\n", sibling.string().c_str());
            model_path = sibling.string();
            extension = ".xml";
        }
    }
    sl::Model *model = extension == ".xml"
                           ? sl::load_flightgear_model(model_path)
                           : sl::load_model(model_path);
    if (!model && extension == ".xml" && ac_path != model_path)
    {
        // The sibling .xml may be something else (e.g. a flight-dynamics file), so use the bare .ac.
        std::printf("Not a model description (%s); loading %s alone\n", sl::last_error().c_str(), ac_path.c_str());
        model = sl::load_model(ac_path);
        extension = ".ac";
    }
    if (!model)
    {
        std::fprintf(stderr, "Model load failed: %s\n", sl::last_error().c_str());
        sl::destroy_surface_3d(surface);
        sl::shutdown();
        return 1;
    }
    if (extension == ".xml")
        for (sl::ModelPart &part : model->parts)
            if (part.name == "sun_and_view_blocker" || part.name == "panel_blocker" ||
                part.name == "BeaconMount" || part.name == "BeaconOffX" || part.name == "LandingLightCone" ||
                part.name == "TaxiLightCone")
                part.visible = false;
    if (!animation_path.empty() && !sl::load_model_animations_json(model, animation_path))
    {
        std::fprintf(stderr, "Animation load failed: %s\n", sl::last_error().c_str());
        sl::destroy_model(model);
        sl::destroy_surface_3d(surface);
        sl::shutdown();
        return 1;
    }
    constexpr const char *engine_rpm_property = "/params/engine/rpm";
    const bool has_engine_rpm = std::any_of(model->animations.begin(), model->animations.end(),
                                            [&](const sl::ModelAnimation &animation) {
                                                return animation.type == sl::ModelAnimationType::spin &&
                                                       animation.property == engine_rpm_property;
                                            });
    float engine_rpm = 0.0f;
    if (has_engine_rpm && !sl::set_model_property(model, engine_rpm_property, engine_rpm))
    {
        std::fprintf(stderr, "Unable to initialize propeller RPM: %s\n", sl::last_error().c_str());
        sl::destroy_model(model);
        sl::destroy_surface_3d(surface);
        sl::shutdown();
        return 1;
    }

    sl::Camera camera;
    camera.target = {1.5f, 0.0f, 0.0f};
    float camera_distance = 9.5f;
    {
        // Frame the visible geometry so both aircraft and small instruments start at a sensible size.
        glm::vec3 low(0.0f), high(0.0f);
        bool any = false;
        for (const sl::ModelPart &part : model->parts)
            if (part.has_bounds && sl::model_part_is_visible(model, part))
            {
                low = any ? glm::min(low, part.bounds_min) : part.bounds_min;
                high = any ? glm::max(high, part.bounds_max) : part.bounds_max;
                any = true;
            }
        if (any)
        {
            camera.target = (low + high) * 0.5f;
            camera_distance = std::max(0.05f, glm::length(high - low) * 0.9f);
        }
    }
    float camera_yaw = -0.75f;
    float camera_pitch = 0.28f;
    auto update_camera = [&]() {
        camera.position = camera.target + glm::vec3(
            std::sin(camera_yaw) * std::cos(camera_pitch),
            std::sin(camera_pitch),
            std::cos(camera_yaw) * std::cos(camera_pitch)) * camera_distance;
        // Keep the clip planes proportional so very close views of small models (instruments) are not clipped.
        camera.near_plane = camera_distance * 0.02f;
        camera.far_plane = camera_distance * 5000.0f;
    };
    update_camera();
    sl::set_light_3d({{-0.4f, -1.0f, -0.6f}, 0.3f});

    float animation_value = 0.0f;
    bool animation_enabled = false;
    bool running = true;
    std::uint32_t frame = 0;
    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
                running = false;
            else if (event.type() == sl::Event::Type::key_down)
            {
                if (event.key() == sl::Event::Key::arrow_left)
                    camera_yaw -= 0.08f;
                else if (event.key() == sl::Event::Key::arrow_right)
                    camera_yaw += 0.08f;
                else if (event.key() == sl::Event::Key::arrow_up)
                    camera_pitch = std::min(1.2f, camera_pitch + 0.06f);
                else if (event.key() == sl::Event::Key::arrow_down)
                    camera_pitch = std::max(-1.2f, camera_pitch - 0.06f);
                else if (event.key() == sl::Event::Key::plus || event.key() == sl::Event::Key::equals ||
                         event.key() == sl::Event::Key::keypad_plus)
                    camera_distance = std::max(0.02f, camera_distance / 1.1f);
                else if (event.key() == sl::Event::Key::minus || event.key() == sl::Event::Key::keypad_minus)
                    camera_distance = std::min(500.0f, camera_distance * 1.1f);
                else if (has_engine_rpm && event.key() == sl::Event::Key::left_bracket)
                    engine_rpm = std::max(0.0f, engine_rpm - 50.0f);
                else if (has_engine_rpm && event.key() == sl::Event::Key::right_bracket)
                    engine_rpm = std::min(2500.0f, engine_rpm + 50.0f);
                else if (event.key() == sl::Event::Key::space && !model->animations.empty())
                {
                    animation_enabled = !animation_enabled;
                    animation_value = animation_enabled ? 1.0f : 0.0f;
                    sl::set_model_property(model, model->animations.front().property, animation_value);
                }
                if (has_engine_rpm &&
                    (event.key() == sl::Event::Key::left_bracket || event.key() == sl::Event::Key::right_bracket) &&
                    !sl::set_model_property(model, engine_rpm_property, engine_rpm))
                {
                    std::fprintf(stderr, "Unable to update propeller RPM: %s\n", sl::last_error().c_str());
                    running = false;
                }
                update_camera();
            }
            sl::display_handle_event(event);
        }

        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({30, 40, 58});
            sl::draw_model(model, glm::mat4(1.0f));
            sl::end_surface_3d();
        }
        sl::clear_to_colour(sl::screen, {18, 22, 32});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0, 0);
        sl::gprintf(16, 16, {255, 255, 255},
                    has_engine_rpm ? "Arrows: orbit  =/-: zoom  []: prop RPM  Space: animation  Esc: exit"
                                   : "Arrows: orbit  =/-: zoom  Space: animation  Esc: exit");
        if (has_engine_rpm)
            sl::gprintf(16, 36, {255, 255, 255}, "Engine RPM: %.0f", engine_rpm);
        if (!model->unsupported_animations.empty())
            sl::gprintf(16, has_engine_rpm ? 56 : 36, {255, 205, 120}, "%zu FlightGear animation(s) not imported",
                        model->unsupported_animations.size());
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();

        ++frame;
        const char *frames = std::getenv("RSIMLIB_FRAMES");
        if (frames && frame >= static_cast<std::uint32_t>(std::max(1, std::atoi(frames))))
            running = false;
        const char *screenshot = std::getenv("RSIMLIB_SCREENSHOT");
        if (screenshot && frame == 2 && !sl::save_bitmap(sl::screen, screenshot))
            std::fprintf(stderr, "Screenshot save failed: %s\n", sl::last_error().c_str());
    }

    sl::wait_for_graphics();
    sl::destroy_model(model);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}

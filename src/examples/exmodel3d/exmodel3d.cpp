#include "sl.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
    /** One model input property that drives animations or part visibility. */
    struct Channel
    {
        std::string name;
        std::string tags;
        std::vector<std::string> parts;
        float minimum = 0.0f;
        float maximum = 1.0f;
        float value = 0.0f;
        bool ranged = false;
    };

    Channel &channel_for(std::vector<Channel> &channels, const std::string &name)
    {
        for (Channel &channel : channels)
            if (channel.name == name)
                return channel;
        channels.push_back({});
        channels.back().name = name;
        return channels.back();
    }

    void add_tag(Channel &channel, const char *tag)
    {
        if (channel.tags.find(tag) == std::string::npos)
            channel.tags += (channel.tags.empty() ? "" : ",") + std::string(tag);
    }

    void add_range(Channel &channel, float low, float high)
    {
        channel.minimum = channel.ranged ? std::min(channel.minimum, low) : low;
        channel.maximum = channel.ranged ? std::max(channel.maximum, high) : high;
        channel.ranged = true;
    }

    void add_parts(Channel &channel, const std::vector<std::string> &parts)
    {
        for (const std::string &part : parts)
            if (std::find(channel.parts.begin(), channel.parts.end(), part) == channel.parts.end())
                channel.parts.push_back(part);
    }

    void collect_condition(std::vector<Channel> &channels, const sl::ModelCondition &condition,
                           const std::vector<std::string> &parts)
    {
        if (!condition.property.empty())
        {
            Channel &channel = channel_for(channels, condition.property);
            add_tag(channel, "show");
            add_parts(channel, parts);
            // Cover comparison thresholds so a "rpm > 400" style rule can be satisfied.
            add_range(channel, 0.0f, std::max(1.0f, std::abs(condition.value) * 2.0f));
        }
        for (const sl::ModelCondition &child : condition.children)
            collect_condition(channels, child, parts);
    }

    std::vector<Channel> collect_channels(const sl::Model &model)
    {
        std::vector<Channel> channels;
        for (const sl::ModelAnimation &animation : model.animations)
        {
            if (animation.property.empty())
                continue; // static offsets have no input to drive
            Channel &channel = channel_for(channels, animation.property);
            add_parts(channel, animation.objects);
            float low = 0.0f, high = 1.0f;
            switch (animation.type)
            {
            case sl::ModelAnimationType::rotation: add_tag(channel, "rot"); break;
            case sl::ModelAnimationType::translation: add_tag(channel, "move"); break;
            case sl::ModelAnimationType::spin:
                add_tag(channel, "spin");
                high = 2500.0f;
                break;
            case sl::ModelAnimationType::blend: add_tag(channel, "fade"); break;
            }
            if (!animation.interpolation.empty())
            {
                low = high = animation.interpolation.front().x;
                for (const glm::vec2 &entry : animation.interpolation)
                {
                    low = std::min(low, entry.x);
                    high = std::max(high, entry.x);
                }
            }
            add_range(channel, low, high);
        }
        for (const sl::ModelVisibility &visibility : model.visibility)
            collect_condition(channels, visibility.condition, visibility.objects);
        for (Channel &channel : channels)
            if (channel.maximum <= channel.minimum)
                channel.maximum = channel.minimum + 1.0f;
        return channels;
    }

    std::string short_name(const std::string &name)
    {
        std::size_t cut = name.size();
        for (int segments = 0; segments < 3 && cut != std::string::npos && cut > 0; ++segments)
            cut = name.rfind('/', cut - 1);
        return cut == std::string::npos || cut == 0 ? name : ".../" + name.substr(cut + 1);
    }
} // namespace

int main(int argc, char **argv)
{
    sl::set_text_panel_style({true});
    std::string model_path;
    std::string animation_path;
    std::vector<char *> backend_arguments{argv[0]};
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument(argv[i]);
        if (argument == "--gl" || argument == "--vulkan" || argument == "--d3d12")
            backend_arguments.push_back(argv[i]);
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
        // Fall back to the prop bundled with the examples, beside the working directory or the executable.
        const std::filesystem::path bundled = std::filesystem::path("assets") / "models" / "box.glb";
        const std::filesystem::path exe_dir = std::filesystem::path(argv[0]).parent_path();
        for (const std::filesystem::path &candidate : {bundled, exe_dir / bundled})
        {
            std::error_code ec;
            if (std::filesystem::exists(candidate, ec))
            {
                model_path = candidate.string();
                break;
            }
        }
    }
    if (model_path.empty())
    {
        std::fprintf(stderr, "Usage: exmodel3d [model.glb|model.ac|flightgear-model.xml] [--animations file.json] [--gl|--vulkan|--d3d12]\n");
        return 2;
    }
    if (!sl::configure_graphics_backend_from_args(static_cast<int>(backend_arguments.size()), backend_arguments.data()) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 960, 640))
    {
        std::fprintf(stderr, "Graphics initialization failed: %s\n", sl::last_error().c_str());
        return 1;
    }
    sl::set_window_title("simlib - glTF / AC3D / FlightGear Model Viewer");
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
    std::vector<Channel> channels = collect_channels(*model);
    std::size_t selected = 0;
    auto apply_channel = [&](Channel &channel, float value) {
        channel.value = std::clamp(value, channel.minimum, channel.maximum);
        return sl::set_model_property(model, channel.name, channel.value);
    };
    for (Channel &channel : channels)
        if (!apply_channel(channel, 0.0f))
            std::fprintf(stderr, "Unable to initialize %s: %s\n", channel.name.c_str(), sl::last_error().c_str());
    // Verification hook: preselect a channel and value without keyboard input.
    if (const char *preset = std::getenv("RSIMLIB_CHANNEL"))
    {
        selected = std::min(channels.size() ? channels.size() - 1 : 0, static_cast<std::size_t>(std::atoi(preset)));
        if (const char *preset_value = std::getenv("RSIMLIB_VALUE"); preset_value && !channels.empty())
            apply_channel(channels[selected], static_cast<float>(std::atof(preset_value)));
    }

    // glTF clips: C selects the next clip, P pauses, L toggles looping, S/F slow down or speed up.
    int clip = -1;
    bool paused = false;
    bool clip_loop = true;
    float clip_speed = 0.35f; // the fixture's clips are about half a second to a second long
    auto start_clip = [&](int index) {
        clip = index;
        if (index < 0)
            sl::stop_model_clip(model);
        else
            sl::play_model_clip(model, model->clips[static_cast<std::size_t>(index)].name, clip_loop, clip_speed);
    };
    if (!model->clips.empty())
        start_clip(0);

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
            camera_distance = std::max(0.05f, glm::length(high - low) * (model->clips.empty() ? 0.9f : 2.5f)); // leave room for animated motion
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
                else if (!channels.empty() && (event.key() == sl::Event::Key::tab ||
                                               event.key() == sl::Event::Key::page_down))
                    selected = (selected + 1) % channels.size();
                else if (!channels.empty() && event.key() == sl::Event::Key::page_up)
                    selected = (selected + channels.size() - 1) % channels.size();
                else if (!channels.empty() && event.key() == sl::Event::Key::space)
                {
                    Channel &channel = channels[selected];
                    apply_channel(channel, channel.value > channel.minimum ? channel.minimum : channel.maximum);
                }
                else if (!channels.empty() &&
                         (event.key() == sl::Event::Key::comma || event.key() == sl::Event::Key::period))
                {
                    Channel &channel = channels[selected];
                    const float step = (channel.maximum - channel.minimum) / 20.0f;
                    apply_channel(channel, channel.value + (event.key() == sl::Event::Key::period ? step : -step));
                }
                else if (event.key() == sl::Event::Key::letter_c && !model->clips.empty())
                    start_clip(clip + 1 < static_cast<int>(model->clips.size()) ? clip + 1 : -1);
                else if (event.key() == sl::Event::Key::letter_p)
                    paused = !paused;
                else if (event.key() == sl::Event::Key::letter_s || event.key() == sl::Event::Key::letter_f)
                {
                    clip_speed = std::clamp(clip_speed * (event.key() == sl::Event::Key::letter_f ? 1.25f : 0.8f), 0.05f, 4.0f);
                    model->clip_speed = clip_speed;
                }
                else if (event.key() == sl::Event::Key::letter_l)
                {
                    clip_loop = !clip_loop;
                    if (clip >= 0)
                        start_clip(clip);
                }
                else if (event.key() == sl::Event::Key::home)
                    for (Channel &channel : channels)
                        apply_channel(channel, 0.0f);
                update_camera();
            }
            sl::display_handle_event(event);
        }

        if (!paused)
            sl::update_model_clip(model, static_cast<float>(sl::get_frame_time() / 1000.0));
        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({30, 40, 58});
            sl::draw_model(model, glm::mat4(1.0f));
            sl::end_surface_3d();
        }
        sl::clear_to_colour(sl::screen, {18, 22, 32});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0, 0);
        sl::gprintf(16, 16, {255, 255, 255}, "Arrows: orbit  =/-: zoom  Esc: exit");
        int y = 36;
        if (!model->clips.empty())
        {
            if (clip >= 0)
                sl::gprintf(16, y, {140, 230, 160}, "Clip %d/%zu \"%s\"  %.2f/%.2fs%s%s", clip + 1, model->clips.size(),
                            model->clips[static_cast<std::size_t>(clip)].name.c_str(), model->clip_time,
                            sl::model_clip_duration(model, clip), paused ? "  [paused]" : "",
                            sl::model_clip_playing(model) ? "" : "  [finished]");
            else
                sl::gprintf(16, y, {140, 230, 160}, "Clip: none (rest pose)");
            y += 16;
            sl::gprintf(16, y, {255, 255, 255}, "C: next clip  P: pause  L: loop %s  S/F: speed %.2fx", clip_loop ? "on" : "off", clip_speed);
            y += 24;
        }
        if (channels.empty() && model->clips.empty())
            sl::gprintf(16, y, {200, 200, 200}, "No animations in this model");
        else if (!channels.empty())
        {
            sl::gprintf(16, y, {255, 255, 255}, "Tab/PgUp/PgDn: select  Space: toggle  ,/.: adjust  Home: reset");
            y += 20;
            constexpr int rows = 12;
            const int total = static_cast<int>(channels.size());
            const int first = std::clamp(static_cast<int>(selected) - rows / 2, 0, std::max(0, total - rows));
            for (int i = first; i < std::min(total, first + rows); ++i)
            {
                const Channel &channel = channels[static_cast<std::size_t>(i)];
                const bool current = static_cast<std::size_t>(i) == selected;
                sl::gprintf(16, y, current ? sl::Colour{255, 235, 120} : sl::Colour{190, 200, 215},
                            "%s %2d  %6.2f  %s  (%s)", current ? ">" : " ", i + 1, channel.value,
                            short_name(channel.name).c_str(), channel.tags.c_str());
                y += 16;
            }
            const Channel &channel = channels[selected];
            std::string parts;
            for (std::size_t i = 0; i < channel.parts.size() && i < 3; ++i)
                parts += (i ? ", " : "") + channel.parts[i];
            if (channel.parts.size() > 3)
                parts += " +" + std::to_string(channel.parts.size() - 3);
            sl::gprintf(16, y + 4, {255, 205, 120}, "%s  [%.2f..%.2f]  parts: %s", channel.name.c_str(),
                        channel.minimum, channel.maximum, parts.c_str());
            y += 24;
        }
        if (!model->unsupported_animations.empty())
            sl::gprintf(16, y, {255, 205, 120}, "%zu FlightGear animation(s) not imported",
                        model->unsupported_animations.size());
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();

        ++frame;
        const char *frames = std::getenv("RSIMLIB_FRAMES");
        if (frames && frame >= static_cast<std::uint32_t>(std::max(1, std::atoi(frames))))
            running = false;
        const char *screenshot = std::getenv("RSIMLIB_SCREENSHOT");
        const char *screenshot_frame = std::getenv("RSIMLIB_SCREENSHOT_FRAME");
        if (screenshot && frame == (screenshot_frame ? static_cast<std::uint32_t>(std::atoi(screenshot_frame)) : 2u) && !sl::save_bitmap(sl::screen, screenshot))
            std::fprintf(stderr, "Screenshot save failed: %s\n", sl::last_error().c_str());
    }

    sl::wait_for_graphics();
    sl::destroy_model(model);
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}

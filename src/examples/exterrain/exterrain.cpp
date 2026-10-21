#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

// Chunked-LOD terrain. Fly over procedural fractal terrain: a flat arena (default) or a planet (run with "planet").
//
//   W/S/A/D: move   Space/C: up/down   Shift: fast   Right-drag: look
//   1-4: speed presets   R: new seed   Esc: exit

namespace
{
    // A 2x2 atlas of tiling layers (sand, grass, rock, snow) made from value noise that wraps every 64 pixels.
    sl::Bitmap *make_layer_atlas()
    {
        constexpr int tile = 128;
        sl::Bitmap *atlas = sl::create_bitmap(tile * 2, tile * 2);
        if (!atlas)
            return nullptr;
        struct Layer
        {
            int base[3];
            int variation;
            int period;
        };
        const Layer layers[4] = {{{194, 178, 128}, 26, 16}, {{70, 120, 48}, 50, 8}, {{118, 112, 106}, 60, 32}, {{236, 240, 248}, 14, 32}};
        auto lattice = [](int x, int y, int layer)
        {
            unsigned h = static_cast<unsigned>(x) * 374761393u + static_cast<unsigned>(y) * 668265263u + layer * 2246822519u;
            h = (h ^ (h >> 13)) * 1274126177u;
            return ((h ^ (h >> 16)) & 255u) / 255.0f;
        };
        for (int layer = 0; layer < 4; ++layer)
            for (int y = 0; y < tile; ++y)
                for (int x = 0; x < tile; ++x)
                {
                    float value = 0.0f, amplitude = 1.0f, total = 0.0f;
                    for (int octave = 0; octave < 3; ++octave)
                    {
                        const int period = std::max(layers[layer].period >> octave, 2);
                        const int cells = tile / period;
                        const float fx = static_cast<float>(x) / period, fy = static_cast<float>(y) / period;
                        const int x0 = static_cast<int>(fx), y0 = static_cast<int>(fy);
                        const float tx = fx - x0, ty = fy - y0;
                        auto at = [&](int a, int b) { return lattice(a % cells, b % cells, layer * 4 + octave); };
                        const float a = at(x0, y0) * (1 - tx) + at(x0 + 1, y0) * tx;
                        const float b = at(x0, y0 + 1) * (1 - tx) + at(x0 + 1, y0 + 1) * tx;
                        value += (a * (1 - ty) + b * ty) * amplitude;
                        total += amplitude;
                        amplitude *= 0.5f;
                    }
                    const float shade = (value / total - 0.5f) * layers[layer].variation * 2.0f;
                    auto channel = [&](int c) { return static_cast<std::uint8_t>(std::clamp(layers[layer].base[c] + shade, 0.0f, 255.0f)); };
                    sl::putpixel(atlas, (layer & 1) * tile + x, (layer >> 1) * tile + y,
                                 {channel(0), channel(1), channel(2), 255});
                }
        atlas->smooth = true;
        return atlas;
    }

    // Tangent-space normals (green up) from the atlas brightness, wrapping within each tile.
    sl::Bitmap *make_normal_atlas(const sl::Bitmap *albedo)
    {
        const int tile = albedo->width / 2;
        sl::Bitmap *normals = sl::create_bitmap(albedo->width, albedo->height);
        if (!normals)
            return nullptr;
        const float strength[4] = {4.0f, 6.0f, 14.0f, 5.0f};
        auto height = [&](int layer, int x, int y)
        {
            x = (x + tile) % tile;
            y = (y + tile) % tile;
            const std::size_t row = static_cast<std::size_t>(layer >> 1) * static_cast<std::size_t>(tile) +
                                    static_cast<std::size_t>(y);
            const std::size_t column = static_cast<std::size_t>(layer & 1) * static_cast<std::size_t>(tile) +
                                       static_cast<std::size_t>(x);
            const std::size_t at = (row * static_cast<std::size_t>(albedo->width) + column) * 4;
            return (albedo->pixels[at] + albedo->pixels[at + 1] + albedo->pixels[at + 2]) / (3.0f * 255.0f);
        };
        for (int layer = 0; layer < 4; ++layer)
            for (int y = 0; y < tile; ++y)
                for (int x = 0; x < tile; ++x)
                {
                    // Image y runs down, so brightness increasing downward tilts the normal toward -green.
                    const float dx = (height(layer, x + 1, y) - height(layer, x - 1, y)) * strength[layer];
                    const float dy = (height(layer, x, y + 1) - height(layer, x, y - 1)) * strength[layer];
                    const float length = std::sqrt(dx * dx + dy * dy + 1.0f);
                    auto encode = [](float v) { return static_cast<std::uint8_t>(std::clamp(v * 127.5f + 127.5f, 0.0f, 255.0f)); };
                    sl::putpixel(normals, (layer & 1) * tile + x, (layer >> 1) * tile + y,
                                 {encode(-dx / length), encode(dy / length), encode(1.0f / length), 255});
                }
        return normals;
    }

    // Cuts one layer out of a 2x2 atlas into a bitmap of its own.
    sl::Bitmap *extract_layer(const sl::Bitmap *atlas, int layer)
    {
        const int tile = atlas->width / 2;
        sl::Bitmap *out = sl::create_bitmap(tile, tile);
        if (!out)
            return nullptr;
        for (int y = 0; y < tile; ++y)
            for (int x = 0; x < tile; ++x)
            {
                const std::size_t at = (static_cast<std::size_t>((layer >> 1) * tile + y) * atlas->width +
                                        static_cast<std::size_t>((layer & 1) * tile + x)) * 4;
                sl::putpixel(out, x, y, {atlas->pixels[at], atlas->pixels[at + 1], atlas->pixels[at + 2], atlas->pixels[at + 3]});
            }
        return out;
    }
}

int main(int argc, char **argv)
{
    sl::set_text_panel_style({true});
    bool planet = false;
    std::vector<char *> args;
    for (int i = 0; i < argc; ++i)
    {
        if (i > 0 && std::strcmp(argv[i], "planet") == 0)
            planet = true;
        else
            args.push_back(argv[i]);
    }
    if (!sl::configure_graphics_backend_from_args(static_cast<int>(args.size()), args.data()))
    {
        return -1;
    }

    constexpr int screen_w = 1100;
    constexpr int screen_h = 680;
    if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, screen_w, screen_h))
    {
        std::fprintf(stderr, "Failed to initialize display: %s\n", sl::last_error().c_str());
        return -1;
    }
    sl::set_window_title("simlib - Chunked LOD Terrain (exterrain)");
    sl::Surface3D *surface = sl::create_surface_3d(screen_w, screen_h, 1);
    if (!surface)
    {
        std::fprintf(stderr, "3D surface unavailable on this backend: %s\n", sl::last_error().c_str());
        sl::shutdown();
        return -1;
    }

    sl::Bitmap *atlas = make_layer_atlas();
    sl::Bitmap *normal_atlas = atlas ? make_normal_atlas(atlas) : nullptr;
    sl::Bitmap *layer_albedo[4] = {};
    sl::Bitmap *layer_normal[4] = {};
    for (int i = 0; i < 4 && atlas && normal_atlas; ++i)
    {
        layer_albedo[i] = extract_layer(atlas, i);
        layer_normal[i] = extract_layer(normal_atlas, i);
    }
    bool use_normals = true;
    bool use_atmosphere = true;
    bool use_clouds = true;
    bool volumetric = false;
    double sun_elevation = 0.8;
    int seed = 7;
    bool use_clutter = true;
    std::unique_ptr<sl::Terrain> terrain;
    std::unique_ptr<sl::ClutterLayer> clutter;
    auto build_clutter = [&]()
    {
        clutter = std::make_unique<sl::ClutterLayer>(*terrain);
        const double tree_line = planet ? 1800.0 : 90.0;
        sl::ClutterSpecies trees;
        trees.mesh = sl::make_conifer_data(9.0f, 2.2f);
        trees.density = 0.004f;
        trees.max_height = static_cast<float>(tree_line);
        trees.max_slope = 30.0f;
        trees.min_scale = 0.7f;
        trees.max_scale = 1.4f;
        trees.near_distance = 220.0f;
        trees.far_distance = 1400.0f;
        trees.suitability = [tree_line](const sl::TerrainSample &s)
        { return static_cast<float>(std::clamp(1.0 - s.height / tree_line, 0.1, 1.0)); };
        clutter->add_species(trees);
        sl::ClutterSpecies rocks;
        rocks.mesh = sl::make_rock_data(1.0f, {118, 114, 108, 255}, 5);
        rocks.density = 0.0015f;
        rocks.max_slope = 55.0f;
        rocks.min_scale = 0.5f;
        rocks.max_scale = 2.2f;
        rocks.lean = 0.5f;
        rocks.near_distance = 120.0f;
        rocks.far_distance = 500.0f;
        rocks.seed = 2;
        clutter->add_species(rocks);
    };
    auto build = [&]()
    {
        clutter.reset();
        sl::NoiseParameters noise;
        noise.seed = seed;
        sl::TerrainConfig config;
        if (planet)
        {
            config.shape = sl::TerrainShape::sphere;
            config.radius = 6.371e6;
            noise.frequency = 1.0 / 400000.0;
            noise.amplitude = 6000.0;
            noise.floor_height = 0.0;
            config.finest_cell_size = 2.0;
        }
        else
        {
            config.size = 16384.0;
            config.origin = {-8192.0, 0.0, -8192.0};
            noise.frequency = 1.0 / 4000.0;
            noise.amplitude = 180.0;
            config.finest_cell_size = 1.0;
        }
        config.layers = sl::default_terrain_layers(0.0, planet ? 6000.0 : 180.0);
        for (std::size_t i = 0; i < config.layers.size() && i < 4; ++i)
        {
            config.layers[i].albedo = layer_albedo[i];
            config.layers[i].normal_map = use_normals ? layer_normal[i] : nullptr;
            config.layers[i].tile_metres = planet ? 32.0 : 16.0;
        }
        terrain = std::make_unique<sl::Terrain>(config, std::make_shared<sl::NoiseSource>(noise));
        sl::TerrainTextureOptions textures;
        textures.fade_end = 700.0;
        terrain->set_layer_textures(textures);
        build_clutter();
    };
    build();

    sl::dvec3 position = planet ? sl::dvec3(0.0, 6.371e6 + 3000.0, 0.0) : sl::dvec3(0.0, 700.0, 0.0);
    double yaw = 0.0, pitch = -0.3;
    double speed = planet ? 400.0 : 80.0;
    int last_mouse_x = 0, last_mouse_y = 0;
    std::uint32_t last_time = sl::time_ms();
    bool running = true;

    while (running)
    {
        const std::uint32_t now = sl::time_ms();
        const double dt = std::clamp(static_cast<double>(now - last_time) * 0.001, 0.001, 0.1);
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
                else if (key == sl::Event::Key::letter_r)
                {
                    ++seed;
                    build();
                }
                else if (key == sl::Event::Key::letter_n)
                {
                    use_normals = !use_normals;
                    build();
                }
                else if (key == sl::Event::Key::letter_h)
                    use_atmosphere = !use_atmosphere;
                else if (key == sl::Event::Key::letter_k)
                    use_clouds = !use_clouds;
                else if (key == sl::Event::Key::letter_v)
                    volumetric = !volumetric;
                else if (key == sl::Event::Key::letter_t)
                    use_clutter = !use_clutter;
                else if (key == sl::Event::Key::digit_1)
                    speed = 20.0;
                else if (key == sl::Event::Key::digit_2)
                    speed = 100.0;
                else if (key == sl::Event::Key::digit_3)
                    speed = 500.0;
                else if (key == sl::Event::Key::digit_4)
                    speed = 5000.0;
            }
        }

        const int mx = sl::mouse_x(), my = sl::mouse_y();
        if (sl::mouse_button_down(sl::MouseButton::right))
        {
            yaw -= (mx - last_mouse_x) * 0.005;
            pitch = std::clamp(pitch - (my - last_mouse_y) * 0.005, -1.5, 1.5);
        }
        last_mouse_x = mx;
        last_mouse_y = my;

        // Local frame: "up" is world Y on the flat arena and radial above a planet.
        const sl::dvec3 up = planet ? glm::normalize(position) : sl::dvec3(0.0, 1.0, 0.0);
        const sl::dvec3 ref = std::fabs(up.y) < 0.99 ? sl::dvec3(0.0, 1.0, 0.0) : sl::dvec3(1.0, 0.0, 0.0);
        const sl::dvec3 east = glm::normalize(glm::cross(ref, up));
        const sl::dvec3 north = glm::cross(up, east);
        const sl::dvec3 heading = std::cos(yaw) * north + std::sin(yaw) * -east;
        const sl::dvec3 right = glm::cross(heading, up);
        const sl::dvec3 forward = std::cos(pitch) * heading + std::sin(pitch) * up;

        const double boost = sl::key_down(sl::Key::left_shift) ? 4.0 : 1.0;
        sl::dvec3 move(0.0);
        if (sl::key_down(sl::Key::letter_w))
            move += forward;
        if (sl::key_down(sl::Key::letter_s))
            move -= forward;
        if (sl::key_down(sl::Key::letter_d))
            move += right;
        if (sl::key_down(sl::Key::letter_a))
            move -= right;
        if (sl::key_down(sl::Key::space))
            move += up;
        if (sl::key_down(sl::Key::letter_c))
            move -= up;
        position += move * speed * boost * dt;
        const double altitude = terrain->altitude_of(position);
        if (altitude < 2.0)
            position += up * (2.0 - altitude);

        sl::TerrainView view;
        view.position = position;
        view.forward = forward;
        view.up = up;
        view.fov_y_degrees = 60.0;
        view.aspect = static_cast<double>(screen_w) / screen_h;
        view.viewport_height = screen_h;
        view.far_distance = planet ? 3.0e6 : 30000.0;
        terrain->update(view);
        if (use_clutter)
            clutter->update(view);

        // The camera sits at the origin and the terrain is drawn relative to the eye, so floats stay precise.
        sl::Camera camera;
        camera.position = glm::vec3(0.0f);
        camera.target = glm::vec3(forward);
        camera.up = glm::vec3(up);
        camera.field_of_view = 60.0f;
        camera.near_plane = 0.5f;
        camera.far_plane = static_cast<float>(view.far_distance);

        if (sl::key_down(sl::Key::letter_z))
            sun_elevation = std::max(sun_elevation - dt * 0.4, -0.3);
        if (sl::key_down(sl::Key::letter_x))
            sun_elevation = std::min(sun_elevation + dt * 0.4, 1.5);
        const sl::dvec3 to_sun = glm::normalize(std::cos(sun_elevation) * glm::normalize(north * 0.8 + east * 0.5) +
                                                std::sin(sun_elevation) * up);

        sl::Environment3D environment;
        environment.sun_direction = glm::vec3(-to_sun);
        environment.up = glm::vec3(up);
        environment.camera_altitude = static_cast<float>(altitude);
        environment.camera_position = position;
        environment.time = now * 0.001;
        environment.planet = planet;
        environment.fog_enabled = use_atmosphere;
        environment.fog_density = 1.4e-4f;
        if (planet)
        {
            environment.sun_intensity = 20.0f;
            environment.haze_scale = 0.35f;
            environment.clouds.altitude = 4500.0f;
        }
        environment.clouds.enabled = use_atmosphere && use_clouds;
        environment.clouds.volumetric = volumetric;

        if (sl::begin_surface_3d(surface, camera))
        {
            sl::clear_surface_3d({150, 185, 230, 255});
            sl::Light3D light;
            light.direction = environment.sun_direction;
            light.ambient = 0.3f;
            if (use_atmosphere)
            {
                sl::set_environment_3d(environment);
                sl::draw_sky_3d();
                light.colour = sl::atmosphere_sun_colour(environment);
                light.ambient = 0.12f + 0.2f * std::clamp(static_cast<float>(std::sin(sun_elevation)) * 2.0f + 0.5f, 0.0f, 1.0f);
            }
            else
            {
                sl::clear_environment_3d();
            }
            sl::set_light_3d(light);
            sl::Material material;
            terrain->draw(position, material);
            if (use_clutter)
                clutter->draw(position);
            sl::end_surface_3d();
        }
        sl::clear_to_colour(sl::screen, {150, 185, 230});
        sl::draw_sprite(sl::surface_3d_bitmap(surface), 0, 0);

        const sl::TerrainStats stats = terrain->stats();
        sl::gprintf(12, 10, {20, 20, 20}, "exterrain - %s terrain   %d chunks, %d triangles, deepest level %d / %d",
                    planet ? "planet" : "flat", stats.visible_chunks, stats.visible_triangles, stats.deepest_level,
                    terrain->max_level());
        sl::gprintf(12, 28, {20, 20, 20}, "Altitude %.1f m   Speed %.0f m/s   Resident %d   Loading %d",
                    terrain->altitude_of(position), speed * boost, stats.resident_chunks, stats.pending_chunks);
        if (use_clutter)
        {
            const sl::ClutterStats cs = clutter->stats();
            sl::gprintf(12, 46, {20, 20, 20}, "Trees/rocks: %d objects, %d billboards, %d draws, %d cells", cs.drawn_objects,
                        cs.drawn_billboards, cs.drawn_meshes, cs.cached_cells);
        }
        sl::gprintf_center(screen_h - 22, {20, 20, 20},
                           "WASD move  Space/C up/down  Shift fast  1-4 speed  R seed  N normals  H atmos  K clouds  V vol  T trees  Z/X sun");
        sl::draw_renderer_label();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    clutter.reset();
    terrain.reset();
    sl::destroy_bitmap(atlas);
    sl::destroy_bitmap(normal_atlas);
    for (int i = 0; i < 4; ++i)
    {
        sl::destroy_bitmap(layer_albedo[i]);
        sl::destroy_bitmap(layer_normal[i]);
    }
    sl::destroy_surface_3d(surface);
    sl::shutdown();
    return 0;
}

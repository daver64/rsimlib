#include "sl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    constexpr int MAP_SIZE = 18;
    constexpr int MAX_HEIGHT = 6;

    enum class TerrainType
    {
        Water,
        Sand,
        Grass,
        Dirt,
        Stone,
        Wood
    };

    struct IsoCell
    {
        TerrainType type = TerrainType::Grass;
        int height = 1; // Number of vertical block layers
        int prop = 0;   // 0 = none, 1 = tree, 2 = torch, 3 = column
    };

    struct TerrainColours
    {
        sl::Colour top;
        sl::Colour left;
        sl::Colour right;
    };

    TerrainColours get_colours(TerrainType type)
    {
        switch (type)
        {
        case TerrainType::Water:
            return {{45, 130, 215}, {28, 90, 160}, {36, 110, 185}};
        case TerrainType::Sand:
            return {{235, 205, 135}, {180, 155, 95}, {205, 180, 115}};
        case TerrainType::Grass:
            return {{105, 195, 75}, {68, 140, 48}, {85, 168, 60}};
        case TerrainType::Dirt:
            return {{165, 115, 68}, {115, 78, 44}, {138, 95, 55}};
        case TerrainType::Stone:
            return {{165, 175, 190}, {110, 120, 135}, {135, 145, 160}};
        case TerrainType::Wood:
            return {{195, 145, 90}, {135, 98, 58}, {165, 120, 74}};
        }
        return {{180, 180, 180}, {120, 120, 120}, {150, 150, 150}};
    }

    struct WorldMap
    {
        IsoCell cells[MAP_SIZE][MAP_SIZE];

        void generate()
        {
            const float center = static_cast<float>(MAP_SIZE) * 0.5f - 0.5f;

            for (int y = 0; y < MAP_SIZE; ++y)
            {
                for (int x = 0; x < MAP_SIZE; ++x)
                {
                    IsoCell &cell = cells[y][x];
                    float dx = static_cast<float>(x) - center;
                    float dy = static_cast<float>(y) - center;
                    float dist = std::sqrt(dx * dx + dy * dy);

                    if (dist > 7.5f)
                    {
                        cell.type = TerrainType::Water;
                        cell.height = 1;
                        cell.prop = 0;
                    }
                    else if (dist > 6.0f)
                    {
                        cell.type = TerrainType::Sand;
                        cell.height = 1;
                        cell.prop = 0;
                    }
                    else if (dist > 3.5f)
                    {
                        cell.type = TerrainType::Grass;
                        cell.height = 2;
                        cell.prop = ((x * 5 + y * 11) % 5 == 0) ? 1 : 0; // Trees
                    }
                    else if (dist > 1.8f)
                    {
                        cell.type = TerrainType::Dirt;
                        cell.height = 3;
                        cell.prop = 0;
                    }
                    else
                    {
                        cell.type = TerrainType::Stone;
                        cell.height = 4;
                        cell.prop = (x == MAP_SIZE / 2 && y == MAP_SIZE / 2) ? 2 : 0; // Center torch
                    }
                }
            }

            // Add stone pillars at plateau corners
            const int mid = MAP_SIZE / 2;
            cells[mid - 2][mid - 2].height = 5;
            cells[mid - 2][mid - 2].type = TerrainType::Stone;
            cells[mid - 2][mid - 2].prop = 3;

            cells[mid + 2][mid - 2].height = 5;
            cells[mid + 2][mid - 2].type = TerrainType::Stone;
            cells[mid + 2][mid - 2].prop = 3;

            cells[mid - 2][mid + 2].height = 5;
            cells[mid - 2][mid + 2].type = TerrainType::Stone;
            cells[mid - 2][mid + 2].prop = 3;

            cells[mid + 2][mid + 2].height = 5;
            cells[mid + 2][mid + 2].type = TerrainType::Stone;
            cells[mid + 2][mid + 2].prop = 3;
        }
    };

    struct RenderItem
    {
        float depth = 0.0f;
        int gx = 0;
        int gy = 0;
        int gz = 0; // Vertical layer index
        TerrainType type = TerrainType::Grass;
        bool is_top_layer = false;
        bool is_highlighted = false;
        int prop = 0;
    };

    void draw_block_layer(const sl::IsometricTransform &iso, const RenderItem &item)
    {
        const float z_top = static_cast<float>(item.gz + 1);
        const sl::IsoTileDiamond diamond = iso.tile_diamond(static_cast<float>(item.gx), static_cast<float>(item.gy), z_top);
        const TerrainColours colours = get_colours(item.type);
        const float step_h = (iso.config.elevation_height * iso.zoom);

        // Left face (extending downwards from top diamond to layer floor)
        {
            const float x1 = diamond.left.x;
            const float y1 = diamond.left.y;
            const float x2 = diamond.bottom.x;
            const float y2 = diamond.bottom.y;

            sl::trianglefill(sl::screen, x1, y1, x2, y2, x2, y2 + step_h, colours.left);
            sl::trianglefill(sl::screen, x1, y1, x2, y2 + step_h, x1, y1 + step_h, colours.left);

            // Left face outline
            sl::line(sl::screen, x1, y1 + step_h, x2, y2 + step_h, {25, 30, 40, 160});
            sl::line(sl::screen, x1, y1, x1, y1 + step_h, {25, 30, 40, 160});
        }

        // Right face
        {
            const float x1 = diamond.bottom.x;
            const float y1 = diamond.bottom.y;
            const float x2 = diamond.right.x;
            const float y2 = diamond.right.y;

            sl::trianglefill(sl::screen, x1, y1, x2, y2, x2, y2 + step_h, colours.right);
            sl::trianglefill(sl::screen, x1, y1, x2, y2 + step_h, x1, y1 + step_h, colours.right);

            // Right face outline
            sl::line(sl::screen, x1, y1 + step_h, x2, y2 + step_h, {25, 30, 40, 160});
            sl::line(sl::screen, x2, y2, x2, y2 + step_h, {25, 30, 40, 160});
            sl::line(sl::screen, x1, y1, x1, y1 + step_h, {25, 30, 40, 160});
        }

        // Top diamond (only needed for top layer of the stack)
        if (item.is_top_layer)
        {
            const sl::Colour top_col = item.is_highlighted ? sl::Colour{255, 235, 130} : colours.top;

            sl::trianglefill(sl::screen, diamond.top.x, diamond.top.y,
                             diamond.right.x, diamond.right.y,
                             diamond.bottom.x, diamond.bottom.y, top_col);
            sl::trianglefill(sl::screen, diamond.top.x, diamond.top.y,
                             diamond.bottom.x, diamond.bottom.y,
                             diamond.left.x, diamond.left.y, top_col);

            const sl::Colour edge_col = item.is_highlighted ? sl::Colour{255, 255, 255} : sl::Colour{35, 40, 50, 160};
            const float thickness = item.is_highlighted ? 2.0f : 1.0f;

            sl::line(sl::screen, diamond.top.x, diamond.top.y, diamond.right.x, diamond.right.y, edge_col, thickness);
            sl::line(sl::screen, diamond.right.x, diamond.right.y, diamond.bottom.x, diamond.bottom.y, edge_col, thickness);
            sl::line(sl::screen, diamond.bottom.x, diamond.bottom.y, diamond.left.x, diamond.left.y, edge_col, thickness);
            sl::line(sl::screen, diamond.left.x, diamond.left.y, diamond.top.x, diamond.top.y, edge_col, thickness);
        }
    }

    void draw_prop(const sl::IsometricTransform &iso, int gx, int gy, int gz, int prop)
    {
        if (prop == 0) return;

        const float z_top = static_cast<float>(gz + 1);
        float sx = 0.0f;
        float sy = 0.0f;
        iso.world_to_screen(gx + 0.5f, gy + 0.5f, z_top, sx, sy);

        if (prop == 1) // Tree
        {
            const float trunk_w = 6.0f * iso.zoom;
            const float trunk_h = 22.0f * iso.zoom;
            sl::rectfill(sl::screen, sx - trunk_w * 0.5f, sy - trunk_h, sx + trunk_w * 0.5f, sy, {110, 75, 45});

            const float r = 16.0f * iso.zoom;
            sl::circlefill(sl::screen, sx, sy - trunk_h - r * 0.35f, r, {45, 140, 55});
            sl::circlefill(sl::screen, sx - r * 0.35f, sy - trunk_h, r * 0.75f, {35, 120, 45});
            sl::circlefill(sl::screen, sx + r * 0.35f, sy - trunk_h, r * 0.75f, {55, 160, 65});
            sl::circlefill(sl::screen, sx, sy - trunk_h - r * 0.7f, r * 0.65f, {65, 175, 75});
        }
        else if (prop == 2) // Torch
        {
            const float post_h = 16.0f * iso.zoom;
            sl::line(sl::screen, sx, sy, sx, sy - post_h, {80, 70, 60}, 3.0f * iso.zoom);
            sl::circlefill(sl::screen, sx, sy - post_h, 5.0f * iso.zoom, {255, 160, 40});
            sl::circlefill(sl::screen, sx, sy - post_h, 2.5f * iso.zoom, {255, 240, 180});
        }
        else if (prop == 3) // Column top
        {
            sl::circlefill(sl::screen, sx, sy - 6.0f * iso.zoom, 7.0f * iso.zoom, {220, 215, 200});
        }
    }
}

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 960, 640))
    {
        return -1;
    }

    sl::set_window_title("simlib - Isometric View (exisometric)");

    WorldMap map;
    map.generate();

    sl::IsometricTransform iso;
    iso.config.tile_width = 64.0f;
    iso.config.tile_height = 32.0f;
    iso.config.elevation_height = 16.0f;
    iso.origin_x = 480.0f;
    iso.origin_y = 170.0f;
    iso.zoom = 1.0f;

    sl::Bitmap *scene = sl::create_render_target(960, 640);
    sl::LightingPass lighting;
    const bool lighting_ready = lighting.initialise();
    if (lighting_ready)
    {
        lighting.set_ambient(0.40f);
    }

    bool running = true;
    bool enable_lighting = true;
    bool dragging = false;
    int last_mouse_x = 0;
    int last_mouse_y = 0;
    float torch_pulse = 0.0f;

    sl::set_fps(60);

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
            else if (event.type() == sl::Event::Type::key_down && !event.key_repeat())
            {
                if (event.key() == sl::Event::Key::letter_r)
                {
                    map.generate();
                    iso.origin_x = 480.0f;
                    iso.origin_y = 170.0f;
                    iso.zoom = 1.0f;
                }
                else if (event.key() == sl::Event::Key::letter_l)
                {
                    enable_lighting = !enable_lighting;
                }
                else if (event.key() == sl::Event::Key::plus || event.key() == sl::Event::Key::keypad_plus || event.key() == sl::Event::Key::equals)
                {
                    iso.zoom = std::min(2.5f, iso.zoom + 0.15f);
                }
                else if (event.key() == sl::Event::Key::minus || event.key() == sl::Event::Key::keypad_minus)
                {
                    iso.zoom = std::max(0.4f, iso.zoom - 0.15f);
                }
            }
            else if (event.type() == sl::Event::Type::mouse_button_down)
            {
                if (event.mouse_button() == 2 || event.mouse_button() == 3)
                {
                    dragging = true;
                    last_mouse_x = sl::mouse_x();
                    last_mouse_y = sl::mouse_y();
                }
                else if (event.mouse_button() == 1)
                {
                    // Find hovered tile by testing from top layer down
                    const float mx = static_cast<float>(sl::mouse_x());
                    const float my = static_cast<float>(sl::mouse_y());

                    int picked_x = -1;
                    int picked_y = -1;

                    for (int z = MAX_HEIGHT; z >= 0 && picked_x < 0; --z)
                    {
                        sl::IsoGridPoint g = iso.screen_to_grid(mx, my, static_cast<float>(z));
                        if (g.x >= 0 && g.x < MAP_SIZE && g.y >= 0 && g.y < MAP_SIZE)
                        {
                            if (map.cells[g.y][g.x].height == z)
                            {
                                picked_x = g.x;
                                picked_y = g.y;
                            }
                        }
                    }

                    if (picked_x >= 0 && picked_y >= 0)
                    {
                        IsoCell &cell = map.cells[picked_y][picked_x];
                        cell.height = std::min(MAX_HEIGHT, cell.height + 1);
                        if (cell.type == TerrainType::Water) cell.type = TerrainType::Sand;
                    }
                }
            }
            else if (event.type() == sl::Event::Type::mouse_button_up)
            {
                if (event.mouse_button() == 2 || event.mouse_button() == 3)
                {
                    dragging = false;
                }
            }
            else if (event.type() == sl::Event::Type::mouse_motion)
            {
                if (dragging)
                {
                    iso.origin_x += static_cast<float>(sl::mouse_x() - last_mouse_x);
                    iso.origin_y += static_cast<float>(sl::mouse_y() - last_mouse_y);
                    last_mouse_x = sl::mouse_x();
                    last_mouse_y = sl::mouse_y();
                }
            }
            sl::display_handle_event(event);
        }

        // Camera keyboard panning
        if (sl::key_down(SDL_SCANCODE_LEFT) || sl::key_down(SDL_SCANCODE_A)) iso.origin_x += 6.0f;
        if (sl::key_down(SDL_SCANCODE_RIGHT) || sl::key_down(SDL_SCANCODE_D)) iso.origin_x -= 6.0f;
        if (sl::key_down(SDL_SCANCODE_UP) || sl::key_down(SDL_SCANCODE_W)) iso.origin_y += 6.0f;
        if (sl::key_down(SDL_SCANCODE_DOWN) || sl::key_down(SDL_SCANCODE_S)) iso.origin_y -= 6.0f;

        torch_pulse += 0.05f;

        // Hover picking detection: test layers top-to-bottom
        const float mx = static_cast<float>(sl::mouse_x());
        const float my = static_cast<float>(sl::mouse_y());
        int hovered_x = -1;
        int hovered_y = -1;

        for (int z = MAX_HEIGHT; z >= 0 && hovered_x < 0; --z)
        {
            sl::IsoGridPoint g = iso.screen_to_grid(mx, my, static_cast<float>(z));
            if (g.x >= 0 && g.x < MAP_SIZE && g.y >= 0 && g.y < MAP_SIZE)
            {
                if (map.cells[g.y][g.x].height == z)
                {
                    hovered_x = g.x;
                    hovered_y = g.y;
                }
            }
        }

        // Build list of all block layers across the map
        std::vector<RenderItem> items;
        items.reserve(MAP_SIZE * MAP_SIZE * 3);

        for (int y = 0; y < MAP_SIZE; ++y)
        {
            for (int x = 0; x < MAP_SIZE; ++x)
            {
                const IsoCell &cell = map.cells[y][x];
                for (int z = 0; z < cell.height; ++z)
                {
                    RenderItem item;
                    item.depth = sl::IsometricTransform::depth_sort_key(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                    item.gx = x;
                    item.gy = y;
                    item.gz = z;
                    item.type = cell.type;
                    item.is_top_layer = (z == cell.height - 1);
                    item.is_highlighted = item.is_top_layer && (x == hovered_x && y == hovered_y);
                    item.prop = item.is_top_layer ? cell.prop : 0;
                    items.push_back(item);
                }
            }
        }

        // Sort items back-to-front (lowest depth key to highest depth key)
        std::sort(items.begin(), items.end(), [](const RenderItem &a, const RenderItem &b) {
            return a.depth < b.depth;
        });

        // Render pass
        const bool use_scene = enable_lighting && scene && lighting_ready;
        if (use_scene)
        {
            sl::begin_render_target(scene);
            sl::clear_render_target({18, 22, 30});
        }
        else
        {
            sl::clear_to_colour(sl::screen, {18, 22, 30});
        }

        for (const RenderItem &item : items)
        {
            draw_block_layer(iso, item);
            if (item.prop != 0)
            {
                draw_prop(iso, item.gx, item.gy, item.gz, item.prop);
            }
        }

        if (use_scene)
        {
            sl::end_render_target();

            // Prepare 2D isometric lights from torch location
            std::vector<sl::Light> lights;
            const int mid = MAP_SIZE / 2;
            const int torch_z = map.cells[mid][mid].height;

            float torch_sx = 0.0f;
            float torch_sy = 0.0f;
            iso.world_to_screen(mid + 0.5f, mid + 0.5f, static_cast<float>(torch_z) + 0.8f, torch_sx, torch_sy);

            sl::Light torch;
            torch.x = torch_sx;
            torch.y = torch_sy;
            torch.radius = 320.0f * iso.zoom;
            torch.intensity = 1.3f + std::sin(torch_pulse) * 0.06f;
            torch.colour = {255, 180, 80};
            lights.push_back(torch);

            sl::clear_to_colour(sl::screen, {18, 22, 30});
            lighting.apply(scene, lights, 0, 0, 960, 640);
        }

        // --- HUD Overlay (always rendered crisp on top) ---
        const sl::Colour heading{235, 220, 155};
        const sl::Colour text{218, 226, 235};
        const sl::Colour muted{145, 160, 178};

        sl::gprintf(24, 20, heading, "Isometric View Engine (exisometric)");
        sl::gprintf(24, 44, text, "Left Click: Raise Block | Right/Middle Drag or WASD: Pan | +/-: Zoom (%.2fx)", iso.zoom);
        sl::gprintf(24, 66, text, "L: 2D Soft Lighting (%s) | R: Reset Map | Escape: Exit", enable_lighting ? "ON" : "OFF");

        if (hovered_x >= 0 && hovered_y >= 0)
        {
            const IsoCell &hc = map.cells[hovered_y][hovered_x];
            sl::gprintf(24, 92, {255, 215, 92}, "Hovered Tile: (%d, %d) | Height: %d blocks", hovered_x, hovered_y, hc.height);
        }
        else
        {
            sl::gprintf(24, 92, muted, "Hovered Tile: None");
        }

        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    lighting.shutdown();
    if (scene)
    {
        sl::destroy_bitmap(scene);
    }
    sl::shutdown();
    return 0;
}

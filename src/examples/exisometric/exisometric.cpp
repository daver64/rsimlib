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
            constexpr int MAX_LEVEL = 5;

            for (int y = 0; y < MAP_SIZE; ++y)
            {
                for (int x = 0; x < MAP_SIZE; ++x)
                {
                    IsoCell &cell = cells[y][x];

                    // Distance (in blocks) to the nearest map edge; each step inward is one
                    // less block wide, forming a stepped pyramid of concentric square layers.
                    const int inset = std::min({x, y, MAP_SIZE - 1 - x, MAP_SIZE - 1 - y});
                    const int level = std::min(MAX_LEVEL, inset + 1);

                    cell.height = level;
                    cell.prop = 0;

                    switch (level)
                    {
                    case 1:
                        cell.type = TerrainType::Water;
                        break;
                    case 2:
                        cell.type = TerrainType::Sand;
                        break;
                    case 3:
                        cell.type = TerrainType::Grass;
                        cell.prop = ((x * 5 + y * 11) % 5 == 0) ? 1 : 0; // Trees
                        break;
                    case 4:
                        cell.type = TerrainType::Dirt;
                        break;
                    default:
                        cell.type = TerrainType::Stone;
                        break;
                    }
                }
            }

            const int mid = MAP_SIZE / 2;
            cells[mid][mid].prop = 2; // Center torch

            // Decorative columns at the stone plateau corners
            cells[mid - 2][mid - 2].prop = 3;
            cells[mid + 2][mid - 2].prop = 3;
            cells[mid - 2][mid + 2].prop = 3;
            cells[mid + 2][mid + 2].prop = 3;
        }
    };

    bool point_in_triangle(float px, float py,
                            float ax, float ay,
                            float bx, float by,
                            float cx, float cy)
    {
        const float d1 = (px - bx) * (ay - by) - (ax - bx) * (py - by);
        const float d2 = (px - cx) * (by - cy) - (bx - cx) * (py - cy);
        const float d3 = (px - ax) * (cy - ay) - (cx - ax) * (py - ay);

        const bool has_neg = (d1 < 0.0f) || (d2 < 0.0f) || (d3 < 0.0f);
        const bool has_pos = (d1 > 0.0f) || (d2 > 0.0f) || (d3 > 0.0f);
        return !(has_neg && has_pos);
    }

    // Tests the full visible hexagon silhouette of a column (top diamond + both side
    // faces), not just its top face, so clicks/hovers on a block's visible side register.
    bool point_in_column_silhouette(const sl::IsometricTransform &iso, int gx, int gy, int height,
                                     float mx, float my)
    {
        const sl::IsoTileDiamond top = iso.tile_diamond(static_cast<float>(gx), static_cast<float>(gy), static_cast<float>(height));
        const sl::IsoTileDiamond ground = iso.tile_diamond(static_cast<float>(gx), static_cast<float>(gy), 0.0f);

        if (point_in_triangle(mx, my, top.top.x, top.top.y, top.right.x, top.right.y, top.bottom.x, top.bottom.y) ||
            point_in_triangle(mx, my, top.top.x, top.top.y, top.bottom.x, top.bottom.y, top.left.x, top.left.y))
        {
            return true;
        }

        if (point_in_triangle(mx, my, top.left.x, top.left.y, top.bottom.x, top.bottom.y, ground.bottom.x, ground.bottom.y) ||
            point_in_triangle(mx, my, top.left.x, top.left.y, ground.bottom.x, ground.bottom.y, ground.left.x, ground.left.y))
        {
            return true;
        }

        if (point_in_triangle(mx, my, top.bottom.x, top.bottom.y, top.right.x, top.right.y, ground.right.x, ground.right.y) ||
            point_in_triangle(mx, my, top.bottom.x, top.bottom.y, ground.right.x, ground.right.y, ground.bottom.x, ground.bottom.y))
        {
            return true;
        }

        return false;
    }

    // Picks the front-most column whose visible silhouette (top or either side face) contains
    // the given screen point, matching the same back-to-front painter's order used for drawing.
    bool pick_tile(const sl::IsometricTransform &iso, const WorldMap &map, float mx, float my, int &out_x, int &out_y)
    {
        int best_x = -1;
        int best_y = -1;
        float best_depth = -1.0f;

        for (int y = 0; y < MAP_SIZE; ++y)
        {
            for (int x = 0; x < MAP_SIZE; ++x)
            {
                const int height = map.cells[y][x].height;
                if (!point_in_column_silhouette(iso, x, y, height, mx, my))
                {
                    continue;
                }

                const float depth = sl::IsometricTransform::depth_sort_key(static_cast<float>(x), static_cast<float>(y), static_cast<float>(height));
                if (best_x < 0 || depth > best_depth)
                {
                    best_depth = depth;
                    best_x = x;
                    best_y = y;
                }
            }
        }

        out_x = best_x;
        out_y = best_y;
        return best_x >= 0;
    }

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

    int get_atlas_tile_index(TerrainType type, bool is_top_layer)
    {
        switch (type)
        {
        case TerrainType::Grass:
            // 1-based index 2 (0-based 1) for top grass layer; 1-based index 1 (0-based 0) for dirt beneath
            return is_top_layer ? 1 : 0;
        case TerrainType::Dirt:
            // 1-based index 1 (0-based 0)
            return 0;
        case TerrainType::Sand:
            // 1-based index 4 (0-based 3)
            return 3;
        case TerrainType::Water:
            // 1-based index 5 (0-based 4)
            return 4;
        case TerrainType::Stone:
            // 1-based index 3 (0-based 2)
            return 2;
        case TerrainType::Wood:
            return 24;
        }
        return 0;
    }

    void draw_textured_block_layer(const sl::IsometricTransform &iso, const sl::Atlas &atlas, const RenderItem &item)
    {
        float cx = 0.0f;
        float cy = 0.0f;
        const float z_top = static_cast<float>(item.gz + 1);
        iso.world_to_screen(static_cast<float>(item.gx) + 0.5f, static_cast<float>(item.gy) + 0.5f, z_top, cx, cy);

        const float dest_w = iso.config.tile_width * iso.zoom;
        const float dest_h = (17.0f / 16.0f) * dest_w;
        const float dest_x = cx - dest_w * 0.5f;
        const float dest_y = cy - (iso.config.tile_height * 0.5f * iso.zoom);

        const int tile_idx = get_atlas_tile_index(item.type, item.is_top_layer);
        sl::atlas_stretch_blit(atlas, sl::screen, tile_idx,
                               static_cast<int>(std::round(dest_x)),
                               static_cast<int>(std::round(dest_y)),
                               static_cast<int>(std::round(dest_w)),
                               static_cast<int>(std::round(dest_h)));

        // Highlight diamond outline
        if (item.is_highlighted)
        {
            const sl::IsoTileDiamond diamond = iso.tile_diamond(static_cast<float>(item.gx), static_cast<float>(item.gy), z_top);
            sl::line(sl::screen, diamond.top.x, diamond.top.y, diamond.right.x, diamond.right.y, {255, 255, 255}, 2.0f);
            sl::line(sl::screen, diamond.right.x, diamond.right.y, diamond.bottom.x, diamond.bottom.y, {255, 255, 255}, 2.0f);
            sl::line(sl::screen, diamond.bottom.x, diamond.bottom.y, diamond.left.x, diamond.left.y, {255, 255, 255}, 2.0f);
            sl::line(sl::screen, diamond.left.x, diamond.left.y, diamond.top.x, diamond.top.y, {255, 255, 255}, 2.0f);
        }
    }

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
    iso.config.tile_width = 48.0f;
    iso.config.tile_height = 24.0f;
    iso.config.elevation_height = 27.0f;
    iso.origin_x = 480.0f;
    iso.origin_y = 120.0f;
    iso.zoom = 1.0f;

    sl::Bitmap *scene = sl::create_render_target(960, 640);
    sl::LightingPass lighting;
    const bool lighting_ready = lighting.initialise();
    if (lighting_ready)
    {
        lighting.set_ambient(0.40f);
    }

    sl::Bitmap *atlas_bitmap = sl::load_bitmap("assets/textures/atlas_iso.png");
    sl::Atlas atlas;
    if (atlas_bitmap)
    {
        atlas = sl::create_atlas(atlas_bitmap, 16, 17, 0, 0);
    }

    bool running = true;
    bool enable_lighting = true;
    bool use_texture_atlas = true;
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
                else if (event.key() == sl::Event::Key::letter_t)
                {
                    if (atlas_bitmap && atlas.tile_count > 0)
                    {
                        use_texture_atlas = !use_texture_atlas;
                    }
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
                if (event.mouse_button() == 2)
                {
                    dragging = true;
                    last_mouse_x = sl::mouse_x();
                    last_mouse_y = sl::mouse_y();
                }
                else if (event.mouse_button() == 1)
                {
                    const float mx = static_cast<float>(sl::mouse_x());
                    const float my = static_cast<float>(sl::mouse_y());

                    int picked_x = -1;
                    int picked_y = -1;
                    pick_tile(iso, map, mx, my, picked_x, picked_y);

                    if (picked_x >= 0 && picked_y >= 0)
                    {
                        IsoCell &cell = map.cells[picked_y][picked_x];
                        cell.height = std::min(MAX_HEIGHT, cell.height + 1);
                        if (cell.type == TerrainType::Water) cell.type = TerrainType::Sand;
                    }
                }
                else if (event.mouse_button() == 3)
                {
                    const float mx = static_cast<float>(sl::mouse_x());
                    const float my = static_cast<float>(sl::mouse_y());

                    int picked_x = -1;
                    int picked_y = -1;
                    pick_tile(iso, map, mx, my, picked_x, picked_y);

                    if (picked_x >= 0 && picked_y >= 0)
                    {
                        IsoCell &cell = map.cells[picked_y][picked_x];
                        cell.height = std::max(1, cell.height - 1);
                        if (cell.height == 1)
                        {
                            cell.type = TerrainType::Water;
                            cell.prop = 0;
                        }
                    }
                }

            }
            else if (event.type() == sl::Event::Type::mouse_button_up)
            {
                if (event.mouse_button() == 2)
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

        // Hover picking detection: same silhouette test used for click picking
        const float mx = static_cast<float>(sl::mouse_x());
        const float my = static_cast<float>(sl::mouse_y());
        int hovered_x = -1;
        int hovered_y = -1;
        pick_tile(iso, map, mx, my, hovered_x, hovered_y);

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
            if (use_texture_atlas && atlas.bitmap)
            {
                draw_textured_block_layer(iso, atlas, item);
            }
            else
            {
                draw_block_layer(iso, item);
            }

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
        sl::gprintf(24, 44, text, "Left Click: Raise | Right Click: Lower | Middle Drag or WASD: Pan | +/-: Zoom (%.2fx)", iso.zoom);
        sl::gprintf(24, 66, text, "T: Texture Atlas (%s) | L: Soft Lighting (%s) | R: Reset Map | Escape: Exit",
                    use_texture_atlas ? "ON" : "OFF", enable_lighting ? "ON" : "OFF");

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
    if (atlas_bitmap)
    {
        sl::destroy_bitmap(atlas_bitmap);
    }
    if (scene)
    {
        sl::destroy_bitmap(scene);
    }
    sl::shutdown();
    return 0;
}

// Pixel sprite/animation editor: forked from slpaint, adds a resizable
// (1x1-512x512) document, per-frame undo, zoom, animation frames/playback,
// and PNG horizontal-spritesheet save/load.
#include "sl.h"

#include <imgui.h>

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    constexpr int WINDOW_W = 1200;
    constexpr int WINDOW_H = 860;

    constexpr int PANEL_X = 8;
    constexpr int PANEL_W = 160;
    constexpr int PANEL_TOP = 8;
    constexpr int PANEL_BOTTOM = 760;

    constexpr int VIEWPORT_X = 180;
    constexpr int VIEWPORT_Y = 40;
    constexpr int VIEWPORT_W = 1000;
    constexpr int VIEWPORT_H = 560;

    constexpr int BUTTON_H = 30;
    constexpr int BUTTON_GAP = 4;

    constexpr int MAX_UNDO = 30;
    constexpr int MAX_DOC = 512;
    constexpr int MAX_ZOOM = 64;

    const sl::Colour UI_BG{235, 235, 235};
    const sl::Colour UI_DARK{70, 70, 70};
    const sl::Colour UI_BORDER{130, 130, 130};
    const sl::Colour UI_SELECTED{180, 205, 235};
    const sl::Colour TRANSPARENT{0, 0, 0, 0};

    enum class Tool
    {
        Pencil,
        Brush,
        Eraser,
        Line,
        Rectangle,
        Ellipse,
        Fill,
        Picker,
        Select
    };

    struct Rect
    {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
    };

    const char *tool_name(Tool tool)
    {
        switch (tool)
        {
        case Tool::Pencil:
            return "Pencil";
        case Tool::Brush:
            return "Brush";
        case Tool::Eraser:
            return "Eraser";
        case Tool::Line:
            return "Line";
        case Tool::Rectangle:
            return "Rectangle";
        case Tool::Ellipse:
            return "Ellipse";
        case Tool::Fill:
            return "Fill";
        case Tool::Picker:
            return "Picker";
        case Tool::Select:
            return "Select";
        }
        return "";
    }

    bool inside(int x, int y, int left, int top, int right, int bottom)
    {
        return x >= left && x < right && y >= top && y < bottom;
    }

    Rect normalise_rect(int x1, int y1, int x2, int y2)
    {
        Rect r;
        r.x = std::min(x1, x2);
        r.y = std::min(y1, y2);
        r.w = std::abs(x2 - x1) + 1;
        r.h = std::abs(y2 - y1) + 1;
        return r;
    }

    // The classic Paint palette.
    const std::vector<sl::Colour> PALETTE = {
        {0, 0, 0}, {128, 128, 128}, {255, 255, 255}, {192, 192, 192},
        {128, 0, 0}, {255, 0, 0}, {255, 128, 128}, {128, 64, 64},
        {128, 64, 0}, {255, 128, 0}, {255, 192, 128}, {192, 128, 64},
        {128, 128, 0}, {255, 255, 0}, {255, 255, 128}, {192, 192, 64},
        {0, 128, 0}, {0, 255, 0}, {128, 255, 128}, {64, 128, 64},
        {0, 128, 128}, {0, 255, 255}, {128, 255, 255}, {64, 160, 160},
        {0, 0, 128}, {0, 0, 255}, {128, 128, 255}, {64, 64, 160},
        {128, 0, 128}, {255, 0, 255}, {255, 128, 255}, {160, 64, 160}};

    struct Frame
    {
        sl::Bitmap *bitmap = nullptr;
        std::vector<std::vector<std::uint8_t>> undo_stack;
        std::vector<std::vector<std::uint8_t>> redo_stack;
    };

    struct SpriteDoc
    {
        int width = 32;
        int height = 32;
        std::vector<Frame> frames;
        int fps = 8;
    };

    class SpriteEditorApp
    {
    public:
        SpriteDoc doc;
        int current_frame = 0;

        sl::Bitmap *clipboard = nullptr;
        sl::Bitmap *selection = nullptr;

        Tool tool = Tool::Pencil;

        sl::Colour foreground{0, 0, 0};
        sl::Colour background{255, 255, 255};

        int brush_size = 1;
        int line_thickness = 1;
        bool filled_shapes = false;

        int zoom_override = -1; // -1 means auto-fit.

        bool playing = false;
        double play_timer = 0.0;

        bool running = true;

        // Current mouse operation.
        bool operation_active = false;
        int operation_button = SDL_BUTTON_LEFT;

        int start_x = 0;
        int start_y = 0;
        int last_x = 0;
        int last_y = 0;

        // Selection state.
        bool selecting = false;
        bool moving_selection = false;
        Rect selection_rect;
        int selection_grab_x = 0;
        int selection_grab_y = 0;
        std::vector<std::uint8_t> selection_before;

        std::string status = "Ready";

        // ImGui panel scratch state.
        int new_w = 32;
        int new_h = 32;
        int open_frame_w = 32;
        char path_buffer[256] = "sprite.png";

        // ------------------------------------------------------------
        // Document management
        // ------------------------------------------------------------

        sl::Bitmap *frame_bitmap(int index)
        {
            return doc.frames[index].bitmap;
        }

        sl::Bitmap *current_bitmap()
        {
            return frame_bitmap(current_frame);
        }

        Frame &current()
        {
            return doc.frames[current_frame];
        }

        void destroy_frame(Frame &frame)
        {
            if (frame.bitmap)
                sl::destroy_bitmap(frame.bitmap);
            frame.bitmap = nullptr;
        }

        void reset_document(int width, int height)
        {
            destroy_selection();

            for (Frame &frame : doc.frames)
                destroy_frame(frame);

            doc.frames.clear();
            doc.width = std::clamp(width, 1, MAX_DOC);
            doc.height = std::clamp(height, 1, MAX_DOC);

            Frame frame;
            frame.bitmap = sl::create_bitmap(doc.width, doc.height);
            sl::clear_to_colour(frame.bitmap, TRANSPARENT);
            sl::upload_bitmap(frame.bitmap);
            doc.frames.push_back(frame);

            current_frame = 0;
            zoom_override = -1;
            playing = false;
            status = "New sprite";
        }

        void initialise()
        {
            reset_document(new_w, new_h);
        }

        void shutdown()
        {
            destroy_selection();

            if (clipboard)
            {
                sl::destroy_bitmap(clipboard);
                clipboard = nullptr;
            }

            for (Frame &frame : doc.frames)
                destroy_frame(frame);

            doc.frames.clear();
        }

        void destroy_selection()
        {
            if (selection)
            {
                sl::destroy_bitmap(selection);
                selection = nullptr;
            }

            selecting = false;
            moving_selection = false;
            selection_before.clear();
        }

        // ------------------------------------------------------------
        // Undo / redo (per active frame)
        // ------------------------------------------------------------

        void snapshot()
        {
            Frame &f = current();
            f.undo_stack.push_back(f.bitmap->pixels);

            if (f.undo_stack.size() > MAX_UNDO)
                f.undo_stack.erase(f.undo_stack.begin());

            f.redo_stack.clear();
        }

        void restore_pixels(const std::vector<std::uint8_t> &pixels)
        {
            sl::Bitmap *bmp = current_bitmap();
            if (pixels.size() != bmp->pixels.size())
                return;

            bmp->pixels = pixels;
            bmp->ram_dirty = true;
            sl::upload_bitmap(bmp);
        }

        void undo()
        {
            if (selecting || selection)
            {
                cancel_selection();
                return;
            }

            Frame &f = current();
            if (f.undo_stack.empty())
                return;

            f.redo_stack.push_back(f.bitmap->pixels);
            restore_pixels(f.undo_stack.back());
            f.undo_stack.pop_back();
            status = "Undo";
        }

        void redo()
        {
            if (selecting || selection)
                return;

            Frame &f = current();
            if (f.redo_stack.empty())
                return;

            f.undo_stack.push_back(f.bitmap->pixels);
            restore_pixels(f.redo_stack.back());
            f.redo_stack.pop_back();
            status = "Redo";
        }

        // ------------------------------------------------------------
        // Frames / animation
        // ------------------------------------------------------------

        void select_frame(int index)
        {
            if (index < 0 || index >= static_cast<int>(doc.frames.size()))
                return;

            commit_selection();
            operation_active = false;
            current_frame = index;
        }

        void add_blank_frame()
        {
            commit_selection();

            Frame frame;
            frame.bitmap = sl::create_bitmap(doc.width, doc.height);
            sl::clear_to_colour(frame.bitmap, TRANSPARENT);
            sl::upload_bitmap(frame.bitmap);

            doc.frames.insert(doc.frames.begin() + current_frame + 1, std::move(frame));
            current_frame += 1;
            status = "Added frame";
        }

        void duplicate_frame()
        {
            commit_selection();

            Frame frame;
            frame.bitmap = sl::create_bitmap(doc.width, doc.height);
            sl::blit(current_bitmap(), frame.bitmap, 0, 0, 0, 0, doc.width, doc.height);
            sl::upload_bitmap(frame.bitmap);

            doc.frames.insert(doc.frames.begin() + current_frame + 1, std::move(frame));
            current_frame += 1;
            status = "Duplicated frame";
        }

        void delete_frame()
        {
            if (doc.frames.size() <= 1)
                return;

            commit_selection();

            destroy_frame(doc.frames[current_frame]);
            doc.frames.erase(doc.frames.begin() + current_frame);
            current_frame = std::clamp(current_frame, 0, static_cast<int>(doc.frames.size()) - 1);
            status = "Deleted frame";
        }

        void move_frame(int delta)
        {
            const int target = current_frame + delta;
            if (target < 0 || target >= static_cast<int>(doc.frames.size()))
                return;

            std::swap(doc.frames[current_frame], doc.frames[target]);
            current_frame = target;
        }

        void update_playback()
        {
            if (!playing || doc.frames.size() <= 1)
                return;

            const double interval = 1.0 / std::max(1, doc.fps);
            play_timer += sl::get_frame_time() / 1000.0; // get_frame_time() is milliseconds.

            while (play_timer >= interval)
            {
                play_timer -= interval;
                current_frame = (current_frame + 1) % static_cast<int>(doc.frames.size());
            }
        }

        // ------------------------------------------------------------
        // File I/O
        // ------------------------------------------------------------

        void save(const std::string &path)
        {
            commit_selection();

            const int frame_count = static_cast<int>(doc.frames.size());
            sl::Bitmap *sheet = sl::create_bitmap(doc.width * frame_count, doc.height);

            if (!sheet)
            {
                status = "Save failed (allocation)";
                return;
            }

            sl::clear_to_colour(sheet, TRANSPARENT);

            for (int i = 0; i < frame_count; ++i)
            {
                sl::blit(
                    frame_bitmap(i),
                    sheet,
                    0,
                    0,
                    static_cast<float>(i * doc.width),
                    0,
                    doc.width,
                    doc.height);
            }

            const bool ok = sl::save_bitmap(sheet, path);
            sl::destroy_bitmap(sheet);

            status = ok ? ("Saved: " + path) : "Save failed";
        }

        void load(const std::string &path, int frame_width)
        {
            sl::Bitmap *loaded = sl::load_bitmap(path);

            if (!loaded)
            {
                status = "Could not load " + path;
                return;
            }

            frame_width = std::clamp(frame_width, 1, loaded->width);

            if (loaded->width % frame_width != 0)
            {
                status = "Frame width must divide image width";
                sl::destroy_bitmap(loaded);
                return;
            }

            const int frame_count = loaded->width / frame_width;

            destroy_selection();

            for (Frame &frame : doc.frames)
                destroy_frame(frame);
            doc.frames.clear();

            doc.width = std::clamp(frame_width, 1, MAX_DOC);
            doc.height = std::clamp(loaded->height, 1, MAX_DOC);

            for (int i = 0; i < frame_count; ++i)
            {
                Frame frame;
                frame.bitmap = sl::create_sub_bitmap(loaded, i * frame_width, 0, doc.width, doc.height);
                sl::upload_bitmap(frame.bitmap);
                doc.frames.push_back(frame);
            }

            sl::destroy_bitmap(loaded);

            current_frame = 0;
            zoom_override = -1;
            status = "Loaded: " + path + " (" + std::to_string(frame_count) + " frames)";
        }

        // ------------------------------------------------------------
        // Zoom / viewport
        // ------------------------------------------------------------

        int auto_fit_zoom() const
        {
            int z = 1;
            while (z * 2 * doc.width <= VIEWPORT_W && z * 2 * doc.height <= VIEWPORT_H && z * 2 <= MAX_ZOOM)
                z *= 2;
            return z;
        }

        int current_zoom() const
        {
            return zoom_override > 0 ? zoom_override : auto_fit_zoom();
        }

        void zoom_in()
        {
            zoom_override = std::min(current_zoom() * 2, MAX_ZOOM);
        }

        void zoom_out()
        {
            zoom_override = std::max(current_zoom() / 2, 1);
        }

        int content_w() const { return doc.width * current_zoom(); }
        int content_h() const { return doc.height * current_zoom(); }
        int view_x() const { return VIEWPORT_X + (VIEWPORT_W - content_w()) / 2; }
        int view_y() const { return VIEWPORT_Y + (VIEWPORT_H - content_h()) / 2; }

        bool screen_to_canvas(int sx, int sy, int &x, int &y)
        {
            const int z = current_zoom();
            const int vx = view_x();
            const int vy = view_y();

            if (!inside(sx, sy, vx, vy, vx + content_w(), vy + content_h()))
                return false;

            x = std::clamp((sx - vx) / z, 0, doc.width - 1);
            y = std::clamp((sy - vy) / z, 0, doc.height - 1);
            return true;
        }

        // ------------------------------------------------------------
        // Brush implementation
        // ------------------------------------------------------------

        void brush_stamp(int x, int y, sl::Colour colour)
        {
            if (brush_size <= 1)
            {
                sl::putpixel(current_bitmap(), x, y, colour);
                return;
            }

            sl::circlefill(current_bitmap(), static_cast<float>(x), static_cast<float>(y), brush_size * 0.5f, colour);
        }

        void brush_line(int x1, int y1, int x2, int y2, sl::Colour colour)
        {
            const int dx = x2 - x1;
            const int dy = y2 - y1;
            const int distance = std::max(std::abs(dx), std::abs(dy));

            if (distance == 0)
            {
                brush_stamp(x1, y1, colour);
                return;
            }

            for (int i = 0; i <= distance; ++i)
            {
                const float t = static_cast<float>(i) / distance;
                const int x = static_cast<int>(std::lround(x1 + dx * t));
                const int y = static_cast<int>(std::lround(y1 + dy * t));
                brush_stamp(x, y, colour);
            }
        }

        void flood_fill(int sx, int sy, sl::Colour replacement)
        {
            sl::flood_fill(current_bitmap(), sx, sy, replacement);
        }

        // ------------------------------------------------------------
        // Selection
        // ------------------------------------------------------------

        void begin_selection(int x, int y)
        {
            snapshot();
            selection_before = current_bitmap()->pixels;

            selecting = true;
            moving_selection = false;

            start_x = x;
            start_y = y;
            last_x = x;
            last_y = y;
        }

        void finish_selection()
        {
            if (!selecting)
                return;

            selecting = false;

            Rect r = normalise_rect(start_x, start_y, last_x, last_y);
            r.x = std::clamp(r.x, 0, doc.width - 1);
            r.y = std::clamp(r.y, 0, doc.height - 1);
            r.w = std::min(r.w, doc.width - r.x);
            r.h = std::min(r.h, doc.height - r.y);

            Frame &f = current();

            if (r.w <= 1 || r.h <= 1)
            {
                restore_pixels(selection_before);
                if (!f.undo_stack.empty())
                    f.undo_stack.pop_back();
                selection_before.clear();
                return;
            }

            selection = sl::create_sub_bitmap(current_bitmap(), r.x, r.y, r.w, r.h);

            if (!selection)
            {
                restore_pixels(selection_before);
                if (!f.undo_stack.empty())
                    f.undo_stack.pop_back();
                selection_before.clear();
                status = "Selection failed";
                return;
            }

            selection_rect = r;

            sl::rectfill(
                current_bitmap(),
                static_cast<float>(r.x),
                static_cast<float>(r.y),
                static_cast<float>(r.x + r.w),
                static_cast<float>(r.y + r.h),
                TRANSPARENT);

            sl::upload_bitmap(current_bitmap());
            status = "Selection created";
        }

        void start_selection_move(int x, int y)
        {
            moving_selection = true;
            selection_grab_x = x - selection_rect.x;
            selection_grab_y = y - selection_rect.y;
        }

        void update_selection_move(int x, int y)
        {
            if (!selection)
                return;

            selection_rect.x = std::clamp(x - selection_grab_x, 0, doc.width - selection_rect.w);
            selection_rect.y = std::clamp(y - selection_grab_y, 0, doc.height - selection_rect.h);
        }

        void commit_selection()
        {
            if (!selection)
                return;

            sl::blit(selection, current_bitmap(), 0, 0, selection_rect.x, selection_rect.y, selection->width, selection->height);
            sl::upload_bitmap(current_bitmap());
            destroy_selection();
            status = "Selection committed";
        }

        void cancel_selection()
        {
            if (!selection && !selecting)
                return;

            if (!selection_before.empty())
                restore_pixels(selection_before);

            Frame &f = current();
            if (!f.undo_stack.empty())
                f.undo_stack.pop_back();

            destroy_selection();
            status = "Selection cancelled";
        }

        void copy_selection()
        {
            if (!selection)
                return;

            if (clipboard)
                sl::destroy_bitmap(clipboard);

            clipboard = sl::create_bitmap(selection->width, selection->height);
            if (clipboard)
            {
                sl::blit(selection, clipboard, 0, 0, 0, 0, selection->width, selection->height);
                sl::upload_bitmap(clipboard);
                status = "Copied";
            }
        }

        void cut_selection()
        {
            if (!selection)
                return;

            copy_selection();
            destroy_selection();
            status = "Cut";
        }

        void paste_selection()
        {
            if (!clipboard)
                return;

            commit_selection();
            snapshot();
            selection_before = current_bitmap()->pixels;

            selection = sl::create_bitmap(clipboard->width, clipboard->height);
            if (!selection)
            {
                Frame &f = current();
                if (!f.undo_stack.empty())
                    f.undo_stack.pop_back();
                selection_before.clear();
                return;
            }

            sl::blit(clipboard, selection, 0, 0, 0, 0, clipboard->width, clipboard->height);

            selection_rect.x = 0;
            selection_rect.y = 0;
            selection_rect.w = clipboard->width;
            selection_rect.h = clipboard->height;

            status = "Pasted";
        }

        // ------------------------------------------------------------
        // Tools
        // ------------------------------------------------------------

        sl::Colour colour_for_button(int button)
        {
            return button == SDL_BUTTON_RIGHT ? background : foreground;
        }

        void begin_drawing(int x, int y, int button)
        {
            operation_active = true;
            operation_button = button;

            start_x = last_x = x;
            start_y = last_y = y;

            const sl::Colour colour = colour_for_button(button);

            switch (tool)
            {
            case Tool::Pencil:
                snapshot();
                sl::putpixel(current_bitmap(), x, y, colour);
                break;

            case Tool::Brush:
            case Tool::Eraser:
                snapshot();
                brush_stamp(x, y, tool == Tool::Eraser ? TRANSPARENT : colour);
                break;

            case Tool::Line:
            case Tool::Rectangle:
            case Tool::Ellipse:
                snapshot();
                break;

            case Tool::Fill:
                snapshot();
                flood_fill(x, y, colour);
                operation_active = false;
                break;

            case Tool::Picker:
            {
                const sl::Colour picked = sl::getpixel(current_bitmap(), x, y);
                if (button == SDL_BUTTON_RIGHT)
                    background = picked;
                else
                    foreground = picked;

                status = "Picked colour";
                operation_active = false;
                break;
            }

            case Tool::Select:
                operation_active = false;
                break;
            }
        }

        void update_freehand(int x, int y)
        {
            if (!operation_active)
                return;

            if (tool != Tool::Pencil && tool != Tool::Brush && tool != Tool::Eraser)
                return;

            const sl::Colour colour = tool == Tool::Eraser ? TRANSPARENT : colour_for_button(operation_button);

            if (tool == Tool::Pencil)
                sl::line(current_bitmap(), last_x, last_y, x, y, colour);
            else
                brush_line(last_x, last_y, x, y, colour);

            last_x = x;
            last_y = y;
        }

        void finish_drawing(int x, int y)
        {
            if (!operation_active)
                return;

            operation_active = false;
            const sl::Colour colour = colour_for_button(operation_button);

            switch (tool)
            {
            case Tool::Line:
                sl::line(current_bitmap(), start_x, start_y, x, y, colour, static_cast<float>(line_thickness));
                break;

            case Tool::Rectangle:
            {
                const Rect r = normalise_rect(start_x, start_y, x, y);
                if (filled_shapes)
                    sl::rectfill(current_bitmap(), r.x, r.y, r.x + r.w, r.y + r.h, colour);
                else
                    sl::rect(current_bitmap(), r.x, r.y, r.x + r.w, r.y + r.h, colour, static_cast<float>(line_thickness));
                break;
            }

            case Tool::Ellipse:
            {
                const Rect r = normalise_rect(start_x, start_y, x, y);
                const float cx = r.x + r.w * 0.5f;
                const float cy = r.y + r.h * 0.5f;
                const float rx = r.w * 0.5f;
                const float ry = r.h * 0.5f;

                if (filled_shapes)
                    sl::ellipsefill(current_bitmap(), cx, cy, rx, ry, colour);
                else
                    sl::ellipse(current_bitmap(), cx, cy, rx, ry, colour, static_cast<float>(line_thickness));
                break;
            }

            default:
                break;
            }

            sl::upload_bitmap(current_bitmap());
        }

        // ------------------------------------------------------------
        // Toolbar hit-testing
        // ------------------------------------------------------------

        int tool_button_y(Tool t)
        {
            int index = 0;
            switch (t)
            {
            case Tool::Pencil: index = 0; break;
            case Tool::Brush: index = 1; break;
            case Tool::Eraser: index = 2; break;
            case Tool::Line: index = 3; break;
            case Tool::Rectangle: index = 4; break;
            case Tool::Ellipse: index = 5; break;
            case Tool::Fill: index = 6; break;
            case Tool::Picker: index = 7; break;
            case Tool::Select: index = 8; break;
            }
            return 50 + index * (BUTTON_H + BUTTON_GAP);
        }

        bool button_hit(int mouse_x, int mouse_y, int x, int y, int w, int h)
        {
            return inside(mouse_x, mouse_y, x, y, x + w, y + h);
        }

        Tool tool_at(int mx, int my, bool &hit)
        {
            const Tool tools[] = {
                Tool::Pencil, Tool::Brush, Tool::Eraser, Tool::Line, Tool::Rectangle,
                Tool::Ellipse, Tool::Fill, Tool::Picker, Tool::Select};

            for (int i = 0; i < 9; ++i)
            {
                const int y = 50 + i * (BUTTON_H + BUTTON_GAP);
                if (button_hit(mx, my, PANEL_X, y, PANEL_W, BUTTON_H))
                {
                    hit = true;
                    return tools[i];
                }
            }

            hit = false;
            return tool;
        }

        // ------------------------------------------------------------
        // Mouse events
        // ------------------------------------------------------------

        void mouse_down(int button)
        {
            if (ImGui::GetIO().WantCaptureMouse)
                return;

            const int mx = sl::mouse_x();
            const int my = sl::mouse_y();

            // Palette.
            const int palette_x = PANEL_X + 8;
            const int palette_y = 520;
            const int cell = 18;

            for (int i = 0; i < static_cast<int>(PALETTE.size()); ++i)
            {
                const int px = palette_x + (i % 8) * cell;
                const int py = palette_y + (i / 8) * cell;

                if (button_hit(mx, my, px, py, 16, 16))
                {
                    if (button == SDL_BUTTON_RIGHT)
                        background = PALETTE[i];
                    else
                        foreground = PALETTE[i];

                    status = "Palette colour";
                    return;
                }
            }

            // Tool buttons.
            bool tool_hit_result = false;
            Tool clicked = tool_at(mx, my, tool_hit_result);

            if (tool_hit_result)
            {
                commit_selection();
                tool = clicked;
                status = tool_name(tool);
                return;
            }

            // Size buttons (1, 2, 4, 8, 16).
            const int sizes[] = {1, 2, 4, 8, 16};
            for (int i = 0; i < 5; ++i)
            {
                const int x = PANEL_X + 6 + i * 30;
                if (button_hit(mx, my, x, 410, 26, 28))
                {
                    brush_size = sizes[i];
                    line_thickness = sizes[i];
                    status = "Size: " + std::to_string(brush_size) + "px";
                    return;
                }
            }

            // Fill/outline toggle.
            if (button_hit(mx, my, PANEL_X, 450, PANEL_W, 28))
            {
                filled_shapes = !filled_shapes;
                status = filled_shapes ? "Filled shapes" : "Outline shapes";
                return;
            }

            // Canvas.
            int x, y;
            if (!screen_to_canvas(mx, my, x, y))
                return;

            if (tool == Tool::Select)
            {
                if (selection &&
                    inside(x, y, selection_rect.x, selection_rect.y,
                           selection_rect.x + selection_rect.w, selection_rect.y + selection_rect.h))
                {
                    start_selection_move(x, y);
                    return;
                }

                if (selection)
                    commit_selection();

                begin_selection(x, y);
                return;
            }

            commit_selection();
            begin_drawing(x, y, button);
        }

        void mouse_up(int button)
        {
            const int mx = sl::mouse_x();
            const int my = sl::mouse_y();

            int x, y;
            if (!screen_to_canvas(mx, my, x, y))
            {
                if (tool == Tool::Select && selecting)
                {
                    x = std::clamp((mx - view_x()) / current_zoom(), 0, doc.width - 1);
                    y = std::clamp((my - view_y()) / current_zoom(), 0, doc.height - 1);
                }
                else
                {
                    return;
                }
            }

            if (tool == Tool::Select)
            {
                if (selecting)
                {
                    last_x = x;
                    last_y = y;
                    finish_selection();
                }
                else if (moving_selection)
                {
                    moving_selection = false;
                    status = "Selection moved";
                }

                return;
            }

            if (button == operation_button)
                finish_drawing(x, y);
        }

        // ------------------------------------------------------------
        // Keyboard
        // ------------------------------------------------------------

        void adjust_size(int delta)
        {
            brush_size = std::clamp(brush_size + delta, 1, 64);
            line_thickness = brush_size;
            status = "Size: " + std::to_string(brush_size) + "px";
        }

        void key_down(sl::Event::Key key)
        {
            const bool ctrl = sl::key_down(SDL_SCANCODE_LCTRL) || sl::key_down(SDL_SCANCODE_RCTRL);

            if (ctrl)
            {
                switch (key)
                {
                case sl::Event::Key::letter_z: undo(); return;
                case sl::Event::Key::letter_y: redo(); return;
                case sl::Event::Key::letter_s: save(path_buffer); return;
                case sl::Event::Key::letter_n: reset_document(new_w, new_h); return;
                case sl::Event::Key::letter_c: copy_selection(); return;
                case sl::Event::Key::letter_x: cut_selection(); return;
                case sl::Event::Key::letter_v: paste_selection(); return;
                default: break;
                }
            }

            switch (key)
            {
            case sl::Event::Key::escape: cancel_selection(); break;
            case sl::Event::Key::letter_p: commit_selection(); tool = Tool::Pencil; break;
            case sl::Event::Key::letter_b: commit_selection(); tool = Tool::Brush; break;
            case sl::Event::Key::letter_e: commit_selection(); tool = Tool::Eraser; break;
            case sl::Event::Key::letter_l: commit_selection(); tool = Tool::Line; break;
            case sl::Event::Key::letter_r: commit_selection(); tool = Tool::Rectangle; break;
            case sl::Event::Key::letter_o: commit_selection(); tool = Tool::Ellipse; break;
            case sl::Event::Key::letter_f: commit_selection(); tool = Tool::Fill; break;
            case sl::Event::Key::letter_i: commit_selection(); tool = Tool::Picker; break;
            case sl::Event::Key::letter_s: commit_selection(); tool = Tool::Select; break;

            case sl::Event::Key::digit_1: brush_size = 1; line_thickness = 1; status = "Size: 1px"; break;
            case sl::Event::Key::digit_2: brush_size = 2; line_thickness = 2; status = "Size: 2px"; break;
            case sl::Event::Key::digit_3: brush_size = 4; line_thickness = 4; status = "Size: 4px"; break;
            case sl::Event::Key::digit_4: brush_size = 8; line_thickness = 8; status = "Size: 8px"; break;
            case sl::Event::Key::digit_5: brush_size = 16; line_thickness = 16; status = "Size: 16px"; break;

            case sl::Event::Key::left_bracket:
            case sl::Event::Key::minus:
            case sl::Event::Key::keypad_minus:
                adjust_size(brush_size <= 2 ? -1 : -2);
                break;

            case sl::Event::Key::right_bracket:
            case sl::Event::Key::equals:
            case sl::Event::Key::plus:
            case sl::Event::Key::keypad_plus:
                adjust_size(brush_size < 2 ? 1 : 2);
                break;

            case sl::Event::Key::page_up: zoom_in(); break;
            case sl::Event::Key::page_down: zoom_out(); break;

            case sl::Event::Key::comma: move_frame(-1); break;
            case sl::Event::Key::period: move_frame(1); break;

            default:
                break;
            }
        }

        // ------------------------------------------------------------
        // Rendering (custom-drawn tool panel + canvas)
        // ------------------------------------------------------------

        void draw_button(int x, int y, int w, int h, const char *label, bool selected)
        {
            const sl::Colour fill = selected ? UI_SELECTED : sl::Colour{248, 248, 248};

            sl::rectfill(sl::screen, x, y, x + w, y + h, fill);
            sl::rect(sl::screen, x, y, x + w, y + h, selected ? sl::Colour{70, 100, 150} : UI_BORDER);

            sl::Font *font = sl::get_default_monospace_font();
            const int text_w = sl::text_length(font, label);
            const int text_x = w > text_w ? x + (w - text_w) / 2 : x + 4;

            sl::gprintf(text_x, y + 7, UI_DARK, "%s", label);
        }

        void draw_dashed_rect(int x, int y, int w, int h, sl::Colour colour)
        {
            constexpr int dash = 6;

            for (int px = x; px < x + w; px += dash * 2)
            {
                sl::line(sl::screen, px, y, std::min(px + dash, x + w), y, colour);
                sl::line(sl::screen, px, y + h, std::min(px + dash, x + w), y + h, colour);
            }

            for (int py = y; py < y + h; py += dash * 2)
            {
                sl::line(sl::screen, x, py, x, std::min(py + dash, y + h), colour);
                sl::line(sl::screen, x + w, py, x + w, std::min(py + dash, y + h), colour);
            }
        }

        void draw_preview()
        {
            if (!operation_active)
                return;

            if (tool != Tool::Line && tool != Tool::Rectangle && tool != Tool::Ellipse)
                return;

            const sl::Colour colour = colour_for_button(operation_button);
            const int z = current_zoom();
            const int vx = view_x();
            const int vy = view_y();

            const int x1 = vx + start_x * z;
            const int y1 = vy + start_y * z;
            const int x2 = vx + last_x * z;
            const int y2 = vy + last_y * z;

            const float thickness = static_cast<float>(std::max(1, line_thickness * z));

            if (tool == Tool::Line)
            {
                sl::line(sl::screen, x1, y1, x2, y2,
                         sl::Colour{colour.red, colour.green, colour.blue, 180}, thickness);
                return;
            }

            const Rect r = normalise_rect(x1, y1, x2, y2);
            const sl::Colour preview{colour.red, colour.green, colour.blue, 100};

            if (filled_shapes)
            {
                if (tool == Tool::Rectangle)
                    sl::rectfill(sl::screen, r.x, r.y, r.x + r.w, r.y + r.h, preview);
                else
                    sl::ellipsefill(sl::screen, r.x + r.w * 0.5f, r.y + r.h * 0.5f, r.w * 0.5f, r.h * 0.5f, preview);
            }
            else
            {
                if (tool == Tool::Rectangle)
                    sl::rect(sl::screen, r.x, r.y, r.x + r.w, r.y + r.h, colour, thickness);
                else
                    sl::ellipse(sl::screen, r.x + r.w * 0.5f, r.y + r.h * 0.5f, r.w * 0.5f, r.h * 0.5f, colour, thickness);
            }
        }

        void render()
        {
            sl::clear_to_colour(sl::screen, UI_BG);

            const int z = current_zoom();
            const int vx = view_x();
            const int vy = view_y();
            const int cw = content_w();
            const int ch = content_h();

            // Canvas frame.
            sl::rectfill(sl::screen, vx - 3, vy - 3, vx + cw + 3, vy + ch + 3, sl::Colour{190, 190, 190});
            sl::draw_sprite_stretched(current_bitmap(), static_cast<float>(vx), static_cast<float>(vy), cw, ch);

            if (selection)
            {
                sl::draw_sprite_stretched(
                    selection,
                    static_cast<float>(vx + selection_rect.x * z),
                    static_cast<float>(vy + selection_rect.y * z),
                    selection_rect.w * z,
                    selection_rect.h * z);

                draw_dashed_rect(
                    vx + selection_rect.x * z, vy + selection_rect.y * z,
                    selection_rect.w * z, selection_rect.h * z, sl::Black);
            }

            if (selecting)
            {
                const Rect r = normalise_rect(
                    vx + start_x * z, vy + start_y * z, vx + last_x * z, vy + last_y * z);
                draw_dashed_rect(r.x, r.y, r.w, r.h, sl::Black);
            }

            draw_preview();

            // Left panel.
            sl::rectfill(sl::screen, PANEL_X, PANEL_TOP, PANEL_X + PANEL_W, PANEL_BOTTOM, UI_BG);
            sl::rect(sl::screen, PANEL_X, PANEL_TOP, PANEL_X + PANEL_W, PANEL_BOTTOM, UI_BORDER);
            sl::gprintf(PANEL_X + 8, 20, sl::Black, "SPRITE EDITOR");

            const Tool tools[] = {
                Tool::Pencil, Tool::Brush, Tool::Eraser, Tool::Line, Tool::Rectangle,
                Tool::Ellipse, Tool::Fill, Tool::Picker, Tool::Select};

            for (Tool t : tools)
                draw_button(PANEL_X, tool_button_y(t), PANEL_W, BUTTON_H, tool_name(t), t == tool);

            sl::gprintf(PANEL_X + 8, 392, UI_DARK, "Size: %dpx", brush_size);

            const int sizes[] = {1, 2, 4, 8, 16};
            for (int i = 0; i < 5; ++i)
            {
                const int x = PANEL_X + 6 + i * 30;
                draw_button(x, 410, 26, 28, std::to_string(sizes[i]).c_str(), brush_size == sizes[i]);
            }

            draw_button(PANEL_X, 450, PANEL_W, 28, filled_shapes ? "Filled shapes" : "Outline shapes", false);

            sl::gprintf(PANEL_X + 8, 500, UI_DARK, "Palette");

            const int palette_x = PANEL_X + 8;
            const int palette_y = 520;
            const int cell = 18;

            for (int i = 0; i < static_cast<int>(PALETTE.size()); ++i)
            {
                const int x = palette_x + (i % 8) * cell;
                const int y = palette_y + (i / 8) * cell;
                sl::rectfill(sl::screen, x, y, x + 16, y + 16, PALETTE[i]);
                sl::rect(sl::screen, x, y, x + 16, y + 16, UI_BORDER);
            }

            sl::gprintf(PANEL_X + 8, 610, UI_DARK, "FG / BG");
            sl::rectfill(sl::screen, PANEL_X + 8, 625, PANEL_X + 42, 650, foreground);
            sl::rect(sl::screen, PANEL_X + 8, 625, PANEL_X + 42, 650, UI_DARK);
            sl::rectfill(sl::screen, PANEL_X + 34, 638, PANEL_X + 68, 663, background);
            sl::rect(sl::screen, PANEL_X + 34, 638, PANEL_X + 68, 663, UI_DARK);

            sl::gprintf(PANEL_X + 8, 690, UI_DARK, "Zoom: %dx", z);
            sl::gprintf(PANEL_X + 8, 710, UI_DARK, "Doc: %dx%d", doc.width, doc.height);
            sl::gprintf(PANEL_X + 8, 730, UI_DARK, "Frame: %d/%d", current_frame + 1,
                        static_cast<int>(doc.frames.size()));

            // Status bar.
            sl::rectfill(sl::screen, 8, 770, WINDOW_W - 8, 850, sl::Colour{220, 220, 220});
            sl::rect(sl::screen, 8, 770, WINDOW_W - 8, 850, UI_BORDER);

            sl::gprintf(18, 780, UI_DARK, "%s", status.c_str());
            sl::gprintf(18, 800, UI_DARK,
                        "LMB=fg RMB=bg  1-5/[-/+] size  PgUp/PgDn zoom  Ctrl-Z/Y undo/redo  ,/. move frame");

            sl::gprintf(700, 780, UI_DARK, "Tool: %s", tool_name(tool));
            sl::gprintf(700, 800, UI_DARK, "Pencil P  Brush B  Line L  Rect R  Ellipse O  Fill F  Select S");
        }

        void update()
        {
            update_playback();

            if (!operation_active && !selecting && !moving_selection)
                return;

            const int mx = sl::mouse_x();
            const int my = sl::mouse_y();

            int x, y;
            if (!screen_to_canvas(mx, my, x, y))
                return;

            if (tool == Tool::Select)
            {
                if (selecting)
                {
                    last_x = x;
                    last_y = y;
                }
                else if (moving_selection)
                {
                    update_selection_move(x, y);
                }

                return;
            }

            update_freehand(x, y);

            if (operation_active && (tool == Tool::Line || tool == Tool::Rectangle || tool == Tool::Ellipse))
            {
                last_x = x;
                last_y = y;
            }
        }

        void handle_event(sl::Event &event)
        {
            sl::display_handle_event(event);
            sl::gui_handle_event(event);

            switch (event.type())
            {
            case sl::Event::Type::quit:
                running = false;
                break;

            case sl::Event::Type::mouse_button_down:
                mouse_down(event.mouse_button());
                break;

            case sl::Event::Type::mouse_button_up:
                mouse_up(event.mouse_button());
                break;

            case sl::Event::Type::key_down:
                if (!event.key_repeat() && !ImGui::GetIO().WantCaptureKeyboard)
                    key_down(event.key());
                break;

            default:
                break;
            }
        }

        // ------------------------------------------------------------
        // ImGui panels: file I/O and animation frames.
        // ------------------------------------------------------------

        void render_gui()
        {
            ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_FirstUseEver);
            ImGui::Begin("File");

            ImGui::Text("New sprite");
            ImGui::InputInt("Width##new", &new_w);
            ImGui::InputInt("Height##new", &new_h);
            new_w = std::clamp(new_w, 1, MAX_DOC);
            new_h = std::clamp(new_h, 1, MAX_DOC);

            if (ImGui::Button("New"))
                reset_document(new_w, new_h);

            ImGui::Separator();
            ImGui::InputText("Path", path_buffer, sizeof(path_buffer));

            if (ImGui::Button("Save"))
                save(path_buffer);

            ImGui::SameLine();
            ImGui::InputInt("Frame width##open", &open_frame_w);
            open_frame_w = std::max(1, open_frame_w);

            if (ImGui::Button("Open"))
                load(path_buffer, open_frame_w);

            ImGui::End();

            ImGui::SetNextWindowPos(ImVec2(8, 240), ImGuiCond_FirstUseEver);
            ImGui::Begin("Animation");

            for (int i = 0; i < static_cast<int>(doc.frames.size()); ++i)
            {
                char label[32];
                std::snprintf(label, sizeof(label), "Frame %d", i + 1);

                if (ImGui::Selectable(label, i == current_frame))
                    select_frame(i);
            }

            ImGui::Separator();

            if (ImGui::Button("Add"))
                add_blank_frame();

            ImGui::SameLine();
            if (ImGui::Button("Duplicate"))
                duplicate_frame();

            ImGui::SameLine();
            if (ImGui::Button("Delete"))
                delete_frame();

            if (ImGui::Button("< Move"))
                move_frame(-1);

            ImGui::SameLine();
            if (ImGui::Button("Move >"))
                move_frame(1);

            ImGui::Separator();
            ImGui::SliderInt("FPS", &doc.fps, 1, 60);
            ImGui::Checkbox("Play", &playing);

            ImGui::End();
        }
    };
}

int main(int argc, char **argv)
{
    SpriteEditorApp app;

    sl::configure_graphics_backend_from_args(argc, argv);

    if (!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, WINDOW_W, WINDOW_H, WINDOW_W, WINDOW_H))
        return 1;

    sl::set_window_title("Sprite Editor");
    sl::set_vsync(true);
    sl::set_fps(60);

    sl::gui_init();
    app.initialise();

    if (app.doc.frames.empty())
    {
        sl::gui_shutdown();
        sl::shutdown();
        return 1;
    }

    sl::Event event;

    while (app.running)
    {
        while (sl::poll_event(&event))
            app.handle_event(event);

        app.update();
        app.render();

        sl::new_frame();
        app.render_gui();
        sl::render();

        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    app.shutdown();
    sl::gui_shutdown();
    sl::shutdown();

    return 0;
}

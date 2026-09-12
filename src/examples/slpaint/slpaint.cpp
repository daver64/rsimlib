#include "sl.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <queue>
#include <string>
#include <vector>

namespace
{
    constexpr int WINDOW_W = 1024;
    constexpr int WINDOW_H = 768;

    constexpr int CANVAS_X = 180;
    constexpr int CANVAS_Y = 60;
    constexpr int CANVAS_W = 800;
    constexpr int CANVAS_H = 600;

    constexpr int PANEL_X = 8;
    constexpr int PANEL_W = 160;

    constexpr int BUTTON_H = 30;
    constexpr int BUTTON_GAP = 4;

    constexpr int MAX_UNDO = 30;

    const sl::Colour UI_BG{235, 235, 235};
    const sl::Colour UI_DARK{70, 70, 70};
    const sl::Colour UI_BORDER{130, 130, 130};
    const sl::Colour UI_SELECTED{180, 205, 235};
    const sl::Colour WHITE{255, 255, 255};

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

    struct Point
    {
        int x;
        int y;
    };

    const char *tool_name(Tool tool)
    {
        switch (tool)
        {
        case Tool::Pencil:    return "Pencil";
        case Tool::Brush:     return "Brush";
        case Tool::Eraser:    return "Eraser";
        case Tool::Line:      return "Line";
        case Tool::Rectangle: return "Rectangle";
        case Tool::Ellipse:   return "Ellipse";
        case Tool::Fill:      return "Fill";
        case Tool::Picker:    return "Picker";
        case Tool::Select:    return "Select";
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

    int clamp_canvas_x(int x)
    {
        return std::clamp(x, 0, CANVAS_W - 1);
    }

    int clamp_canvas_y(int y)
    {
        return std::clamp(y, 0, CANVAS_H - 1);
    }

    // The classic Paint palette.
    const std::vector<sl::Colour> PALETTE = {
        {0, 0, 0},
        {128, 128, 128},
        {255, 255, 255},
        {192, 192, 192},

        {128, 0, 0},
        {255, 0, 0},
        {255, 128, 128},
        {128, 64, 64},

        {128, 64, 0},
        {255, 128, 0},
        {255, 192, 128},
        {192, 128, 64},

        {128, 128, 0},
        {255, 255, 0},
        {255, 255, 128},
        {192, 192, 64},

        {0, 128, 0},
        {0, 255, 0},
        {128, 255, 128},
        {64, 128, 64},

        {0, 128, 128},
        {0, 255, 255},
        {128, 255, 255},
        {64, 160, 160},

        {0, 0, 128},
        {0, 0, 255},
        {128, 128, 255},
        {64, 64, 160},

        {128, 0, 128},
        {255, 0, 255},
        {255, 128, 255},
        {160, 64, 160}
    };


    class PaintApp
    {
    public:
        sl::Bitmap *canvas = nullptr;
        sl::Bitmap *clipboard = nullptr;
        sl::Bitmap *selection = nullptr;

        Tool tool = Tool::Pencil;

        sl::Colour foreground{0, 0, 0};
        sl::Colour background{255, 255, 255};

        int brush_size = 4;
        bool filled_shapes = false;

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

        // The canvas state immediately before the selection was created.
        std::vector<std::uint8_t> selection_before;

        // History is just pixel buffers. For an 800x600 RGBA bitmap,
        // one snapshot is ~1.9 MB.
        std::vector<std::vector<std::uint8_t>> undo_stack;
        std::vector<std::vector<std::uint8_t>> redo_stack;

        std::string status = "Ready";

        void initialise()
        {
            canvas = sl::create_bitmap(CANVAS_W, CANVAS_H);

            if (!canvas)
                return;

            sl::clear_to_colour(canvas, WHITE);
            sl::upload_bitmap(canvas);

            status = "Ready - draw something!";
        }

        void shutdown()
        {
            destroy_selection();

            if (clipboard)
            {
                sl::destroy_bitmap(clipboard);
                clipboard = nullptr;
            }

            if (canvas)
            {
                sl::destroy_bitmap(canvas);
                canvas = nullptr;
            }
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

        void set_status(const std::string &text)
        {
            status = text;
        }

        void snapshot()
        {
            if (!canvas)
                return;

            undo_stack.push_back(canvas->pixels);

            if (undo_stack.size() > MAX_UNDO)
                undo_stack.erase(undo_stack.begin());

            redo_stack.clear();
        }

        void restore_pixels(const std::vector<std::uint8_t> &pixels)
        {
            if (!canvas || pixels.size() != canvas->pixels.size())
                return;

            canvas->pixels = pixels;
            canvas->ram_dirty = true;
            sl::upload_bitmap(canvas);
        }

        void undo()
        {
            if (selecting || selection)
            {
                cancel_selection();
                return;
            }

            if (undo_stack.empty())
                return;

            redo_stack.push_back(canvas->pixels);

            restore_pixels(undo_stack.back());
            undo_stack.pop_back();

            status = "Undo";
        }

        void redo()
        {
            if (selecting || selection)
                return;

            if (redo_stack.empty())
                return;

            undo_stack.push_back(canvas->pixels);

            restore_pixels(redo_stack.back());
            redo_stack.pop_back();

            status = "Redo";
        }

        void new_document()
        {
            destroy_selection();

            snapshot();

            sl::clear_to_colour(canvas, WHITE);
            sl::upload_bitmap(canvas);

            undo_stack.clear();
            redo_stack.clear();

            status = "New document";
        }

        void save()
        {
            commit_selection();

            if (sl::save_bitmap(canvas, "painting.png"))
                status = "Saved: painting.png";
            else
                status = "Save failed";
        }

        void load()
        {
            destroy_selection();

            sl::Bitmap *loaded = sl::load_bitmap("painting.png");

            if (!loaded)
            {
                status = "Could not load painting.png";
                return;
            }

            snapshot();

            // Scale an arbitrary input image into our fixed document.
            sl::clear_to_colour(canvas, WHITE);

            sl::stretch_blit(
                loaded,
                canvas,
                0,
                0,
                loaded->width,
                loaded->height,
                0,
                0,
                CANVAS_W,
                CANVAS_H);

            sl::destroy_bitmap(loaded);
            sl::upload_bitmap(canvas);

            status = "Loaded: painting.png";
        }


        // ------------------------------------------------------------
        // Canvas coordinate conversion
        // ------------------------------------------------------------

        bool screen_to_canvas(int sx, int sy, int &x, int &y)
        {
            if (!inside(
                    sx,
                    sy,
                    CANVAS_X,
                    CANVAS_Y,
                    CANVAS_X + CANVAS_W,
                    CANVAS_Y + CANVAS_H))
            {
                return false;
            }

            x = clamp_canvas_x(sx - CANVAS_X);
            y = clamp_canvas_y(sy - CANVAS_Y);
            return true;
        }


        // ------------------------------------------------------------
        // Brush implementation
        // ------------------------------------------------------------

        void brush_stamp(int x, int y, sl::Colour colour)
        {
            if (brush_size <= 1)
            {
                sl::putpixel(canvas, x, y, colour);
                return;
            }

            sl::circlefill(
                canvas,
                static_cast<float>(x),
                static_cast<float>(y),
                brush_size * 0.5f,
                colour);
        }

        void brush_line(
            int x1,
            int y1,
            int x2,
            int y2,
            sl::Colour colour)
        {
            const int dx = x2 - x1;
            const int dy = y2 - y1;

            const int distance =
                std::max(std::abs(dx), std::abs(dy));

            if (distance == 0)
            {
                brush_stamp(x1, y1, colour);
                return;
            }

            for (int i = 0; i <= distance; ++i)
            {
                const float t =
                    static_cast<float>(i) / distance;

                const int x =
                    static_cast<int>(std::lround(
                        x1 + dx * t));

                const int y =
                    static_cast<int>(std::lround(
                        y1 + dy * t));

                brush_stamp(x, y, colour);
            }
        }


        // ------------------------------------------------------------
        // Flood fill
        // ------------------------------------------------------------

        static bool same_colour(
            const sl::Colour &a,
            const sl::Colour &b)
        {
            return a.red == b.red &&
                   a.green == b.green &&
                   a.blue == b.blue &&
                   a.alpha == b.alpha;
        }

        void flood_fill(int sx, int sy, sl::Colour replacement)
        {
            if (!sl::acquire_bitmap(canvas))
                return;

            const std::size_t stride =
                static_cast<std::size_t>(canvas->width) * 4;

            const std::size_t start_offset =
                static_cast<std::size_t>(sy) * stride +
                static_cast<std::size_t>(sx) * 4;

            sl::Colour target{
                canvas->pixels[start_offset + 0],
                canvas->pixels[start_offset + 1],
                canvas->pixels[start_offset + 2],
                canvas->pixels[start_offset + 3]
            };

            if (same_colour(target, replacement))
            {
                sl::release_bitmap(canvas);
                return;
            }

            std::queue<Point> queue;
            queue.push({sx, sy});

            while (!queue.empty())
            {
                Point p = queue.front();
                queue.pop();

                if (p.x < 0 ||
                    p.x >= canvas->width ||
                    p.y < 0 ||
                    p.y >= canvas->height)
                {
                    continue;
                }

                const std::size_t offset =
                    static_cast<std::size_t>(p.y) * stride +
                    static_cast<std::size_t>(p.x) * 4;

                sl::Colour current{
                    canvas->pixels[offset + 0],
                    canvas->pixels[offset + 1],
                    canvas->pixels[offset + 2],
                    canvas->pixels[offset + 3]
                };

                if (!same_colour(current, target))
                    continue;

                canvas->pixels[offset + 0] = replacement.red;
                canvas->pixels[offset + 1] = replacement.green;
                canvas->pixels[offset + 2] = replacement.blue;
                canvas->pixels[offset + 3] = replacement.alpha;

                queue.push({p.x + 1, p.y});
                queue.push({p.x - 1, p.y});
                queue.push({p.x, p.y + 1});
                queue.push({p.x, p.y - 1});
            }

            canvas->ram_dirty = true;
            sl::release_bitmap(canvas);
        }


        // ------------------------------------------------------------
        // Selection
        // ------------------------------------------------------------

        void begin_selection(int x, int y)
        {
            snapshot();

            selection_before = canvas->pixels;

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

            Rect r = normalise_rect(
                start_x,
                start_y,
                last_x,
                last_y);

            r.x = std::clamp(r.x, 0, CANVAS_W - 1);
            r.y = std::clamp(r.y, 0, CANVAS_H - 1);

            r.w = std::min(r.w, CANVAS_W - r.x);
            r.h = std::min(r.h, CANVAS_H - r.y);

            if (r.w <= 1 || r.h <= 1)
            {
                // Nothing selected. Undo the history entry because
                // this operation did not change the document.
                restore_pixels(selection_before);

                if (!undo_stack.empty())
                    undo_stack.pop_back();

                selection_before.clear();
                return;
            }

            selection = sl::create_sub_bitmap(
                canvas,
                r.x,
                r.y,
                r.w,
                r.h);

            if (!selection)
            {
                restore_pixels(selection_before);

                if (!undo_stack.empty())
                    undo_stack.pop_back();

                selection_before.clear();
                status = "Selection failed";
                return;
            }

            selection_rect = r;

            // Pick the pixels up from the document.
            // This is intentionally the simple opaque Paint behaviour.
            sl::rectfill(
                canvas,
                static_cast<float>(r.x),
                static_cast<float>(r.y),
                static_cast<float>(r.x + r.w),
                static_cast<float>(r.y + r.h),
                background);

            sl::upload_bitmap(canvas);

            status = "Selection created";
        }

        void start_selection_move(int x, int y)
        {
            moving_selection = true;

            selection_grab_x =
                x - selection_rect.x;

            selection_grab_y =
                y - selection_rect.y;
        }

        void update_selection_move(int x, int y)
        {
            if (!selection)
                return;

            selection_rect.x =
                std::clamp(
                    x - selection_grab_x,
                    0,
                    CANVAS_W - selection_rect.w);

            selection_rect.y =
                std::clamp(
                    y - selection_grab_y,
                    0,
                    CANVAS_H - selection_rect.h);
        }

        void commit_selection()
        {
            if (!selection)
                return;

            sl::blit(
                selection,
                canvas,
                0,
                0,
                selection_rect.x,
                selection_rect.y,
                selection->width,
                selection->height);

            sl::upload_bitmap(canvas);

            destroy_selection();

            status = "Selection committed";
        }

        void cancel_selection()
        {
            if (!selection && !selecting)
                return;

            if (!selection_before.empty())
                restore_pixels(selection_before);

            if (!undo_stack.empty())
                undo_stack.pop_back();

            destroy_selection();

            status = "Selection cancelled";
        }

        void copy_selection()
        {
            if (!selection)
                return;

            if (clipboard)
                sl::destroy_bitmap(clipboard);

            clipboard = sl::create_bitmap(
                selection->width,
                selection->height);

            if (clipboard)
            {
                sl::blit(
                    selection,
                    clipboard,
                    0,
                    0,
                    0,
                    0,
                    selection->width,
                    selection->height);

                sl::upload_bitmap(clipboard);
                status = "Copied";
            }
        }

        void cut_selection()
        {
            if (!selection)
                return;

            copy_selection();

            // The selection has already been removed from the canvas.
            destroy_selection();

            status = "Cut";
        }

        void paste_selection()
        {
            if (!clipboard)
                return;

            commit_selection();

            snapshot();

            selection_before = canvas->pixels;

            selection = sl::create_bitmap(
                clipboard->width,
                clipboard->height);

            if (!selection)
            {
                if (!undo_stack.empty())
                    undo_stack.pop_back();

                selection_before.clear();
                return;
            }

            sl::blit(
                clipboard,
                selection,
                0,
                0,
                0,
                0,
                clipboard->width,
                clipboard->height);

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
            return button == SDL_BUTTON_RIGHT
                ? background
                : foreground;
        }

        void begin_drawing(int x, int y, int button)
        {
            operation_active = true;
            operation_button = button;

            start_x = last_x = x;
            start_y = last_y = y;

            const sl::Colour colour =
                colour_for_button(button);

            switch (tool)
            {
            case Tool::Pencil:
                snapshot();
                sl::putpixel(canvas, x, y, colour);
                break;

            case Tool::Brush:
            case Tool::Eraser:
                snapshot();

                if (tool == Tool::Eraser)
                    brush_stamp(x, y, background);
                else
                    brush_stamp(x, y, colour);

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
                const sl::Colour picked =
                    sl::getpixel(canvas, x, y);

                if (button == SDL_BUTTON_RIGHT)
                    background = picked;
                else
                    foreground = picked;

                status = "Picked colour";
                operation_active = false;
                break;
            }

            case Tool::Select:
                // Handled separately.
                operation_active = false;
                break;
            }
        }
        
        void update_freehand(int x, int y)
        {
            if (!operation_active)
                return;

            if (tool != Tool::Pencil &&
                tool != Tool::Brush &&
                tool != Tool::Eraser)
            {
                return;
            }

            const sl::Colour colour =
                tool == Tool::Eraser
                    ? background
                    : colour_for_button(operation_button);

            if (tool == Tool::Pencil)
            {
                sl::line(
                    canvas,
                    last_x,
                    last_y,
                    x,
                    y,
                    colour);
            }
            else
            {
                brush_line(
                    last_x,
                    last_y,
                    x,
                    y,
                    colour);
            }

            last_x = x;
            last_y = y;
        }

        void finish_drawing(int x, int y)
        {
            if (!operation_active)
                return;

            operation_active = false;

            switch (tool)
            {
            case Tool::Line:
            {
                const sl::Colour colour =
                    colour_for_button(operation_button);

                sl::line(
                    canvas,
                    start_x,
                    start_y,
                    x,
                    y,
                    colour);

                break;
            }

            case Tool::Rectangle:
            {
                const Rect r =
                    normalise_rect(
                        start_x,
                        start_y,
                        x,
                        y);

                const sl::Colour colour =
                    colour_for_button(operation_button);

                if (filled_shapes)
                {
                    sl::rectfill(
                        canvas,
                        r.x,
                        r.y,
                        r.x + r.w,
                        r.y + r.h,
                        colour);
                }
                else
                {
                    sl::rect(
                        canvas,
                        r.x,
                        r.y,
                        r.x + r.w,
                        r.y + r.h,
                        colour);
                }

                break;
            }

            case Tool::Ellipse:
            {
                const Rect r =
                    normalise_rect(
                        start_x,
                        start_y,
                        x,
                        y);

                const sl::Colour colour =
                    colour_for_button(operation_button);

                const float cx =
                    r.x + r.w * 0.5f;

                const float cy =
                    r.y + r.h * 0.5f;

                const float rx =
                    r.w * 0.5f;

                const float ry =
                    r.h * 0.5f;

                if (filled_shapes)
                {
                    sl::ellipsefill(
                        canvas,
                        cx,
                        cy,
                        rx,
                        ry,
                        colour);
                }
                else
                {
                    sl::ellipse(
                        canvas,
                        cx,
                        cy,
                        rx,
                        ry,
                        colour);
                }

                break;
            }

            default:
                break;
            }

            sl::upload_bitmap(canvas);
        }


        // ------------------------------------------------------------
        // Toolbar
        // ------------------------------------------------------------

        int tool_button_y(Tool t)
        {
            int index = 0;

            switch (t)
            {
            case Tool::Pencil:    index = 0; break;
            case Tool::Brush:     index = 1; break;
            case Tool::Eraser:    index = 2; break;
            case Tool::Line:      index = 3; break;
            case Tool::Rectangle: index = 4; break;
            case Tool::Ellipse:   index = 5; break;
            case Tool::Fill:      index = 6; break;
            case Tool::Picker:    index = 7; break;
            case Tool::Select:    index = 8; break;
            }

            return 50 + index * (BUTTON_H + BUTTON_GAP);
        }

        bool button_hit(int mouse_x, int mouse_y, int x, int y, int w, int h)
        {
            return inside(
                mouse_x,
                mouse_y,
                x,
                y,
                x + w,
                y + h);
        }

        Tool tool_at(int mx, int my, bool &hit)
        {
            const Tool tools[] = {
                Tool::Pencil,
                Tool::Brush,
                Tool::Eraser,
                Tool::Line,
                Tool::Rectangle,
                Tool::Ellipse,
                Tool::Fill,
                Tool::Picker,
                Tool::Select
            };

            for (int i = 0; i < 9; ++i)
            {
                const int y =
                    50 + i * (BUTTON_H + BUTTON_GAP);

                if (button_hit(
                        mx,
                        my,
                        PANEL_X,
                        y,
                        PANEL_W,
                        BUTTON_H))
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
            const int mx = sl::mouse_x();
            const int my = sl::mouse_y();

            // Palette.
            const int palette_x = PANEL_X + 8;
            const int palette_y = 520;
            const int cell = 18;

            for (int i = 0; i < static_cast<int>(PALETTE.size()); ++i)
            {
                const int px =
                    palette_x + (i % 8) * cell;

                const int py =
                    palette_y + (i / 8) * cell;

                if (button_hit(
                        mx,
                        my,
                        px,
                        py,
                        16,
                        16))
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
            Tool clicked =
                tool_at(mx, my, tool_hit_result);

            if (tool_hit_result)
            {
                commit_selection();
                tool = clicked;
                status = tool_name(tool);
                return;
            }

            // Brush-size buttons.
            const int sizes[] = {1, 4, 8, 16};

            for (int i = 0; i < 4; ++i)
            {
                const int x =
                    PANEL_X + 8 + i * 36;

                if (button_hit(mx, my, x, 410, 32, 28))
                {
                    brush_size = sizes[i];
                    status = "Brush size";
                    return;
                }
            }

            // Fill/outline toggle.
            if (button_hit(mx, my, PANEL_X, 450, PANEL_W, 28))
            {
                filled_shapes = !filled_shapes;
                status =
                    filled_shapes
                        ? "Filled shapes"
                        : "Outline shapes";
                return;
            }

            // File buttons.
            if (button_hit(mx, my, PANEL_X, 485, 48, 28))
            {
                new_document();
                return;
            }

            if (button_hit(mx, my, PANEL_X + 54, 485, 48, 28))
            {
                save();
                return;
            }

            if (button_hit(mx, my, PANEL_X + 108, 485, 48, 28))
            {
                load();
                return;
            }

            // Canvas.
            int x, y;

            if (!screen_to_canvas(mx, my, x, y))
                return;

            if (tool == Tool::Select)
            {
                // Clicking an existing selection moves it.
                if (selection &&
                    inside(
                        x,
                        y,
                        selection_rect.x,
                        selection_rect.y,
                        selection_rect.x + selection_rect.w,
                        selection_rect.y + selection_rect.h))
                {
                    start_selection_move(x, y);
                    return;
                }

                // Click outside an existing selection commits it
                // before starting a new selection.
                if (selection)
                    commit_selection();

                begin_selection(x, y);
                return;
            }

            // Any other tool commits a floating selection.
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
                if (tool == Tool::Select &&
                    selecting)
                {
                    x = clamp_canvas_x(mx - CANVAS_X);
                    y = clamp_canvas_y(my - CANVAS_Y);
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

        void key_down(sl::Event::Key key)
        {
            const bool ctrl =
                sl::key_down(SDL_SCANCODE_LCTRL) ||
                sl::key_down(SDL_SCANCODE_RCTRL);

            if (ctrl)
            {
                switch (key)
                {
                case sl::Event::Key::letter_z:
                    undo();
                    return;

                case sl::Event::Key::letter_y:
                    redo();
                    return;

                case sl::Event::Key::letter_s:
                    save();
                    return;

                case sl::Event::Key::letter_n:
                    new_document();
                    return;

                case sl::Event::Key::letter_c:
                    copy_selection();
                    return;

                case sl::Event::Key::letter_x:
                    cut_selection();
                    return;

                case sl::Event::Key::letter_v:
                    paste_selection();
                    return;

                default:
                    break;
                }
            }

            switch (key)
            {
            case sl::Event::Key::escape:
                cancel_selection();
                break;

            case sl::Event::Key::letter_p:
                commit_selection();
                tool = Tool::Pencil;
                break;

            case sl::Event::Key::letter_b:
                commit_selection();
                tool = Tool::Brush;
                break;

            case sl::Event::Key::letter_e:
                commit_selection();
                tool = Tool::Eraser;
                break;

            case sl::Event::Key::letter_l:
                commit_selection();
                tool = Tool::Line;
                break;

            case sl::Event::Key::letter_r:
                commit_selection();
                tool = Tool::Rectangle;
                break;

            case sl::Event::Key::letter_o:
                commit_selection();
                tool = Tool::Ellipse;
                break;

            case sl::Event::Key::letter_f:
                commit_selection();
                tool = Tool::Fill;
                break;

            case sl::Event::Key::letter_i:
                commit_selection();
                tool = Tool::Picker;
                break;

            case sl::Event::Key::letter_s:
                commit_selection();
                tool = Tool::Select;
                break;

            case sl::Event::Key::digit_1:
                brush_size = 1;
                break;

            case sl::Event::Key::digit_2:
                brush_size = 4;
                break;

            case sl::Event::Key::digit_3:
                brush_size = 8;
                break;

            case sl::Event::Key::digit_4:
                brush_size = 16;
                break;

            default:
                break;
            }
        }


        // ------------------------------------------------------------
        // Rendering
        // ------------------------------------------------------------

        void draw_button(
            int x,
            int y,
            int w,
            int h,
            const char *label,
            bool selected)
        {
            const sl::Colour fill =
                selected ? UI_SELECTED : sl::Colour{248, 248, 248};

            sl::rectfill(
                sl::screen,
                x,
                y,
                x + w,
                y + h,
                fill);

            sl::rect(
                sl::screen,
                x,
                y,
                x + w,
                y + h,
                selected ? sl::Colour{70, 100, 150} : UI_BORDER);

            sl::gprintf(
                x + 8,
                y + 7,
                UI_DARK,
                "%s",
                label);
        }

        void draw_dashed_rect(
            int x,
            int y,
            int w,
            int h,
            sl::Colour colour)
        {
            constexpr int dash = 6;

            for (int px = x; px < x + w; px += dash * 2)
            {
                sl::line(
                    sl::screen,
                    px,
                    y,
                    std::min(px + dash, x + w),
                    y,
                    colour);

                sl::line(
                    sl::screen,
                    px,
                    y + h,
                    std::min(px + dash, x + w),
                    y + h,
                    colour);
            }

            for (int py = y; py < y + h; py += dash * 2)
            {
                sl::line(
                    sl::screen,
                    x,
                    py,
                    x,
                    std::min(py + dash, y + h),
                    colour);

                sl::line(
                    sl::screen,
                    x + w,
                    py,
                    x + w,
                    std::min(py + dash, y + h),
                    colour);
            }
        }

        void draw_preview()
        {
            if (!operation_active)
                return;

            if (tool != Tool::Line &&
                tool != Tool::Rectangle &&
                tool != Tool::Ellipse)
            {
                return;
            }

            const sl::Colour colour =
                colour_for_button(operation_button);

            const int x1 = CANVAS_X + start_x;
            const int y1 = CANVAS_Y + start_y;
            const int x2 = CANVAS_X + last_x;
            const int y2 = CANVAS_Y + last_y;

            if (tool == Tool::Line)
            {
                sl::line(
                    sl::screen,
                    x1,
                    y1,
                    x2,
                    y2,
                    sl::Colour{
                        colour.red,
                        colour.green,
                        colour.blue,
                        180});
            }
            else
            {
                const Rect r =
                    normalise_rect(
                        x1,
                        y1,
                        x2,
                        y2);

                const sl::Colour preview{
                    colour.red,
                    colour.green,
                    colour.blue,
                    100
                };

                if (filled_shapes)
                {
                    if (tool == Tool::Rectangle)
                    {
                        sl::rectfill(
                            sl::screen,
                            r.x,
                            r.y,
                            r.x + r.w,
                            r.y + r.h,
                            preview);
                    }
                    else
                    {
                        sl::ellipsefill(
                            sl::screen,
                            r.x + r.w * 0.5f,
                            r.y + r.h * 0.5f,
                            r.w * 0.5f,
                            r.h * 0.5f,
                            preview);
                    }
                }
                else
                {
                    if (tool == Tool::Rectangle)
                    {
                        sl::rect(
                            sl::screen,
                            r.x,
                            r.y,
                            r.x + r.w,
                            r.y + r.h,
                            colour);
                    }
                    else
                    {
                        sl::ellipse(
                            sl::screen,
                            r.x + r.w * 0.5f,
                            r.y + r.h * 0.5f,
                            r.w * 0.5f,
                            r.h * 0.5f,
                            colour);
                    }
                }
            }
        }

        void render()
        {
            sl::clear_to_colour(
                sl::screen,
                UI_BG);

            // Canvas shadow/frame.
            sl::rectfill(
                sl::screen,
                CANVAS_X - 3,
                CANVAS_Y - 3,
                CANVAS_X + CANVAS_W + 3,
                CANVAS_Y + CANVAS_H + 3,
                sl::Colour{190, 190, 190});

            sl::draw_sprite(
                canvas,
                CANVAS_X,
                CANVAS_Y);

            // Floating selection.
            if (selection)
            {
                sl::draw_sprite(
                    selection,
                    CANVAS_X + selection_rect.x,
                    CANVAS_Y + selection_rect.y);

                draw_dashed_rect(
                    CANVAS_X + selection_rect.x,
                    CANVAS_Y + selection_rect.y,
                    selection_rect.w,
                    selection_rect.h,
                    sl::Black);
            }

            // Selection being created.
            if (selecting)
            {
                const Rect r =
                    normalise_rect(
                        CANVAS_X + start_x,
                        CANVAS_Y + start_y,
                        CANVAS_X + last_x,
                        CANVAS_Y + last_y);

                draw_dashed_rect(
                    r.x,
                    r.y,
                    r.w,
                    r.h,
                    sl::Black);
            }

            draw_preview();

            // Left panel.
            sl::rectfill(
                sl::screen,
                PANEL_X,
                8,
                PANEL_X + PANEL_W,
                650,
                UI_BG);

            sl::rect(
                sl::screen,
                PANEL_X,
                8,
                PANEL_X + PANEL_W,
                650,
                UI_BORDER);

            sl::gprintf(
                PANEL_X + 8,
                20,
                sl::Black,
                "SIMPAINT");

            // Tools.
            const Tool tools[] = {
                Tool::Pencil,
                Tool::Brush,
                Tool::Eraser,
                Tool::Line,
                Tool::Rectangle,
                Tool::Ellipse,
                Tool::Fill,
                Tool::Picker,
                Tool::Select
            };

            for (Tool t : tools)
            {
                draw_button(
                    PANEL_X,
                    tool_button_y(t),
                    PANEL_W,
                    BUTTON_H,
                    tool_name(t),
                    t == tool);
            }

            // Brush sizes.
            const int sizes[] = {1, 4, 8, 16};

            for (int i = 0; i < 4; ++i)
            {
                const int x =
                    PANEL_X + 8 + i * 36;

                draw_button(
                    x,
                    410,
                    32,
                    28,
                    std::to_string(sizes[i]).c_str(),
                    brush_size == sizes[i]);
            }

            draw_button(
                PANEL_X,
                450,
                PANEL_W,
                28,
                filled_shapes ? "Filled shapes" : "Outline shapes",
                false);

            // File controls.
            draw_button(PANEL_X,       485, 48, 28, "New", false);
            draw_button(PANEL_X + 54,  485, 48, 28, "Save", false);
            draw_button(PANEL_X + 108, 485, 48, 28, "Load", false);

            // Palette.
            sl::gprintf(
                PANEL_X + 8,
                510,
                UI_DARK,
                "Palette");

            const int palette_x = PANEL_X + 8;
            const int palette_y = 520;
            const int cell = 18;

            for (int i = 0; i < static_cast<int>(PALETTE.size()); ++i)
            {
                const int x =
                    palette_x + (i % 8) * cell;

                const int y =
                    palette_y + (i / 8) * cell;

                sl::rectfill(
                    sl::screen,
                    x,
                    y,
                    x + 16,
                    y + 16,
                    PALETTE[i]);

                sl::rect(
                    sl::screen,
                    x,
                    y,
                    x + 16,
                    y + 16,
                    UI_BORDER);
            }

            // Current colours.
            sl::gprintf(
                PANEL_X + 8,
                610,
                UI_DARK,
                "FG / BG");

            sl::rectfill(
                sl::screen,
                PANEL_X + 8,
                625,
                PANEL_X + 42,
                650,
                foreground);

            sl::rect(
                sl::screen,
                PANEL_X + 8,
                625,
                PANEL_X + 42,
                650,
                UI_DARK);

            sl::rectfill(
                sl::screen,
                PANEL_X + 34,
                638,
                PANEL_X + 68,
                663,
                background);

            sl::rect(
                sl::screen,
                PANEL_X + 34,
                638,
                PANEL_X + 68,
                663,
                UI_DARK);

            // Status bar.
            sl::rectfill(
                sl::screen,
                8,
                700,
                WINDOW_W - 8,
                752,
                sl::Colour{220, 220, 220});

            sl::rect(
                sl::screen,
                8,
                700,
                WINDOW_W - 8,
                752,
                UI_BORDER);

            sl::gprintf(
                18,
                710,
                UI_DARK,
                "%s",
                status.c_str());

            sl::gprintf(
                18,
                730,
                UI_DARK,
                "LMB=foreground  RMB=background  "
                "Ctrl-Z/Y undo/redo  Ctrl-C/X/V selection");

            sl::gprintf(
                600,
                710,
                UI_DARK,
                "Tool: %s",
                tool_name(tool));

            sl::gprintf(
                600,
                730,
                UI_DARK,
                "Pencil P  Brush B  Line L  Rect R  Ellipse O  Fill F  Select S");

            // Preview must be last so it appears over the document.
            draw_preview();
        }


        void update()
        {
            if (!operation_active &&
                !selecting &&
                !moving_selection)
            {
                return;
            }

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

            if (operation_active &&
                (tool == Tool::Line ||
                 tool == Tool::Rectangle ||
                 tool == Tool::Ellipse))
            {
                last_x = x;
                last_y = y;
            }
        }


        void handle_event(sl::Event &event)
        {
            sl::display_handle_event(event);

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
                if (!event.key_repeat())
                    key_down(event.key());
                break;

            default:
                break;
            }
        }
    };
}


int main(int argc, char **argv)
{
    PaintApp app;

    // Let simlib select OpenGL/Vulkan from --gl/--vulkan.
    sl::configure_graphics_backend_from_args(argc, argv);

    if (!sl::set_gfx_mode(
            sl::GFX_AUTODETECT_WINDOWED,
            WINDOW_W,
            WINDOW_H,
            WINDOW_W,
            WINDOW_H))
    {
        return 1;
    }

    sl::set_window_title("SimPaint");
    sl::set_vsync(true);
    sl::set_fps(60);

    app.initialise();

    if (!app.canvas)
    {
        sl::display_shutdown();
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

        sl::show_video_bitmap();
        sl::end_frame();
    }

    app.shutdown();

    sl::display_shutdown();
    sl::shutdown();

    return 0;
}

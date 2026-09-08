#include "lua_canvas.h"

#include <iostream>

namespace
{
    bool matches(simlib::Colour colour, Uint8 red, Uint8 green, Uint8 blue)
    {
        return colour.red == red && colour.green == green && colour.blue == blue;
    }
}

int main()
{
    simlib::LuaCanvas canvas;
    const simlib::LuaScriptResult result = canvas.run_text(
        "app.clear_screen(10, 20, 30)\n"
        "function on_keypress(key)\n"
        "  if key == 'space' then app.line(1, 1, 6, 1, 200, 100, 50) end\n"
        "end\n"
        "function on_ready() app.pixel(2, 2, 20, 220, 80) end\n"
        "function on_spawned(name, x, y)\n"
        "  if name == 'marker' then app.pixel(x, y, 90, 120, 255) end\n"
        "end\n"
        "assert(not app.sprite_rotated('missing', 4, 4, 45))\n"
        "assert(not app.sprite_rotated_stretched('missing', 4, 4, 45, 16, 16))\n");
    if (!result.success)
    {
        std::cerr << result.error << '\n';
        return 1;
    }
    const simlib::LuaScriptResult callback_result = canvas.dispatch_keypress("space");
    if (!callback_result.success)
    {
        std::cerr << callback_result.error << '\n';
        return 1;
    }
    const simlib::LuaScriptResult ready_result = canvas.runtime().emit("on_ready");
    const simlib::LuaScriptResult spawn_result = canvas.runtime().emit("on_spawned", "marker", 5.0f, 4.0f);
    if (!ready_result.success || !spawn_result.success)
    {
        std::cerr << (!ready_result.success ? ready_result.error : spawn_result.error) << '\n';
        return 1;
    }

    simlib::Bitmap *bitmap = simlib::create_bitmap(8, 8);
    canvas.render(bitmap);
    const bool passed =
        matches(simlib::getpixel(bitmap, 0, 0), 10, 20, 30) &&
        matches(simlib::getpixel(bitmap, 3, 1), 200, 100, 50) &&
        matches(simlib::getpixel(bitmap, 2, 2), 20, 220, 80) &&
        matches(simlib::getpixel(bitmap, 5, 4), 90, 120, 255);
    simlib::destroy_bitmap(bitmap);

    if (!passed)
    {
        std::cerr << "LuaCanvas rendered unexpected pixels.\n";
        return 1;
    }
    return 0;
}
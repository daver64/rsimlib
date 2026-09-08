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
        "app.line(1, 1, 6, 1, 200, 100, 50)\n");
    if (!result.success)
    {
        std::cerr << result.error << '\n';
        return 1;
    }

    simlib::Bitmap *bitmap = simlib::create_bitmap(8, 8);
    canvas.render(bitmap);
    const bool passed =
        matches(simlib::getpixel(bitmap, 0, 0), 10, 20, 30) &&
        matches(simlib::getpixel(bitmap, 3, 1), 200, 100, 50);
    simlib::destroy_bitmap(bitmap);

    if (!passed)
    {
        std::cerr << "LuaCanvas rendered unexpected pixels.\n";
        return 1;
    }
    return 0;
}
#include "lua_canvas.h"

#include <iostream>

namespace
{
    bool matches(sl::Colour colour, Uint8 red, Uint8 green, Uint8 blue)
    {
        return colour.red == red && colour.green == green && colour.blue == blue;
    }
}

int main()
{
    sl::LuaCanvas canvas;
    const sl::LuaScriptResult result = canvas.run_text(
        "app.clear_screen(10, 20, 30)\n"
        "function on_keypress(key)\n"
        "  if key == 'space' then app.line(1, 1, 6, 1, 200, 100, 50) end\n"
        "end\n"
        "function on_ready() app.pixel(2, 2, 20, 220, 80) end\n"
        "function on_spawned(name, x, y)\n"
        "  if name == 'marker' then app.pixel(x, y, 90, 120, 255) end\n"
        "end\n"
        "assert(not app.sprite_rotated('missing', 4, 4, 45))\n"
        "assert(not app.sprite_rotated_stretched('missing', 4, 4, 45, 16, 16))\n"
        "assert(not app.load_sound('bad', '../outside.wav'))\n"
        "assert(app.play_sound('missing') == 0)\n"
        "assert(not app.unload_sound('missing'))\n"
        "assert(not app.load_music('bad', '../outside.ogg'))\n"
        "assert(not app.play_music('missing'))\n"
        "assert(not app.unload_music('missing'))\n"
        "local world = physics.create_world(0, 100)\n"
        "assert(world > 0)\n"
        "local floor = physics.create_body(world, 'static', 0, 100)\n"
        "local body = physics.create_body(world, 'dynamic', 0, 20)\n"
        "assert(physics.add_box(floor, 100, 10))\n"
        "assert(physics.add_circle(body, 8))\n"
        "assert(physics.set_velocity(body, 0, 0))\n"
        "assert(physics.step(world, 0.1))\n"
        "local position = physics.position(body)\n"
        "assert(position.y > 20)\n"
        "assert(type(physics.contacts(world)) == 'table')\n"
        "physics.destroy_world(world)\n"
        "assert(not physics.step(world, 0.1))\n");
    if (!result.success)
    {
        std::cerr << result.error << '\n';
        return 1;
    }
    const sl::LuaScriptResult callback_result = canvas.dispatch_keypress("space");
    if (!callback_result.success)
    {
        std::cerr << callback_result.error << '\n';
        return 1;
    }
    const sl::LuaScriptResult ready_result = canvas.runtime().emit("on_ready");
    const sl::LuaScriptResult spawn_result = canvas.runtime().emit("on_spawned", "marker", 5.0f, 4.0f);
    if (!ready_result.success || !spawn_result.success)
    {
        std::cerr << (!ready_result.success ? ready_result.error : spawn_result.error) << '\n';
        return 1;
    }

    sl::Bitmap *bitmap = sl::create_bitmap(8, 8);
    canvas.render(bitmap);
    const bool passed =
        matches(sl::getpixel(bitmap, 0, 0), 10, 20, 30) &&
        matches(sl::getpixel(bitmap, 3, 1), 200, 100, 50) &&
        matches(sl::getpixel(bitmap, 2, 2), 20, 220, 80) &&
        matches(sl::getpixel(bitmap, 5, 4), 90, 120, 255);
    sl::destroy_bitmap(bitmap);

    if (!passed)
    {
        std::cerr << "LuaCanvas rendered unexpected pixels.\n";
        return 1;
    }
    return 0;
}
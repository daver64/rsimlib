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
        "assert(not physics.step(world, 0.1))\n"
        "assert(type(system.time_ms()) == 'number')\n"
        "assert(app.create_render_target('offscreen', 64, 64))\n"
        "assert(app.sprite('offscreen', 0, 0))\n"
        "assert(app.destroy_render_target('offscreen'))\n"
        "assert(terrain.create('hills', {shape = 'flat', size = 2048, source = {type = 'noise', seed = 7, amplitude = 100}}))\n"
        "assert(not terrain.create('hills', {}))\n"
        "assert(not terrain.create('bad', {shape = 'torus'}))\n"
        "local h = terrain.height_at('hills', 100, 200)\n"
        "assert(type(h) == 'number' and h == terrain.height_at('hills', 100, 200))\n"
        "assert(math.abs(terrain.normal_at('hills', 100, 200).y) <= 1.0001)\n"
        "assert(terrain.altitude_of('hills', 100, h + 5, 200) > 4.9)\n"
        "local hit = terrain.raycast('hills', 100, h + 500, 200, 0, -1, 0, 2000)\n"
        "assert(hit and math.abs(hit.y - h) < 0.5 and hit.normal.y > 0)\n"
        "assert(not terrain.raise('hills', 0, 0, 10, 5))\n"
        "assert(terrain.create('pit', {size = 256, source = {type = 'grid', width = 65, world_width = 256, height = 10}}))\n"
        "assert(terrain.raise('pit', 128, 128, 20, 8))\n"
        "assert(terrain.height_at('pit', 128, 128) > 17)\n"
        "assert(terrain.flatten('pit', 128, 128, 20, 0))\n"
        "assert(terrain.height_at('pit', 128, 128) < 1)\n"
        "assert(terrain.create('moon', {shape = 'sphere', radius = 1737000, source = {type = 'flat', height = 100}}))\n"
        "assert(terrain.altitude_of('moon', 0, 1737100 + 50, 0) > 49)\n"
        "assert(terrain.height_at_direction('moon', 0, 1, 0) == 100)\n"
        "assert(not terrain.set_atlas('hills', 'missing'))\n"
        "assert(terrain.add_clutter('hills', {mesh = 'conifer', density = 0.004, max_slope = 30}) == 1)\n"
        "assert(terrain.add_clutter('hills', {mesh = 'rock', size = 1.5}) == 2)\n"
        "assert(not terrain.add_clutter('hills', {mesh = 'sofa'}))\n"
        "assert(not terrain.add_clutter('nowhere', {}))\n"
        "assert(terrain.clear_clutter('hills') and not terrain.clear_clutter('nowhere'))\n"
        "assert(terrain.view('hills', 0, 0, 64, 48, {0, 300, 0}, {0, 250, 100}, {environment = {fog_density = 1e-4, clouds = {volumetric = true, coverage = 0.6, wind = {5, 2}}}}))\n"
        "assert(terrain.stats('hills').visible_chunks == 0)\n"
        "assert(terrain.destroy('hills') and terrain.destroy('pit') and terrain.destroy('moon'))\n"
        "assert(not terrain.destroy('hills') and terrain.height_at('hills', 0, 0) == nil)\n"
        "fx.shake(5.0, 0.2)\n"
        "assert(fx.shake_active())\n"
        "fx.clear_shake()\n"
        "assert(not fx.shake_active())\n");
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

    const sl::LuaScriptResult world_result = canvas.run_text(R"(
        assert(fluid.create('f', 16, 16))
        assert(not fluid.create('f', 16, 16))
        assert(fluid.set_cell('f', 8, 2, 'water', 1.0))
        assert(not fluid.set_cell('f', 8, 2, 'cheese'))
        fluid.step('f', 20)
        local cell = fluid.get_cell('f', 8, 2)
        assert(cell and cell.element == 'empty')
        assert(fluid.get_cell('f', 99, 99) == nil)
        assert(fluid.destroy('f'))

        assert(voxel.create_world('w'))
        assert(voxel.define_block(1, {100, 100, 100}))
        assert(voxel.fill('w', 0, 0, 0, 15, 0, 15, 1))
        assert(not voxel.fill('w', 0, 0, 0, 5000, 5000, 5000, 1))
        assert(voxel.get_block('w', 3, 0, 3) == 1)
        local hit = voxel.raycast('w', 3.5, 5.5, 3.5, 0, -1, 0, 20)
        assert(hit and hit.y == 0 and hit.place_y == 1)
        assert(voxel.raycast('w', 3.5, 5.5, 3.5, 0, 1, 0, 20) == nil)

        assert(fluid3d.create('s', 16, 12, 16, 0, 0, 0))
        assert(fluid3d.import('s', 'w') == true)
        assert(fluid3d.set_cell('s', 8, 6, 8, 'water'))
        local before = fluid3d.count('s')
        fluid3d.step('s', 30)
        assert(fluid3d.count('s') == before)
        local landed = false
        for y = 0, 3 do
            local c = fluid3d.get_cell('s', 8, y, 8)
            if c and c.element == 'water' then landed = true end
        end
        assert(landed)
        assert(fluid3d.export('s', 'w') > 0)
        assert(tilemap.load('t', 'missing.tmj') == false)
        assert(tilemap.get('nope', 0, 0, 0) == 0)
        assert(fluid3d.destroy('s'))
        assert(not voxel.set_atlas('w', 'missing', 4, 4))
        assert(not voxel.set_atlas('nope', 'missing', 4, 4))
        assert(not app.set_normal_map('missing', 'also_missing'))
        lighting.enable(0.3, 40)
        assert(lighting.add(10, 10, 50, 1, 255, 255, 255) == 1)
        assert(lighting.add(20, 20, 50, 1, 255, 255, 255) == 2)
        lighting.set(1, 30, 30)
        lighting.clear()
        lighting.disable()
        assert(voxel.destroy_world('w'))
    )");
    if (!world_result.success)
    {
        std::cerr << "world bindings: " << world_result.error << '\n';
        return 1;
    }

    sl::LuaLimits limits;
    limits.max_instructions = 2'000'000;
    limits.max_memory_bytes = 16u * 1024u * 1024u;
    canvas.runtime().set_limits(limits);
    const sl::LuaScriptResult loop_result = canvas.run_text("while true do end");
    const sl::LuaScriptResult memory_result = canvas.run_text("local s = string.rep('x', 1024) for i = 1, 20 do s = s .. s end");
    const sl::LuaScriptResult after_result = canvas.run_text("local n = 0 for i = 1, 1000 do n = n + i end assert(n == 500500)");
    if (loop_result.success || memory_result.success || !after_result.success)
    {
        std::cerr << "Lua resource limits were not enforced: " << loop_result.error << " | " << memory_result.error
                  << " | " << after_result.error << '\n';
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
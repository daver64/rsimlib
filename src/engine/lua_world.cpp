/** @file
 * @brief Lua bindings for tile maps, fluid simulations and voxel worlds.
 */

#include "lua_world.h"

#include "draw.h"
#include "fluid.h"
#include "fluid3d.h"
#include "surface3d.h"
#include "tilemap.h"
#include "voxel.h"

#include <sol/sol.hpp>

#include <algorithm>
#include <memory>
#include <tuple>
#include <unordered_map>

namespace sl
{
    namespace
    {
        constexpr int max_fluid_2d_size = 1024;
        constexpr int max_fluid_3d_size = 128;
        constexpr long long max_fill_blocks = 4'000'000;
        constexpr int max_view_size = 4096;

        struct ElementName
        {
            const char *name;
            FluidElement element;
            Colour colour;
        };

        constexpr ElementName element_names[] = {
            {"empty", FluidElement::empty, {0, 0, 0, 0}},
            {"solid", FluidElement::solid, {110, 110, 120, 255}},
            {"wood", FluidElement::wood, {120, 80, 40, 255}},
            {"sand", FluidElement::sand, {220, 190, 110, 255}},
            {"water", FluidElement::water, {40, 100, 230, 255}},
            {"oil", FluidElement::oil, {90, 60, 30, 255}},
            {"acid", FluidElement::acid, {120, 230, 60, 255}},
            {"lava", FluidElement::lava, {240, 90, 20, 255}},
            {"gunpowder", FluidElement::gunpowder, {70, 70, 70, 255}},
            {"plant", FluidElement::plant, {50, 170, 60, 255}},
            {"pump", FluidElement::pump, {200, 60, 200, 255}},
            {"fire", FluidElement::fire, {255, 160, 30, 255}},
            {"smoke", FluidElement::smoke, {110, 110, 110, 200}},
            {"steam", FluidElement::steam, {210, 220, 235, 180}},
        };

        std::optional<FluidElement> element_from_name(const std::string &name)
        {
            for (const ElementName &entry : element_names)
            {
                if (name == entry.name)
                    return entry.element;
            }
            return std::nullopt;
        }

        const ElementName &element_entry(FluidElement element)
        {
            return element_names[std::min<std::size_t>(static_cast<std::size_t>(element), std::size(element_names) - 1)];
        }

        sol::table cell_table(sol::state_view lua, const FluidCell &cell)
        {
            sol::table result = lua.create_table();
            result["element"] = element_entry(cell.type).name;
            result["mass"] = cell.mass;
            result["temp"] = cell.temp;
            return result;
        }

        Colour colour_from_table(sol::optional<sol::table> table, Colour fallback)
        {
            if (!table)
                return fallback;
            auto channel = [&](int index, Uint8 value)
            { return static_cast<Uint8>(std::clamp(table->get_or(index, static_cast<int>(value)), 0, 255)); };
            return {channel(1, fallback.red), channel(2, fallback.green), channel(3, fallback.blue), channel(4, fallback.alpha)};
        }

        template <typename T>
        T *find_in(const std::unordered_map<std::string, T *> &map, const std::string &id)
        {
            const auto it = map.find(id);
            return it == map.end() ? nullptr : it->second;
        }
    }

    struct LuaWorld::Implementation
    {
        struct TileView
        {
            float x = 0.0f;
            float y = 0.0f;
            int width = 0;
            int height = 0;
        };
        struct FluidView
        {
            float x = 0.0f;
            float y = 0.0f;
            float scale = 2.0f;
        };
        struct VoxelView
        {
            int x = 0;
            int y = 0;
            int width = 0;
            int height = 0;
            Camera camera;
            Colour sky{18, 22, 36, 255};
            Surface3D *surface = nullptr;
        };

        std::unordered_map<std::string, TileMap *> tilemaps;
        std::unordered_map<std::string, TileView> tile_views;
        std::unordered_map<std::string, FluidSimulation *> fluids;
        std::unordered_map<std::string, FluidView> fluid_views;
        std::unordered_map<std::string, FluidSimulation3D *> fluids_3d;
        std::unordered_map<std::string, std::unique_ptr<VoxelWorld>> worlds;
        std::unordered_map<std::string, VoxelView> voxel_views;
        FluidVoxelBlocks fluid_blocks;

        VoxelWorld *world(const std::string &id) const
        {
            const auto it = worlds.find(id);
            return it == worlds.end() ? nullptr : it->second.get();
        }

        void destroy_view(const std::string &id)
        {
            const auto it = voxel_views.find(id);
            if (it != voxel_views.end())
            {
                if (it->second.surface)
                    destroy_surface_3d(it->second.surface);
                voxel_views.erase(it);
            }
        }

        void clear()
        {
            for (auto &entry : tilemaps)
                destroy_tilemap(entry.second);
            for (auto &entry : fluids)
                destroy_fluid_simulation(entry.second);
            for (auto &entry : fluids_3d)
                destroy_fluid_simulation_3d(entry.second);
            for (auto &entry : voxel_views)
            {
                if (entry.second.surface)
                    destroy_surface_3d(entry.second.surface);
            }
            tilemaps.clear();
            tile_views.clear();
            fluids.clear();
            fluid_views.clear();
            fluids_3d.clear();
            voxel_views.clear();
            worlds.clear();
        }

        void bind_tilemap(sol::table table, const PathResolver &resolver, const DrawRequest &request);
        void bind_fluid(sol::table table, const DrawRequest &request);
        void bind_fluid_3d(sol::table table);
        void bind_voxel(sol::table table, const DrawRequest &request);
    };

    void LuaWorld::Implementation::bind_tilemap(sol::table table, const PathResolver &resolver, const DrawRequest &request)
    {
        table.set_function("load", [this, resolver](const std::string &id, const std::string &path)
                           {
            const auto resolved = resolver(path);
            if (id.empty() || !resolved || tilemaps.count(id))
                return false;
            TileMap *map = load_tiled_map(resolved->string());
            if (!map)
                return false;
            tilemaps.emplace(id, map);
            return true; });
        table.set_function("unload", [this](const std::string &id)
                           {
            const auto it = tilemaps.find(id);
            if (it == tilemaps.end())
                return false;
            destroy_tilemap(it->second);
            tilemaps.erase(it);
            tile_views.erase(id);
            return true; });
        table.set_function("info", [this](sol::this_state state, const std::string &id) -> sol::object
                           {
            sol::state_view lua(state);
            const TileMap *map = find_in(tilemaps, id);
            if (!map)
                return sol::nil;
            sol::table info = lua.create_table();
            info["width"] = map->width;
            info["height"] = map->height;
            info["tile_width"] = map->tile_width;
            info["tile_height"] = map->tile_height;
            info["layers"] = static_cast<int>(map->layers.size());
            info["isometric"] = map->orientation == TileMapOrientation::isometric;
            return info; });
        table.set_function("get", [this](const std::string &id, int layer, int column, int row)
                           { return static_cast<lua_Integer>(tilemap_get(find_in(tilemaps, id), layer, column, row)); });
        table.set_function("set", [this](const std::string &id, int layer, int column, int row, lua_Integer cell)
                           {
            TileMap *map = find_in(tilemaps, id);
            return map && cell >= 0 && tilemap_set(map, layer, column, row, static_cast<std::uint32_t>(cell)); });
        table.set_function("is_solid", [this](const std::string &id, int column, int row, sol::optional<int> layer)
                           { return tilemap_is_solid(find_in(tilemaps, id), column, row, layer.value_or(-1)); });
        table.set_function("rect_blocked", [this](const std::string &id, float x, float y, float width, float height,
                                                  sol::optional<int> layer)
                           { return tilemap_rect_blocked(find_in(tilemaps, id), x, y, width, height, layer.value_or(-1)); });
        table.set_function("set_solid", [this](const std::string &id, lua_Integer gid, bool solid)
                           {
            TileMap *map = find_in(tilemaps, id);
            if (!map || gid <= 0)
                return false;
            tilemap_set_solid(map, static_cast<std::uint32_t>(gid), solid);
            return true; });
        table.set_function("property", [this](const std::string &id, lua_Integer gid, const std::string &key)
                           {
            const TileMap *map = find_in(tilemaps, id);
            return map && gid > 0 ? tilemap_tile_property(map, static_cast<std::uint32_t>(gid), key) : std::string{}; });
        table.set_function("world_to_tile", [this](const std::string &id, float x, float y)
                           {
            int column = 0, row = 0;
            if (const TileMap *map = find_in(tilemaps, id))
                tilemap_world_to_tile(map, x, y, column, row);
            return std::make_tuple(column, row); });
        table.set_function("tile_to_world", [this](const std::string &id, int column, int row)
                           {
            float x = 0.0f, y = 0.0f;
            if (const TileMap *map = find_in(tilemaps, id))
                tilemap_tile_to_world(map, column, row, x, y);
            return std::make_tuple(x, y); });
        table.set_function("update", [this](const std::string &id, float dt)
                           {
            TileMap *map = find_in(tilemaps, id);
            if (map)
                update_tilemap(map, dt);
            return map != nullptr; });
        table.set_function("find_object", [this](sol::this_state state, const std::string &id, const std::string &name) -> sol::object
                           {
            sol::state_view lua(state);
            const TileObject *object = tilemap_find_object(find_in(tilemaps, id), name);
            if (!object)
                return sol::nil;
            sol::table result = lua.create_table();
            result["name"] = object->name;
            result["type"] = object->type;
            result["x"] = object->x;
            result["y"] = object->y;
            result["width"] = object->width;
            result["height"] = object->height;
            sol::table properties = lua.create_table();
            for (const auto &[key, value] : object->properties)
                properties[key] = value;
            result["properties"] = properties;
            return result; });
        table.set_function("draw", [this, request](const std::string &id, float camera_x, float camera_y,
                                                   sol::optional<int> width, sol::optional<int> height)
                           {
            if (!find_in(tilemaps, id))
                return false;
            tile_views[id] = {camera_x, camera_y, std::max(0, width.value_or(0)), std::max(0, height.value_or(0))};
            request(LuaWorldDraw::tilemap, id);
            return true; });
    }

    void LuaWorld::Implementation::bind_fluid(sol::table table, const DrawRequest &request)
    {
        table.set_function("create", [this](const std::string &id, int width, int height)
                           {
            if (id.empty() || fluids.count(id) || width < 3 || height < 3 || width > max_fluid_2d_size ||
                height > max_fluid_2d_size)
                return false;
            FluidSimulation *simulation = create_fluid_simulation(width, height);
            if (!simulation)
                return false;
            fluids.emplace(id, simulation);
            return true; });
        table.set_function("destroy", [this](const std::string &id)
                           {
            const auto it = fluids.find(id);
            if (it == fluids.end())
                return false;
            destroy_fluid_simulation(it->second);
            fluids.erase(it);
            fluid_views.erase(id);
            return true; });
        table.set_function("clear", [this](const std::string &id)
                           {
            FluidSimulation *simulation = find_in(fluids, id);
            if (simulation)
                fluid_clear(simulation);
            return simulation != nullptr; });
        table.set_function("step", [this](const std::string &id, sol::optional<int> steps)
                           {
            FluidSimulation *simulation = find_in(fluids, id);
            if (!simulation)
                return false;
            for (int i = 0; i < std::clamp(steps.value_or(1), 1, 100); ++i)
                fluid_step(simulation);
            return true; });
        table.set_function("set_cell", [this](const std::string &id, int x, int y, const std::string &element,
                                              sol::optional<float> mass, sol::optional<float> temp)
                           {
            FluidSimulation *simulation = find_in(fluids, id);
            const auto type = element_from_name(element);
            return simulation && type && fluid_set_cell(simulation, x, y, *type, mass.value_or(type == FluidElement::empty ? 0.0f : 1.0f),
                                                        temp.value_or(20.0f)); });
        table.set_function("get_cell", [this](sol::this_state state, const std::string &id, int x, int y) -> sol::object
                           {
            const FluidSimulation *simulation = find_in(fluids, id);
            if (!simulation || !fluid_in_bounds(simulation, x, y))
                return sol::nil;
            return cell_table(sol::state_view(state), fluid_get_cell(simulation, x, y)); });
        table.set_function("size", [this](const std::string &id)
                           {
            const FluidSimulation *simulation = find_in(fluids, id);
            return std::make_tuple(fluid_width(simulation), fluid_height(simulation)); });
        table.set_function("draw", [this, request](const std::string &id, float x, float y, sol::optional<float> scale)
                           {
            if (!find_in(fluids, id))
                return false;
            fluid_views[id] = {x, y, std::clamp(scale.value_or(2.0f), 0.25f, 64.0f)};
            request(LuaWorldDraw::fluid, id);
            return true; });
    }

    void LuaWorld::Implementation::bind_fluid_3d(sol::table table)
    {
        table.set_function("create", [this](const std::string &id, int width, int height, int depth,
                                            sol::optional<int> origin_x, sol::optional<int> origin_y, sol::optional<int> origin_z)
                           {
            const int largest = std::max({width, height, depth});
            if (id.empty() || fluids_3d.count(id) || std::min({width, height, depth}) < 3 || largest > max_fluid_3d_size)
                return false;
            FluidSimulation3D *simulation = create_fluid_simulation_3d(width, height, depth);
            if (!simulation)
                return false;
            fluid3d_set_origin(simulation, origin_x.value_or(0), origin_y.value_or(0), origin_z.value_or(0));
            fluid3d_register_blocks(fluid_blocks);
            fluids_3d.emplace(id, simulation);
            return true; });
        table.set_function("destroy", [this](const std::string &id)
                           {
            const auto it = fluids_3d.find(id);
            if (it == fluids_3d.end())
                return false;
            destroy_fluid_simulation_3d(it->second);
            fluids_3d.erase(it);
            return true; });
        table.set_function("clear", [this](const std::string &id)
                           {
            FluidSimulation3D *simulation = find_in(fluids_3d, id);
            if (simulation)
                fluid3d_clear(simulation);
            return simulation != nullptr; });
        table.set_function("step", [this](const std::string &id, sol::optional<int> steps)
                           {
            FluidSimulation3D *simulation = find_in(fluids_3d, id);
            if (!simulation)
                return false;
            for (int i = 0; i < std::clamp(steps.value_or(1), 1, 100); ++i)
                fluid3d_step(simulation);
            return true; });
        table.set_function("set_budget", [this](const std::string &id, int blocks)
                           {
            FluidSimulation3D *simulation = find_in(fluids_3d, id);
            if (simulation)
                simulation->max_blocks_per_step = std::max(0, blocks);
            return simulation != nullptr; });
        table.set_function("set_cell", [this](const std::string &id, int x, int y, int z, const std::string &element,
                                              sol::optional<float> mass)
                           {
            FluidSimulation3D *simulation = find_in(fluids_3d, id);
            const auto type = element_from_name(element);
            return simulation && type && fluid3d_set_cell(simulation, x, y, z, *type, mass.value_or(type == FluidElement::empty ? 0.0f : 1.0f)); });
        table.set_function("get_cell", [this](sol::this_state state, const std::string &id, int x, int y, int z) -> sol::object
                           {
            const FluidSimulation3D *simulation = find_in(fluids_3d, id);
            if (!simulation || !fluid3d_in_bounds(simulation, x, y, z))
                return sol::nil;
            return cell_table(sol::state_view(state), fluid3d_get_cell(simulation, x, y, z)); });
        table.set_function("count", [this](const std::string &id)
                           {
            const FluidSimulation3D *simulation = find_in(fluids_3d, id);
            return simulation ? static_cast<lua_Integer>(fluid3d_count_non_empty(simulation)) : 0; });
        table.set_function("import", [this](const std::string &id, const std::string &world_id)
                           {
            FluidSimulation3D *simulation = find_in(fluids_3d, id);
            const VoxelWorld *voxels = world(world_id);
            if (!simulation || !voxels)
                return false;
            fluid3d_import_from_voxels(simulation, *voxels, fluid_blocks);
            return true; });
        table.set_function("export", [this](const std::string &id, const std::string &world_id)
                           {
            FluidSimulation3D *simulation = find_in(fluids_3d, id);
            VoxelWorld *voxels = world(world_id);
            return simulation && voxels ? static_cast<lua_Integer>(fluid3d_export_to_voxels(simulation, *voxels, fluid_blocks)) : 0; });
        table.set_function("move_region", [this](const std::string &id, const std::string &world_id, int x, int y, int z)
                           {
            FluidSimulation3D *simulation = find_in(fluids_3d, id);
            VoxelWorld *voxels = world(world_id);
            if (!simulation || !voxels)
                return false;
            fluid3d_move_region(simulation, *voxels, x, y, z, fluid_blocks);
            return true; });
    }

    void LuaWorld::Implementation::bind_voxel(sol::table table, const DrawRequest &request)
    {
        table.set_function("create_world", [this](const std::string &id)
                           {
            if (id.empty() || worlds.count(id))
                return false;
            worlds.emplace(id, std::make_unique<VoxelWorld>());
            return true; });
        table.set_function("destroy_world", [this](const std::string &id)
                           {
            destroy_view(id);
            return worlds.erase(id) > 0; });
        table.set_function("define_block", [](int block, sol::table tint, sol::optional<sol::table> options)
                           {
            if (block < 1 || block > 255)
                return false;
            BlockDefinition definition;
            definition.tint = colour_from_table(tint, {255, 255, 255, 255});
            if (options)
            {
                definition.is_solid = options->get_or("solid", true);
                definition.is_transparent = options->get_or("transparent", false);
                definition.light_emission = static_cast<std::uint8_t>(std::clamp(options->get_or("emission", 0), 0, 15));
            }
            register_block_definition(static_cast<BlockId>(block), definition);
            return true; });
        table.set_function("set_block", [this](const std::string &id, int x, int y, int z, int block)
                           {
            VoxelWorld *voxels = world(id);
            return voxels && block >= 0 && block <= 255 && voxels->set_block(x, y, z, static_cast<BlockId>(block)); });
        table.set_function("get_block", [this](const std::string &id, int x, int y, int z)
                           {
            const VoxelWorld *voxels = world(id);
            return voxels ? static_cast<int>(voxels->get_block(x, y, z)) : 0; });
        table.set_function("fill", [this](const std::string &id, int x1, int y1, int z1, int x2, int y2, int z2, int block)
                           {
            VoxelWorld *voxels = world(id);
            if (!voxels || block < 0 || block > 255)
                return false;
            const long long volume = (static_cast<long long>(std::abs(x2 - x1)) + 1) * (std::abs(y2 - y1) + 1) *
                                     (std::abs(z2 - z1) + 1);
            if (volume > max_fill_blocks)
                return false;
            for (int z = std::min(z1, z2); z <= std::max(z1, z2); ++z)
                for (int y = std::min(y1, y2); y <= std::max(y1, y2); ++y)
                    for (int x = std::min(x1, x2); x <= std::max(x1, x2); ++x)
                        voxels->set_block(x, y, z, static_cast<BlockId>(block));
            return true; });
        table.set_function("update_lighting", [this](const std::string &id)
                           {
            VoxelWorld *voxels = world(id);
            if (voxels)
                voxels->update_lighting();
            return voxels != nullptr; });
        table.set_function("stats", [this](sol::this_state state, const std::string &id) -> sol::object
                           {
            const VoxelWorld *voxels = world(id);
            if (!voxels)
                return sol::nil;
            sol::table stats = sol::state_view(state).create_table();
            stats["chunks"] = static_cast<lua_Integer>(voxels->chunk_count());
            stats["triangles"] = voxels->total_triangles();
            return stats; });
        table.set_function("raycast", [this](sol::this_state state, const std::string &id, float ox, float oy, float oz,
                                             float dx, float dy, float dz, sol::optional<float> max_distance) -> sol::object
                           {
            const VoxelWorld *voxels = world(id);
            const glm::vec3 direction(dx, dy, dz);
            if (!voxels || glm::dot(direction, direction) < 1e-12f)
                return sol::nil;
            const VoxelRaycastResult hit = voxels->raycast({ox, oy, oz}, glm::normalize(direction),
                                                           std::clamp(max_distance.value_or(8.0f), 0.0f, 512.0f));
            if (!hit.hit)
                return sol::nil;
            sol::table result = sol::state_view(state).create_table();
            result["x"] = hit.hit_block.x;
            result["y"] = hit.hit_block.y;
            result["z"] = hit.hit_block.z;
            result["place_x"] = hit.place_block.x;
            result["place_y"] = hit.place_block.y;
            result["place_z"] = hit.place_block.z;
            result["face"] = static_cast<int>(hit.hit_face);
            result["distance"] = hit.distance;
            return result; });
        table.set_function("view", [this, request](const std::string &id, int x, int y, int width, int height,
                                                   sol::table eye, sol::table target, sol::optional<float> field_of_view,
                                                   sol::optional<sol::table> sky)
                           {
            if (!world(id) || width < 1 || height < 1 || width > max_view_size || height > max_view_size)
                return false;
            VoxelView &view = voxel_views[id];
            if (view.surface && (view.width != width || view.height != height))
            {
                destroy_surface_3d(view.surface);
                view.surface = nullptr;
            }
            view.x = x;
            view.y = y;
            view.width = width;
            view.height = height;
            view.camera.position = {eye.get_or(1, 0.0f), eye.get_or(2, 0.0f), eye.get_or(3, 0.0f)};
            view.camera.target = {target.get_or(1, 0.0f), target.get_or(2, 0.0f), target.get_or(3, 0.0f)};
            view.camera.field_of_view = std::clamp(field_of_view.value_or(60.0f), 10.0f, 120.0f);
            view.camera.near_plane = 0.1f;
            view.camera.far_plane = 500.0f;
            view.sky = colour_from_table(sky, view.sky);
            request(LuaWorldDraw::voxel_view, id);
            return true; });
    }

    LuaWorld::LuaWorld()
        : implementation_(new Implementation)
    {
    }

    LuaWorld::~LuaWorld()
    {
        clear();
        delete implementation_;
    }

    void LuaWorld::bind(sol::state &lua, PathResolver resolver, DrawRequest request)
    {
        implementation_->bind_tilemap(lua.create_named_table("tilemap"), resolver, request);
        implementation_->bind_fluid(lua.create_named_table("fluid"), request);
        implementation_->bind_fluid_3d(lua.create_named_table("fluid3d"));
        implementation_->bind_voxel(lua.create_named_table("voxel"), request);
    }

    void LuaWorld::clear()
    {
        implementation_->clear();
    }

    void LuaWorld::draw(LuaWorldDraw kind, const std::string &id, Bitmap *target) const
    {
        Implementation &state = *implementation_;
        switch (kind)
        {
        case LuaWorldDraw::tilemap:
        {
            const TileMap *map = find_in(state.tilemaps, id);
            const auto view = state.tile_views.find(id);
            if (map && view != state.tile_views.end())
                draw_tilemap(map, view->second.x, view->second.y, view->second.width, view->second.height);
            break;
        }
        case LuaWorldDraw::fluid:
        {
            const FluidSimulation *simulation = find_in(state.fluids, id);
            const auto view = state.fluid_views.find(id);
            if (!simulation || view == state.fluid_views.end())
                break;
            const float scale = view->second.scale;
            for (int y = 0; y < simulation->height; ++y)
            {
                for (int x = 0; x < simulation->width; ++x)
                {
                    const FluidCell &cell = simulation->cells[static_cast<std::size_t>(y) * simulation->width + x];
                    if (cell.type == FluidElement::empty)
                        continue;
                    const float left = view->second.x + static_cast<float>(x) * scale;
                    const float top = view->second.y + static_cast<float>(y) * scale;
                    rectfill(target, left, top, left + scale, top + scale, element_entry(cell.type).colour);
                }
            }
            break;
        }
        case LuaWorldDraw::voxel_view:
        {
            VoxelWorld *voxels = state.world(id);
            const auto view = state.voxel_views.find(id);
            if (!voxels || view == state.voxel_views.end())
                break;
            Implementation::VoxelView &voxel_view = view->second;
            if (!voxel_view.surface)
                voxel_view.surface = create_surface_3d(voxel_view.width, voxel_view.height);
            if (!voxel_view.surface)
                break;
            voxels->update_dirty_chunks();
            if (begin_surface_3d(voxel_view.surface, voxel_view.camera))
            {
                clear_surface_3d(voxel_view.sky);
                Material material;
                material.cull = CullMode::none;
                voxels->draw(material);
                end_surface_3d();
                draw_sprite(surface_3d_bitmap(voxel_view.surface), static_cast<float>(voxel_view.x),
                            static_cast<float>(voxel_view.y));
            }
            break;
        }
        }
    }
}

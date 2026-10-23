# Props catalogue

Every part in [`props3d.h`](../include/props3d.h) (plus the conifer and rock in [`clutter.h`](../include/clutter.h)), with its signature, size and how to put it in a scene. All parts are generated in code, so there are no asset files. See `exprops` for a lit gallery of all of them, and the lesson [Building a scene from parts](tutorial/3d-06-building-a-scene-from-parts.md).

## Instantiating a prop

Each `make_*_data()` builder returns a `sl::MeshData` (vertices and indices on the CPU). Arguments are optional; the defaults give a sensible part. The same arguments always give the same geometry, and `seed` varies colours, which windows are lit and small details.

**One part, drawn once**

```cpp
#include <sl.h>

sl::MeshData data = sl::make_house_data();                       // defaults
sl::Mesh *house = sl::create_mesh(data.vertices, data.indices);  // upload to the GPU

// each frame, inside begin_surface_3d()
sl::draw_mesh(house, sl::model_matrix({10, 0, 5}, {0, 0.5f, 0}));

sl::destroy_mesh(house);                                         // at shutdown
```

**Many copies, one draw call** (static scenery): bake the instances into a single mesh.

```cpp
std::vector<sl::Instance3D> spots = {
    {{-13, 0, 0}, 1.5708f},                 // position, yaw (radians)
    {{13, 0, 8}, -1.5708f, 1.2f},           // optional scale, up vector and tint follow
};
sl::Mesh *houses = sl::bake_instances(sl::make_house_data(), spots);
sl::draw_mesh(houses, glm::mat4(1.0f));
```

**Assembled parts**: build a bigger object by moving parts and merging them.

```cpp
sl::MeshData rocket = sl::make_rocket_stage_data(2.0f, 12.0f);
sl::MeshData nose = sl::make_payload_fairing_data(2.0f, 6.0f);
sl::transform_mesh_data(nose, glm::translate(glm::mat4(1.0f), {0, 12.0f, 0}));
sl::append_mesh_data(rocket, nose);
sl::Mesh *mesh = sl::create_mesh(rocket.vertices, rocket.indices);
```

Use `draw_mesh_instanced()` instead of baking when instances move or change.

## Conventions

- Metres. +Y is up and a part stands on `y = 0`, centred on x and z.
- The front (door, bonnet, nose) faces +Z. Fences, walls and containers run along X; roads run along Z.
- `Instance3D::yaw` turns a part's front to `(sin yaw, cos yaw)`: 0 faces +Z, pi/2 faces +X, pi faces -Z.
- Windows and details are baked into the vertex colours (no textures), and parts are static: wheels, propellers and the like are not separate.
- Colours are `sl::Colour` values such as `{214, 196, 160, 255}`.

Each entry lists the arguments in order with their defaults.

## Buildings

| Builder | Arguments (defaults) | Notes |
|---------|----------------------|-------|
| `make_house_data` | `width 6, depth 5, height 3, wall {214,196,160}, roof {150,64,48}, seed 1` | Gable-roof cottage with chimney, door and windows; the ridge runs along X. |
| `make_office_data` | `width 14, depth 10, floors 4, wall {170,172,176}, seed 1` | Flat-roofed block, 3.5 m per storey, entrance and rooftop plant. |
| `make_skyscraper_data` | `width 14, floors 30, glass {66,96,128}, seed 1` | Stepped glass tower, 3.6 m per storey, lit antenna. |
| `make_hangar_data` | `width 24, depth 30, height 9, wall {170,175,180}, roof {118,128,140}` | Vaulted roof; the large dark door is on +Z. |

## Structures

| Builder | Arguments (defaults) | Notes |
|---------|----------------------|-------|
| `make_wall_data` | `length 6, height 2.5, thickness 0.4, colour {150,144,134}` | Runs along X, with capstones and raised end piers so runs tile. |
| `make_fence_data` | `length 6, height 1.2, colour {176,140,96}` | Picket fence section along X. |
| `make_water_tower_data` | `height 16, tank_radius 3, tank {120,140,160}` | Braced legs, tank and conical lid. |
| `make_radar_data` | `height 8, dish_radius 3, colour {205,208,212}` | Mast and tilted dish facing +Z. |
| `make_domed_radar_data` | `radius 4, colour {236,238,240}` | Plinth and faceted radome. |
| `make_control_tower_data` | `height 24, seed 1` | Terminal block, shaft and glazed cab. |
| `make_street_lamp_data` | `height 6, colour {70,74,80}` | The lamp head glows at `(0, height, 0.9)`, the place for a local light. |
| `make_road_data` | `length 10, width 6` | Runs along Z, 0.06 m thick, with kerbs and a dashed centre line. |

## Small props

| Builder | Arguments (defaults) | Notes |
|---------|----------------------|-------|
| `make_barrel_data` | `radius 0.4, height 1, colour {60,90,150}` | |
| `make_crate_data` | `size 1, colour {168,128,80}` | Corner posts and edge boards. |
| `make_container_data` | `length 6.1, width 2.44, height 2.6, colour {160,52,42}, seed 1` | Doors on +Z; `seed` shifts the paint. |
| `make_tank_data` | `radius 3, height 6, colour {196,198,200}` | Domed top, ladder and pipe. |

## Nature

| Builder | Arguments (defaults) | Notes |
|---------|----------------------|-------|
| `make_tree_data` | `height 6, radius 2, trunk {104,74,48}, foliage {64,134,58}, seed 1` | Broadleaf: short trunk and lumpy canopy. |
| `make_bush_data` | `radius 0.8, colour {58,118,52}, seed 1` | |
| `make_rock_cluster_data` | `radius 1.5, colour {120,118,112}, seed 1` | Three to five boulders. |
| `make_conifer_data` (clutter.h) | `height 8, radius 2, trunk {96,64,40}, foliage {40,110,50}, sides 8` | Trunk and two foliage cones. |
| `make_rock_data` (clutter.h) | `radius 1, colour {120,118,112}, seed 1` | One faceted boulder. |

## Vehicles

| Builder | Arguments (defaults) | Notes |
|---------|----------------------|-------|
| `make_car_data` | `length 4.4, body {190,44,40}, seed 1` | Glazed cabin, wheels and lights. |
| `make_spaceship_data` | `length 8, hull {170,176,190}` | Landed on three legs, with glowing engines. |
| `make_aeroplane_data` | `wingspan 12, body {232,234,238}, stripe {40,90,170}` | Parked twin-engine airliner on its gear. |

## Rockets and spacecraft

Apollo era, at real sizes in metres. Rocket and station parts are bodies of revolution about Y that start on `y = 0` and end at `y = length`. Stack them by translating each part up by the heights below it with `transform_mesh_data`, then merge with `append_mesh_data`.

| Builder | Arguments (defaults) | Notes |
|---------|----------------------|-------|
| `make_rocket_stage_data` | `radius 5.05, length 20, body {236,236,230}, accent {28,28,32}, seed 1` | One cylindrical stage or a short ring such as an instrument unit; bands, tank rings and a +Z umbilical. |
| `make_rocket_engines_data` | `stage_radius 5.05, engines 5, length 5.9` | 1, 4 or 5 bells. Bell exits are on `y = 0` and the thrust frame is the top at `y = length`. |
| `make_rocket_adapter_data` | `bottom_radius 5.05, top_radius 3.3, length 3.3, body {236,236,230}, accent {28,28,32}` | Conical interstage. |
| `make_rocket_fins_data` | `radius 5.05, span 3.2, chord 7, count 4, colour {236,236,230}` | Swept tail fins; the first is 45 degrees off +Z. |
| `make_payload_fairing_data` | `radius 2, length 9, body {236,236,230}, accent {28,28,32}` | Ogive nose, closed at both ends. |
| `make_launch_escape_tower_data` | `height 10, motor {226,226,220}` | Four-legged truss under a motor and nose. |
| `make_apollo_csm_data` | `radius 1.95, hull {220,220,212}` | About 7 radii tall; the bell exit is on `y = 0`. |
| `make_saturn_v_data` | `scale 1` | The whole vehicle on the pad, about 115 m tall. |
| `make_launch_tower_data` | `height 120, width 8, arm_length 14, colour {196,70,52}` | Umbilical tower; the swing arms reach toward +X. |
| `make_launch_pad_data` | `size 60` | Slab with a flame trench and four lightning masts; the top of the slab is at `y = 1.5`. |

```cpp
sl::MeshData saturn = sl::make_saturn_v_data();
sl::MeshData pad = sl::make_launch_pad_data();
sl::transform_mesh_data(saturn, glm::translate(glm::mat4(1.0f), {0, 1.5f, 0}));   // stand on the slab
sl::append_mesh_data(pad, saturn);
```

## Satellites and stations

| Builder | Arguments (defaults) | Notes |
|---------|----------------------|-------|
| `make_station_module_data` | `radius 3.3, length 14.6, hull {236,236,230}, seed 1` | Skylab-style workshop: ribbed hull, windows on +Z, docking port on top. Axis along Y. |
| `make_docking_adapter_data` | `radius 1.5, length 5.2, hull {200,200,196}` | Axial port on top and a radial port on +Z. |
| `make_solar_array_data` | `length 10, width 3, panels 4, cell {30,48,118}` | Hinged on `x = 0`, spreads along +X; width is along Y, centred on `y = 0`. |
| `make_solar_windmill_data` | `blade_length 7, blade_width 3` | Four wings in a cross about the origin, in the XY plane facing +Z. |
| `make_antenna_dish_data` | `radius 1` | Mast on `y = 0`, dish facing +Z and tilted 20 degrees up. |
| `make_satellite_bus_data` | `size 1.5, foil {200,164,72}, seed 1` | Gold-foil body with instrument tube, thrusters, whip antenna and launch ring. |
| `make_satellite_data` | `size 1.5, seed 1` | The bus, two solar wings and a dish. |
| `make_skylab_data` | `scale 1` | The whole station standing upright, about 38 m tall. |

## Control parts

For 3D panels and cockpits. See [Control Panels](api_reference.md#control-panels-includecontrolsh) and the lesson [A clickable 3D panel](tutorial/3d-07-a-clickable-3d-panel.md). Static parts and their moving partners are separate so you can animate the moving one.

| Builder | Arguments (defaults) | Notes |
|---------|----------------------|-------|
| `make_panel_data` | `width, height, depth 0.03, colour {58,62,70}` | Flat panel behind the `z = 0` face, with a frame and corner screws. Width and height are required. |
| `make_dial_data` | `radius 0.1, ticks 11, sweep_degrees 270, face {18,20,24}` | Gauge face, centred on straight up. |
| `make_needle_data` | `radius 0.1, colour {255,120,40}` | Points along +Y; rotate about Z (clockwise is negative). |
| `make_knob_base_data` | `radius 0.04, ticks 11, sweep_degrees 270` | Static disc and scale. |
| `make_knob_data` | `radius 0.04, height 0.04, colour {50,54,62}` | Pointer along +Y; rotate about Z. |
| `make_toggle_base_data` | `size 0.04` | Static nut and collar. |
| `make_toggle_lever_data` | `size 0.04` | Pivots at `z = 0.3 * size`, points along +Z; tilt about X. |
| `make_button_base_data` | `radius 0.03` | Static bezel. |
| `make_button_cap_data` | `radius 0.03, colour {200,40,36}` | Slide along -Z when pressed. |
| `make_lamp_bezel_data` | `radius 0.015` | Static bezel. |
| `make_lamp_lens_data` | `radius 0.015` | White dome: tint it with the lamp colour and add an emissive colour to glow. |
| `make_slider_track_data` | `length 0.2, width 0.04` | Track along Y with scale marks. |
| `make_slider_cap_data` | `width 0.04` | Centred on the track. |
| `make_meter_housing_data` | `width 0.03, height 0.2` | Dark housing of a level meter. |
| `make_meter_segment_data` | `width 0.026, height 0.014` | One white segment: tint and add emissive. |

## Control assemblies

| Builder | Arguments | Notes |
|---------|-----------|-------|
| `make_cockpit_data` | `panel_width, panel_height, glm::mat4 *panel_pose = nullptr` | A small aircraft cockpit: floor, seat, stick, consoles, canopy frame and a tilted dashboard. The pilot sits at +Z. |
| `make_control_cabinet_data` | `panel_width, panel_height, glm::mat4 *panel_pose = nullptr` | A freestanding factory cabinet with a door, handle and vents; the panel sits on the door. Front faces +Z. |

`panel_pose` receives the matrix that puts a panel of that size (origin at its centre, +Z out) on the assembly:

```cpp
glm::mat4 pose;
sl::MeshData cabinet = sl::make_control_cabinet_data(0.6f, 0.4f, &pose);
// draw the panel's controls with `cabinet_matrix * pose * control_local_matrix`
```

## Helpers

- `append_mesh_data(into, part)` merges two meshes.
- `transform_mesh_data(data, matrix)` moves, rotates or uniformly scales a mesh's positions and rotates its normals. Non-uniform scale is not supported.
- `bake_instances(source, instances)` merges a copy per `Instance3D` into one mesh. Build it once, not per frame.

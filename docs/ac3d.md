# AC3D Models and FlightGear Animations

simlib can load static AC3D `.ac` models with `load_model()` or `load_ac3d_model()`. Loading creates GPU meshes, so initialize a graphics backend first. AC3D object names and composed parent/local transforms are retained on model parts; object surfaces are triangulated and retain UVs, diffuse material colour/transparency, two-sided rendering, and object texture references. Texture paths are relative to the `.ac` file. Missing textures are skipped. Line surfaces are ignored; AC3D specular/emissive shading and non-transform object types are not represented by the current material API.

```cpp
sl::Model *model = sl::load_model("aircraft.ac");
if (!model)
    std::fprintf(stderr, "%s\n", sl::last_error().c_str());
// In the 3D draw pass:
sl::draw_model(model, glm::mat4(1.0f));
sl::destroy_model(model);
```

## FlightGear model XML

`load_flightgear_model("Models/c172p.xml")` reads the direct `<path>` entry relative to the XML file, loads that model, and imports supported animations from its `<animation>` elements. It supports `rotate`, `translate`, `spin`, `blend`, and `select` visibility, including direct property names (literal `alias` strings are retained), factor/offset values, axis vectors or axis endpoints, centers, and linear `<interpolation>` tables. Propertyless translations with a fixed offset are imported as static part transforms. Spin animations advance using elapsed wall-clock time; the host supplies the input property, such as engine RPM. Other animation types (including `pick`, `billboard`, and `alpha-test`) are listed in `Model::unsupported_animations` and are not evaluated. The importer does not execute FlightGear's property tree, resolve relative property aliases, expand XML includes, or implement picking, sound, or Nasal behavior.

Use `save_model_animations_json()` after importing to convert supported transforms, spin/blend animations, and visibility definitions into a simlib JSON sidecar. Unsupported FlightGear animation types are not written.

Supply values from the host simulation using `set_model_property()`:

```cpp
sl::Model *aircraft = sl::load_flightgear_model("Models/c172p.xml");
if (aircraft)
{
    sl::set_model_property(aircraft, "controls/aileron", 0.5f);
    sl::draw_model(aircraft, glm::mat4(1.0f));
    sl::destroy_model(aircraft);
}
```

For imported `<property alias="..."/>` entries, the alias text is the property key accepted by `set_model_property()`; simlib does not resolve it against FlightGear's property tree. This allows a FlightGear-aware host to bind the source property itself.

## JSON animation definitions

For new projects, JSON is the preferred animation format. Load the model first, then pass an animation file to `load_model_animations_json()`. Version 1 accepts rotation, translation, spin, and blend animations, matching AC3D/model part names, input properties, a 3D axis (except for blend), an optional rotation center, a direct factor/offset mapping, blend minimum/maximum values, an optional piecewise-linear interpolation table, and the same boolean visibility conditions as the XML importer:

```json
{
  "version": 1,
  "animations": [
    {
      "type": "rotate",
      "property": "controls/aileron",
      "objects": ["leftaileron", "rightaileron"],
      "axis": [0, 0, 1],
      "center": [0, 0, 0],
      "factor": 20,
      "offset": 0,
      "interpolation": [[-1, -20], [0, 0], [1, 20]]
    }
  ],
  "visibility": [
    {
      "objects": ["Propeller.Fast"],
      "condition": {
        "op": "and",
        "conditions": [
          {"op": "greater_than", "property": "/params/engine/rpm", "value": 400},
          {"op": "not", "conditions": [
            {"op": "property", "property": "/params/engine/crashed"}
          ]}
        ]
      }
    }
  ]
}
```

When `interpolation` is present, its output values replace the direct `factor`/`offset` mapping and clamp to the first/last table output outside the input range. Rotation outputs are degrees; translation outputs are multiplied by the axis vector in model units. Spin treats its property as RPM and advances at `RPM * factor * 6` degrees per second. Blend maps its property to material alpha and clamps to its configured range. Multiple animations may use the same input property.

Build and run the viewer with a model path; the viewer lists every imported animation input and visibility switch (lights, doors and so on) on screen:

```bash
cmake --build build --target exmodel3d
./build/exmodel3d path/to/aircraft.ac
./build/exmodel3d path/to/FlightGear/Models/c172p.xml
./build/exmodel3d path/to/aircraft.ac --animations path/to/animations.json
```

The viewer uses the OpenGL backend by default and also supports Vulkan (`--vulkan`); `Surface3D` is not available on Direct3D 12. Use the arrow keys to orbit and `=`/`-` to zoom (zoom range scales to the model, so small instruments work). `Tab`/`PageDown` and `PageUp` select an input, `Space` toggles it between its minimum and maximum, `,`/`.` step the value, and `Home` resets everything; the selected input's parts and range are shown below the list. Inputs start at 0, so the engine is stopped and the propeller blades are visible; select `/params/engine/rpm` to spin it. When given a `.ac` that has a same-named FlightGear `.xml` beside it, the viewer loads the `.xml` instead, because the `.ac` alone carries none of the visibility rules. FlightGear's view blockers, Rembrandt light cones and unsupported beacon light helper meshes are hidden in the viewer. The mesh shader does not alpha-blend, so `draw_model()` skips parts whose material alpha (AC3D `1 - trans`, or a `blend` animation) is below 0.5: invisible hotspots, glass and the fast-propeller blur are not drawn rather than appearing as opaque slabs. FlightGear `not-equals`, `less-than-equals` and `greater-than-equals` conditions are imported, so lights-on gated parts (nav, landing and taxi lights) stay hidden until their property is set. It is a model-rendering example, not a FlightGear runtime. It does not provide aircraft systems, property-tree semantics, cockpit behavior, or full FlightGear animation compatibility.

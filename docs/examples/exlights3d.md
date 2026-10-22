# exlights3d

A moonlit courtyard lit by up to 16 coloured point lights and a sweeping spotlight, with soft sun shadows and shadows from the sweeping spotlight and one orbiting point light.

![exlights3d screenshot](img/exlights3d.png)

## What it shows
- `set_local_lights_3d()`: point and spot lights added on top of the `Light3D` sun, for both PBR and Lambert materials
- Smooth range falloff: each light reaches exactly zero at its `range`, so far-away lights cost nothing
- A sun shadow map (`ShadowMap3D`) rendered from the light's point of view and sampled with a soft 5x5 filter
- The same scene function drawn twice: once into the shadow map, once into the main surface

## Key code

```cpp
sl::LocalLight3D spot;
spot.position = {0.0f, 12.0f, 0.0f};
spot.direction = glm::normalize(glm::vec3(9.0f, -12.0f, 0.0f));
spot.intensity = 300.0f;
spot.range = 40.0f;
spot.inner_angle = 10.0f;
spot.outer_angle = 24.0f;
sl::set_local_lights_3d({spot});
```

```cpp
if (sl::begin_shadow_pass_3d(shadow_map, {0, 0, 0}, 16.0f))
{
    draw_scene();
    sl::end_shadow_pass_3d();
    sl::set_shadow_map_3d(shadow_map);
}
```

## Running
```
./build/exlights3d            # or --gl / --vulkan / --d3d12
```

## Controls
- Up / Down: number of lights
- P: PBR or Lambert
- S: sun shadows on or off
- L: spot and point light shadows on or off (`LocalShadowMap3D`: a perspective map for the spotlight, a six-face cube for the point light)
- Space: pause
- Right-drag: look; `[` / `]`: zoom
- Esc: exit

## Key APIs
- `sl::LocalLight3D`, `sl::set_local_lights_3d`, `sl::max_local_lights_3d`
- `sl::create_shadow_map_3d`, `sl::begin_shadow_pass_3d`, `sl::end_shadow_pass_3d`, `sl::set_shadow_map_3d`, `sl::create_local_shadow_map_3d`, `sl::render_local_shadows_3d`, `sl::set_local_shadow_map_3d`
- `sl::Light3D`, `sl::Material` (`pbr`, `metallic`, `roughness`)

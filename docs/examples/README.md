# Example Gallery

[← Back to README](../../README.md)

The [Tutorial](../tutorial/README.md) builds up the same ideas lesson by lesson; these pages show finished examples.

Every graphical example with a screenshot, what it demonstrates, how to run it and its controls. Screenshots are captured with [`tools/capture_examples.sh`](../../tools/capture_examples.sh) (OpenGL backend unless noted).

| Example | Summary |
|---|---|
| [![ex2dfeatures](img/ex2dfeatures.png)<br>**ex2dfeatures**](ex2dfeatures.md) | A tour of newer 2D features: text blocks, blend modes, clip rects, sprite animation clips, hex and infinite tile maps, and integer-scaled low-resolution targets. |
| [![ex3d_gl](img/ex3d_gl.png)<br>**ex3d_gl**](ex3d_gl.md) | A minimal raw OpenGL 3D example: a spinning model drawn with a custom shader and the simlib `Camera`. OpenGL only. |
| [![ex3d_surface](img/ex3d_surface.png)<br>**ex3d_surface**](ex3d_surface.md) | Backend-independent 3D with `Surface3D`: procedural meshes, an OBJ pyramid loaded from memory, lighting and MSAA, composited into the 2D screen. |
| [![ex3d_vulkan](img/ex3d_vulkan.png)<br>**ex3d_vulkan**](ex3d_vulkan.md) | A minimal raw Vulkan 3D example: a spinning model drawn with Vulkan-specific buffers and shaders. Vulkan only. |
| [![exanimsprite](img/exanimsprite.png)<br>**exanimsprite**](exanimsprite.md) | Animated sprites: looping sprites at different frame rates, a rotated pulsing bar, and click-to-spawn explosion rings. |
| [![exatlas](img/exatlas.png)<br>**exatlas**](exatlas.md) | Texture atlas browser: pick a tile from a grid atlas and see it drawn at 4x next to the full atlas texture. |
| [![exaudio](img/exaudio.png)<br>**exaudio**](exaudio.md) | Audio playback: sound effect samples, streamed music, pause and resume, and volume control. |
| [![exbitmap](img/exbitmap.png)<br>**exbitmap**](exbitmap.md) | Bitmap loading and blitting: loads a PNG and blits it to the screen at 60 FPS. |
| [![excafluid](img/excafluid.png)<br>**excafluid**](excafluid.md) | A falling-sand style sandbox: a multi-material cellular-automata fluid engine coupled to Box2D bodies, with lighting and bloom. |
| [![excamera2d](img/excamera2d.png)<br>**excamera2d**](excamera2d.md) | A 2D camera with zoom, rotation and shake, plus clip rectangles, extended sprite parameters and tweens. |
| [![exclouds](img/exclouds.png)<br>**exclouds**](exclouds.md) | A sky viewer for procedural cloud types, with altitude, sun position, planet curvature and volumetric rendering. |
| [![excompute](img/excompute.png)<br>**excompute**](excompute.md) | GPU compute: a compute shader updates 1,000 particles stored in a shader storage buffer, then the results are read back and drawn. |
| [![exconfig](img/exconfig.png)<br>**exconfig**](exconfig.md) | The JSON configuration subsystem (`sl::Config`): nested dotted-path keys and typed values edited live, saved to and loaded from `config_demo.json`. |
| [![exdb](img/exdb.png)<br>**exdb**](exdb.md) | A small embedded database: opens a file, reads rows and lists them on screen. |
| [![execs](img/execs.png)<br>**execs**](execs.md) | A header-only archetype ECS (`recs.h`) driving thousands of simulated entities, rendered with simlib. |
| [![exflight](img/exflight.png)<br>**exflight**](exflight.md) | Aircraft flight with `FlightWorld`: lifting and control surfaces, a jet engine, landing gear on a runway and an exponential atmosphere. |
| [![exfluid3d](img/exfluid3d.png)<br>**exfluid3d**](exfluid3d.md) | 3D voxel fluid sandbox: spray materials into a voxel volume rendered as a mesh with an orbit camera. |
| [![exfont](img/exfont.png)<br>**exfont**](exfont.md) | Loads TrueType fonts, measures text, and draws proportional and monospace samples side by side. |
| [![exgpuparticles](img/exgpuparticles.png)<br>**exgpuparticles**](exgpuparticles.md) | 150,000 particles simulated entirely on the GPU by a compute shader, with gravity wells, explosions and an additive glow. |
| [![exgui](img/exgui.png)<br>**exgui**](exgui.md) | Dear ImGui integration: the retained and debug GUI drawn on top of simlib, with the active backend and window size shown. |
| [![exhello](img/exhello.png)<br>**exhello**](exhello.md) | The smallest simlib program: open a window, draw centred text on a cleared background, and wait for a key press. |
| [![exinput](img/exinput.png)<br>**exinput**](exinput.md) | Input mapping: one `InputMap` binds keyboard, mouse and gamepad inputs to named actions, with live rebinding. |
| [![exinstancing](img/exinstancing.png)<br>**exinstancing**](exinstancing.md) | Compares three ways to draw thousands of cubes: one call each, per-frame instance vectors, and a persistent `InstanceBuffer`. |
| [![exparallel](img/exparallel.png)<br>**exparallel**](exparallel.md) | Parallel command recording: thousands of draw calls recorded on one thread, on worker threads by the backend, or built by your own threads with `RenderQueue3D`. |
| [![exisometric](img/exisometric.png)<br>**exisometric**](exisometric.md) | An isometric terrain editor: raise and lower diamond tiles, pan and zoom the view, and toggle textures and soft lighting. |
| [![exhdr](img/exhdr.png)<br>**exhdr**](exhdr.md) | HDR post-process: scene-space bloom, live exposure and tone mapping on a lamp-lit courtyard. |
| [![exjoints](img/exjoints.png)<br>**exjoints**](exjoints.md) | Physics joints: a bridge of hinged planks, a spring pendulum and a wheel-jointed car, all draggable with the mouse. |
| [![exlighting](img/exlighting.png)<br>**exlighting**](exlighting.md) | 2D radial lights with polygon soft shadows, rendered through a `LightingPass` into a render target. |
| [![exlobby](img/exlobby.png)<br>**exlobby**](exlobby.md) | A game lobby that finds games through an IRC channel and connects players directly over UDP with `NetServer` / `NetClient`. |
| [![exlua](img/exlua.png)<br>**exlua**](exlua.md) | Runs a sandboxed Lua script (`assets/scripts/scene.lua`) on a `LuaCanvas` and shows the result. |
| [![exluaconsole](img/exluaconsole.png)<br>**exluaconsole**](exluaconsole.md) | An interactive Lua console: type Lua on the input line and run it live against simlib’s Lua bindings. |
| [![exmipmap](img/exmipmap.png)<br>**exmipmap**](exmipmap.md) | Texture filtering comparison: three textured planes shown with bilinear, mipmapped and mipmapped 16x anisotropic sampling. |
| [![exmodel3d](img/exmodel3d.png)<br>**exmodel3d**](exmodel3d.md) | A model viewer for glTF/GLB, AC3D and FlightGear XML models with clip playback and property-driven animations. |
| [![exnormalmap](img/exnormalmap.png)<br>**exnormalmap**](exnormalmap.md) | 2D normal mapping: sprites lit by a mouse-driven light using per-sprite normal maps, with a toggle to compare against flat lighting. |
| [![exnormalmap3d](img/exnormalmap3d.png)<br>**exnormalmap3d**](exnormalmap3d.md) | 3D normal mapping: a cube and sphere with a procedural normal-mapped material under a 3D light, with a toggle to compare. |
| [![exlights3d](img/exlights3d.png)<br>**exlights3d**](exlights3d.md) | Up to 16 point and spot lights plus soft sun and light shadows in a moonlit courtyard. |
| [![exprops](img/exprops.png)<br>**exprops**](exprops.md) | A gallery of 22 low-poly scenery parts (houses, towers, vehicles, trees and more) with focus, zoom and day/night. |
| [![extown](img/extown.png)<br>**extown**](extown.md) | A town and airfield built from props, with sun shadows and lamp lights at night. |
| [![excontrols](img/excontrols.png)<br>**excontrols**](excontrols.md) | 2D cockpit and factory control panels: dials, knobs, switches, LED meters and glowing lamps. |
| [![excockpit](img/excockpit.png)<br>**excockpit**](excockpit.md) | 3D cockpit and control cabinet with mouse picking and emissive LEDs. |
| [![exocean](img/exocean.png)<br>**exocean**](exocean.md) | An ocean of Gerstner waves under volumetric clouds with a dreadnought heaving, pitching and rolling on the swell. |
| [![exorbit](img/exorbit.png)<br>**exorbit**](exorbit.md) | Spaceflight: a craft with main engine, retro thrust and RCS, orbit prediction, time warp and a Moon on rails. |
| [![exparticles](img/exparticles.png)<br>**exparticles**](exparticles.md) | CPU particle emitter that follows the mouse, with gravity, blur and glow post effects. |
| [![expathfinding](img/expathfinding.png)<br>**expathfinding**](expathfinding.md) | Layered pathfinding: enemies pick between chase, scent and sound maps to hunt a player through a tile grid with editable walls. |
| [![expbr](img/expbr.png)<br>**expbr**](expbr.md) | Physically based rendering: a grid of spheres varying metallic and roughness, with an emissive sphere and a toggle for the PBR path. |
| [![exphysics](img/exphysics.png)<br>**exphysics**](exphysics.md) | Box2D physics: a static floor and walls with falling boxes, circles and polygons, and contact polling. |
| [![expostprocess](img/expostprocess.png)<br>**expostprocess**](expostprocess.md) | A gallery of full-screen post-processing effects that can be toggled live over an animated scene. |
| [![exrendertarget](img/exrendertarget.png)<br>**exrendertarget**](exrendertarget.md) | Offscreen render targets: draws shapes into a render target and then draws that target to the screen as a sprite. |
| [![exresources](img/exresources.png)<br>**exresources**](exresources.md) | ZIP resource archives: opens `resource_demo.zip`, lists entries and previews a Lua script from inside it. |
| [![exroguelike](img/exroguelike.png)<br>**exroguelike**](exroguelike.md) | A coloured ASCII roguelike: procedural overworld and dungeons, line-of-sight FOV, lights and combat, built on simlib, `recs` and `rworld`. |
| [![exrotatesprite](img/exrotatesprite.png)<br>**exrotatesprite**](exrotatesprite.md) | Loads a bitmap and spins it with rotation and scaling at a fixed frame rate. |
| [![exshader](img/exshader.png)<br>**exshader**](exshader.md) | Loads a GLSL vertex and fragment shader from files and uses it to tint a sprite. |
| [![exspriteeditor](img/exspriteeditor.png)<br>**exspriteeditor**](exspriteeditor.md) | A pixel sprite and animation editor with a resizable canvas, undo, zoom, frames and PNG spritesheet save and load. |
| [![exsynth](img/exsynth.png)<br>**exsynth**](exsynth.md) | Procedural audio: generates laser, explosion, coin and tone samples from oscillator parameters at startup. |
| [![exterrain](img/exterrain.png)<br>**exterrain**](exterrain.md) | A fly-over procedural terrain with normals, atmosphere, clouds, volumetrics, and instanced trees and rocks. |
| [![extilemap](img/extilemap.png)<br>**extilemap**](extilemap.md) | Tile map rendering: a map built in code, or a Tiled `.tmj` map loaded from a path, scrolled with the arrow keys with tile collision. |
| [![exxeno](img/exxeno.png)<br>**exxeno**](exxeno.md) | Lit top-down shooter with synthesised sound and music: a marine with a flashlight shoots xenomorphs; kills leave acid splats stamped with tile-map decals. |
| [![extriangle](img/extriangle.png)<br>**extriangle**](extriangle.md) | Immediate-mode 2D primitives: outlined and filled triangles drawn over a loaded bitmap. |
| [![exvoxel](img/exvoxel.png)<br>**exvoxel**](exvoxel.md) | A Minecraft-style voxel world: fly around, break and place blocks, with chunk meshing. |
| [![exvulkan](img/exvulkan.png)<br>**exvulkan**](exvulkan.md) | A Vulkan smoke test: a textured, rotating sprite and primitives rendered into a target with a bloom pass. |

## Deep dives
- [Roguelike guide](../exroguelike_guide.md)
- [Cellular Automata Fluid guide](../excafluid_guide.md)
- [GPU Compute Particles guide](../exgpuparticles_guide.md)
- [slpaint guide](../slpaint_guide.md)

## Regenerating screenshots
```bash
tools/capture_examples.sh            # all rows in tools/capture_examples.tsv
tools/capture_examples.sh exlighting # one or more by name
```
Needs an X display, `xdotool` and ImageMagick (`import`). Each row of the table gives the executable, arguments, a startup delay and optional scripted input steps.

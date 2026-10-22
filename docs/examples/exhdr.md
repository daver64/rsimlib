# exhdr

A lamp-lit courtyard post-processed with `sl::Hdr`: bright lamps bloom and highlights roll off instead of clipping.

![exhdr screenshot](img/exhdr.png)

## What it shows
- `sl::Hdr`: expands the 8-bit frame into scene-referred values (near-white pixels count as values up to a white point of 8), blooms what exceeds a threshold, then applies exposure and a tone-mapping curve
- Live exposure control and a choice of Reinhard (exact round trip at exposure 1) or ACES filmic curves
- Toggle HDR off with `H` to compare with the plain frame
- Works on OpenGL, Vulkan and Direct3D 12 using the standard custom-shader path

This is "pseudo-HDR": render targets stay 8-bit, so values above the white point cannot be represented, but unlit white pixels and saturated highlights are treated as bright light sources.

## Key code

```cpp
sl::Hdr hdr;
hdr.initialise();
hdr.set_bloom_threshold(2.0f);
hdr.set_exposure(1.5f);
hdr.set_tonemap(sl::HdrTonemap::aces);
hdr.apply(sl::surface_3d_bitmap(surface), 0, 0, width, height);
```

## Running
```
./build/exhdr            # or --gl / --vulkan / --d3d12
```

## Controls
- H: HDR on or off
- Up / Down: exposure
- T: Reinhard or ACES curve
- Space: pause
- Right-drag: look; `[` / `]`: zoom
- Esc: exit

## Key APIs
- `sl::Hdr` (`set_exposure`, `set_white_point`, `set_bloom_threshold`, `set_bloom_intensity`, `set_bloom_radius`, `set_downsample`, `set_tonemap`, `apply`), `sl::HdrTonemap`

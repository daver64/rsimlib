# exsynth

Procedural audio: generates laser, explosion, coin and tone samples from oscillator parameters at startup.

![exsynth screenshot](img/exsynth.png)

## What it shows
- Describing sounds with `SynthParams` oscillators and envelopes
- Waveforms: square, saw, triangle, sine and noise
- Rendering parameters to a sample with `generate_sample`
- Playing the result with `play_sample`

## Running
```bash
./build/exsynth
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Needs a working audio device.

## Controls
| Input | Action |
|-------|--------|
| 1 | Laser |
| 2 | Explosion |
| 3 | Coin |
| 4 | Tone (A4) |
| Escape | Exit |

## Key APIs
- `SynthParams`, `Waveform`, `generate_sample` — [Procedural Audio](../api_reference.md#procedural-audio-synthesis-includeaudio_synthh) · [`audio_synth.h`](../../include/audio_synth.h)
- `audio_fx_init`, `play_sample`, `destroy_sample` — [Audio Subsystem](../api_reference.md#audio-subsystem-samples--streams-includeaudioh) · [`audio.h`](../../include/audio.h)

## Source
[src/examples/exsynth/exsynth.cpp](../../src/examples/exsynth/exsynth.cpp)

## See also
- [exaudio](exaudio.md)

# exsynth

Procedural audio: generates laser, explosion, coin and tone samples from oscillator parameters at startup.

![exsynth screenshot](img/exsynth.png)

## What it shows
- Describing sounds with `SynthParams` oscillators and envelopes
- Waveforms: square, saw, triangle, sine and noise
- Rendering parameters to a sample with `generate_sample`
- Playing the result with `play_sample`

## Key code

In `exsynth.cpp`, sounds are described with `SynthParams` (duration, envelope and oscillators with different waveforms) and rendered with `generate_sample`.

```cpp
sl::Sample *make_explosion()
{
    sl::SynthParams p;
    p.duration = 0.9f;
    p.envelope = {0.005f, 0.3f, 0.4f, 0.5f};
    p.oscillators.push_back({sl::Waveform::noise, 1.0f, 0.7f, {}});
    p.oscillators.push_back({sl::Waveform::saw, 110.0f, 0.5f, {1.0f, 0.3f}});
    return sl::generate_sample(p);
}
```

Other patches use the square, triangle and sine waveforms the same way.

```cpp
p.oscillators.push_back({sl::Waveform::square, 880.0f, 0.6f, {1.0f, 0.15f}});
// ...
p.oscillators.push_back({sl::Waveform::triangle, 988.0f, 0.5f, {1.0f, 1.0f}});
// ...
p.oscillators.push_back({sl::Waveform::sine, 440.0f, 1.0f, {}});
```

Pressing a digit key plays the generated sample with `play_sample`.

```cpp
if (index >= 0 && patches[index].sample)
    sl::play_sample(patches[index].sample, 220);
```

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

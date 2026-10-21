# exaudio

Audio playback: sound effect samples, streamed music, pause and resume, and volume control.

![exaudio screenshot](img/exaudio.png)

## What it shows
- Initialising the audio systems with `audio_fx_init` and `music_init`
- Loading and playing a sample with `load_sample` and `play_sample`
- Streaming music with `load_stream` and `play_stream`
- Pausing, resuming and stopping a stream
- Adjusting sound and music volume

## Running
```bash
./build/exaudio
```

Accepts `--gl`, `--vulkan` and `--d3d12` (Windows only) to choose the graphics backend; the default is OpenGL on Linux and Direct3D 12 on Windows. Needs the audio assets under `assets/music` and `assets/sfx` (run from the repository root) and a working audio device.

## Controls
| Input | Action |
|-------|--------|
| Space | Play the sound effect |
| M | Pause or resume the music |
| S | Stop the music |
| Left / Right arrows | Decrease / increase volume |
| Escape | Exit |

## Key APIs
- `load_sample`, `play_sample`, `stop_voice`, `audio_fx_set_volume` — [Audio Subsystem](../api_reference.md#audio-subsystem-samples--streams-includeaudioh) · [`audio.h`](../../include/audio.h)
- `load_stream`, `play_stream`, `pause_stream`, `resume_stream`, `stop_stream`, `music_set_volume` — [Audio Subsystem](../api_reference.md#audio-subsystem-samples--streams-includeaudioh) · [`audio.h`](../../include/audio.h)
- `last_error` — [Error Handling](../api_reference.md#error-handling-includeerrorh) · [`error.h`](../../include/error.h)

## Source
[src/examples/exaudio/exaudio.cpp](../../src/examples/exaudio/exaudio.cpp)

## See also
- [exsynth](exsynth.md)
- [exinput](exinput.md)

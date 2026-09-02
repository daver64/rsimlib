#pragma once

#include <cstdint>
#include <string>

namespace simlib {

namespace audio_fx {

struct Sample;

/** Volume and pan match Allegro 4's 0-255 convention. */
/** Initialize the sound effects mixer. */
bool init();
/** Shut down sound effects and release mixer resources. */
void shutdown();

/** Load a sample from an audio file. */
Sample* load_sample(const std::string& path);
/** Destroy a sample returned by load_sample(). */
void destroy_sample(Sample* sample);

/** Play a sample and return its mixer voice identifier. */
std::uint64_t play_sample(
	Sample* sample,
	int volume = 255,
	int pan = 128,
	int frequency = 1000,
	int loops = 0
);
/** Stop one mixer voice. */
void stop_voice(std::uint64_t voice);
/** Stop every voice currently playing the sample. */
void stop_sample(Sample* sample);
/** Stop all active sound-effect voices. */
void stop_all_samples();
/** Return whether a voice is currently playing. */
bool voice_is_playing(std::uint64_t voice);
/** Set a voice's pan using the 0-255 Allegro convention. */
void set_pan(std::uint64_t voice, int pan);
/** Set the global sound effects volume. */
void set_volume(int volume);

} // namespace audio_fx

namespace music {

struct Stream;

/** Initialize the music mixer. */
bool init();
/** Shut down music playback and release mixer resources. */
void shutdown();

/** Load a music stream from a file. */
Stream* load_stream(const std::string& path);
/** Destroy a stream returned by load_stream(). */
void destroy_stream(Stream* stream);

/** Start playback, looping forever by default. */
void play_stream(Stream* stream, int loops = -1);
/** Stop the current music stream. */
void stop_stream();
/** Pause the current music stream. */
void pause_stream();
/** Resume paused music playback. */
void resume_stream();
/** Set the global music volume. */
void set_volume(int volume);

} // namespace music

} // namespace simlib

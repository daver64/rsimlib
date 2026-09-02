#pragma once

#include <cstdint>
#include <string>

namespace rvoid {

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
/** Set the global sound effects volume. */
void set_volume(int volume);

} // namespace audio_fx

namespace music {

struct Track;

/** Initialize the music mixer. */
bool init();
/** Shut down music playback and release mixer resources. */
void shutdown();

/** Load a MIDI track from a file. */
Track* load_midi(const std::string& path);
/** Destroy a track returned by load_midi(). */
void destroy_midi(Track* track);

/** Start playback, looping forever by default. */
void play_midi(Track* track, int loops = -1);
/** Stop the current music track. */
void stop_midi();
/** Pause the current music track. */
void pause_midi();
/** Resume paused music playback. */
void resume_midi();
/** Set the global music volume. */
void set_volume(int volume);

} // namespace music

} // namespace rvoid

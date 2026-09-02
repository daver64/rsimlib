#include "audio_test.h"

#include "audio.h"

#include <filesystem>
#include <iostream>
#include <random>
#include <vector>

namespace {

class AudioFxTest {
public:
    void initialize() {
        if (!simlib::audio_fx::init()) {
            std::cerr << "Failed to initialize audio effects.\n";
            return;
        }

        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator("assets/sfx", error)) {
            if (entry.is_regular_file(error) && entry.path().extension() == ".wav") {
                files_.push_back(entry.path().string());
            }
        }
        if (error || files_.empty()) {
            std::cerr << "No WAV files found in assets/sfx.\n";
            simlib::audio_fx::shutdown();
        }
    }

    void play_random() {
        if (files_.empty()) {
            return;
        }

        std::uniform_int_distribution<std::size_t> fileIndex(0, files_.size() - 1);
        const std::string& path = files_[fileIndex(engine_)];
        simlib::audio_fx::Sample* sample = simlib::audio_fx::load_sample(path);
        if (!sample) {
            std::cerr << "Failed to load sample: " << path << '\n';
            return;
        }

        simlib::audio_fx::play_sample(sample);
        samples_.push_back(sample);
        if (samples_.size() > max_retained_samples) {
            simlib::audio_fx::stop_sample(samples_.front());
            simlib::audio_fx::destroy_sample(samples_.front());
            samples_.erase(samples_.begin());
        }
    }
    void stop() {
        for (simlib::audio_fx::Sample* sample : samples_) {
            simlib::audio_fx::stop_sample(sample);
        }
    }
    void shutdown() {
        for (simlib::audio_fx::Sample* sample : samples_) {
            simlib::audio_fx::stop_sample(sample);
            simlib::audio_fx::destroy_sample(sample);
        }
        samples_.clear();
        simlib::audio_fx::shutdown();
    }

private:
    static constexpr std::size_t max_retained_samples = 16;

    std::vector<std::string> files_;
    std::vector<simlib::audio_fx::Sample*> samples_;
    std::mt19937 engine_{std::random_device{}()};
};

class MusicTest {
public:
    void initialize() {
        if (!simlib::music::init()) {
            std::cerr << "Failed to initialize music playback.\n";
            return;
        }

        track_ = simlib::music::load_stream("assets/music/Solar Serenity.ogg");
        if (!track_) {
            std::cerr << "Failed to load music: assets/music/Solar Serenity.ogg\n";
            simlib::music::shutdown();
        }
    }

    void play() {
        if (track_) {
            simlib::music::play_stream(track_, 0);
        }
    }
    void stop() {
        if (track_) {
            simlib::music::stop_stream();
        }
    }
    void shutdown() {
        if (track_) {
            simlib::music::stop_stream();
            simlib::music::destroy_stream(track_);
            track_ = nullptr;
        }
        simlib::music::shutdown();
    }

private:
    simlib::music::Stream* track_ = nullptr;
};

AudioFxTest audio_fx_test;
MusicTest music_test;

} // namespace

namespace audio_test {

void initialize() {
    audio_fx_test.initialize();
    music_test.initialize();
}

void handle_event(const SDL_Event& event) {
    if (event.type != SDL_KEYDOWN || event.key.repeat != 0) {
        return;
    }

    if (event.key.keysym.sym == SDLK_s) {
        audio_fx_test.play_random();
    } else if (event.key.keysym.sym == SDLK_m) {
        music_test.play();
    } else if (event.key.keysym.sym==SDLK_SPACE) {
        audio_fx_test.stop();
        music_test.stop();
    }
}

void shutdown() {
    music_test.shutdown();
    audio_fx_test.shutdown();
}

} // namespace audio_test
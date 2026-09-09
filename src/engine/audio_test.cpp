/** @file
 * @brief Implements the interactive audio test harnesses.
 */

#include "audio_test.h"

#include "audio.h"

#include <filesystem>
#include <iostream>
#include <random>
#include <vector>

namespace
{

    /** Interactive sound-effect test harness. */
    class AudioFxTest
    {
    public:
        /** Initialize audio effects and discover test samples. */
        void initialize()
        {
            if (!sl::audio_fx_init())
            {
                std::cerr << "Failed to initialize audio effects.\n";
                return;
            }

            std::error_code error;
            for (const auto &entry : std::filesystem::directory_iterator("assets/sfx", error))
            {
                if (entry.is_regular_file(error) && entry.path().extension() == ".wav")
                {
                    files_.push_back(entry.path().string());
                }
            }
            if (error || files_.empty())
            {
                std::cerr << "No WAV files found in assets/sfx.\n";
                sl::audio_fx_shutdown();
            }
        }

        /** Play one randomly selected test sample. */
        void play_random()
        {
            if (files_.empty())
            {
                return;
            }

            std::uniform_int_distribution<std::size_t> fileIndex(0, files_.size() - 1);
            const std::string &path = files_[fileIndex(engine_)];
            sl::Sample *sample = sl::load_sample(path);
            if (!sample)
            {
                std::cerr << "Failed to load sample: " << path << '\n';
                return;
            }

            sl::play_sample(sample);
            samples_.push_back(sample);
            if (samples_.size() > max_retained_samples)
            {
                sl::stop_sample(samples_.front());
                sl::destroy_sample(samples_.front());
                samples_.erase(samples_.begin());
            }
        }
        /** Stop all samples retained by the test harness. */
        void stop()
        {
            for (sl::Sample *sample : samples_)
            {
                sl::stop_sample(sample);
            }
        }
        /** Release all samples and shut down audio effects. */
        void shutdown()
        {
            for (sl::Sample *sample : samples_)
            {
                sl::stop_sample(sample);
                sl::destroy_sample(sample);
            }
            samples_.clear();
            sl::audio_fx_shutdown();
        }

    private:
        static constexpr std::size_t max_retained_samples = 16;

        std::vector<std::string> files_;
        std::vector<sl::Sample *> samples_;
        std::mt19937 engine_{std::random_device{}()};
    };

    /** Interactive music-stream test harness. */
    class MusicTest
    {
    public:
        /** Initialize music playback and load the test stream. */
        void initialize()
        {
            if (!sl::music_init())
            {
                std::cerr << "Failed to initialize music playback.\n";
                return;
            }

            track_ = sl::load_stream("assets/music/Solar Serenity.ogg");
            if (!track_)
            {
                std::cerr << "Failed to load music: assets/music/Solar Serenity.ogg\n";
                sl::music_shutdown();
            }
        }

        /** Start the loaded test stream. */
        void play()
        {
            if (track_)
            {
                sl::play_stream(track_, 0);
            }
        }
        /** Stop the test stream. */
        void stop()
        {
            if (track_)
            {
                sl::stop_stream();
            }
        }
        /** Release the test stream and shut down music playback. */
        void shutdown()
        {
            if (track_)
            {
                sl::stop_stream();
                sl::destroy_stream(track_);
                track_ = nullptr;
            }
            sl::music_shutdown();
        }

    private:
        sl::Stream *track_ = nullptr;
    };

    AudioFxTest audio_fx_test;
    MusicTest music_test;

} // namespace

namespace audio_test
{

    void initialize()
    {
        audio_fx_test.initialize();
        music_test.initialize();
    }

    void handle_event(const sl::Event &event)
    {
        if (event.type() != sl::Event::Type::key_down || event.key_repeat())
        {
            return;
        }

        if (event.key() == sl::Event::Key::letter_s)
        {
            audio_fx_test.play_random();
        }
        else if (event.key() == sl::Event::Key::letter_m)
        {
            music_test.play();
        }
        else if (event.key() == sl::Event::Key::space)
        {
            audio_fx_test.stop();
            music_test.stop();
        }
    }

    void shutdown()
    {
        music_test.shutdown();
        audio_fx_test.shutdown();
    }

} // namespace audio_test
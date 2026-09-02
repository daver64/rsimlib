
#include "audio.h"

#include "audio_backend.h"
#include "error.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <unordered_map>

namespace simlib::audio_fx {

struct Sample {
	Mix_Chunk* chunk = nullptr;
};

namespace {

class FxWorker {
public:
	bool start() {
		std::lock_guard<std::mutex> lock(mutex_);
		if (thread_.joinable()) {
			return true;
		}
		if (!audio_detail::acquire_mixer()) {
			return false;
		}
		stopping_ = false;
		thread_ = std::thread([this] { run(); });
		return true;
	}

	void stop() {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (!thread_.joinable()) {
				return;
			}
			stopping_ = true;
		}
		condition_.notify_one();
		thread_.join();
		audio_detail::release_mixer();
	}

	template <typename Function>
	auto call(Function&& function) -> decltype(function()) {
		using Result = decltype(function());
		auto result = std::make_shared<std::promise<Result>>();
		auto future = result->get_future();
		enqueue([function = std::forward<Function>(function), result]() mutable {
			if constexpr (std::is_void_v<Result>) {
				function();
				result->set_value();
			} else {
				result->set_value(function());
			}
		});
		return future.get();
	}

	void enqueue(std::function<void()> command) {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			commands_.push_back(std::move(command));
		}
		condition_.notify_one();
	}

private:
	void run() {
		for (;;) {
			std::function<void()> command;
			{
				std::unique_lock<std::mutex> lock(mutex_);
				condition_.wait(lock, [this] { return stopping_ || !commands_.empty(); });
				if (commands_.empty() && stopping_) {
					return;
				}
				command = std::move(commands_.front());
				commands_.pop_front();
			}
			command();
		}
	}

	std::mutex mutex_;
	std::condition_variable condition_;
	std::deque<std::function<void()>> commands_;
	std::thread thread_;
	bool stopping_ = false;
};

FxWorker worker;
std::atomic<std::uint64_t> next_voice{1};
std::unordered_map<std::uint64_t, int> voices;
int master_volume = 255;

int mixer_volume(int volume) {
	return std::clamp(volume, 0, 255) * MIX_MAX_VOLUME / 255;
}

void apply_pan(int channel, int pan) {
	pan = std::clamp(pan, 0, 255);
	const Uint8 left = static_cast<Uint8>(pan <= 128 ? 255 : 255 - ((pan - 128) * 255 / 127));
	const Uint8 right = static_cast<Uint8>(pan >= 128 ? 255 : pan * 255 / 128);
	Mix_SetPanning(channel, left, right);
}

} // namespace

bool init() {
	const bool started = worker.start();
	if (!started) simlib::detail::set_error(Mix_GetError());
	return started;
}

void shutdown() {
	worker.stop();
}

Sample* load_sample(const std::string& path) {
	if (!init()) {
		return nullptr;
	}
	return worker.call([path] {
		std::lock_guard<std::mutex> lock(audio_detail::mixer_mutex());
		Mix_Chunk* chunk = Mix_LoadWAV(path.c_str());
		if (!chunk) simlib::detail::set_error(Mix_GetError());
		return chunk ? new Sample{chunk} : nullptr;
	});
}

void destroy_sample(Sample* sample) {
	if (!sample || !init()) {
		return;
	}
	worker.call([sample] {
		std::lock_guard<std::mutex> lock(audio_detail::mixer_mutex());
		for (auto voice = voices.begin(); voice != voices.end();) {
			if (Mix_GetChunk(voice->second) == sample->chunk) {
				Mix_HaltChannel(voice->second);
				voice = voices.erase(voice);
			} else {
				++voice;
			}
		}
		Mix_FreeChunk(sample->chunk);
		delete sample;
	});
}

std::uint64_t play_sample(Sample* sample, int volume, int pan, int frequency, int loops) {
	if (!sample || !init()) {
		return 0;
	}
	(void)frequency;
	const std::uint64_t voice = next_voice++;
	worker.enqueue([sample, volume, pan, loops, voice] {
		std::lock_guard<std::mutex> lock(audio_detail::mixer_mutex());
		const int channel = Mix_PlayChannel(-1, sample->chunk, std::max(loops, 0));
		if (channel < 0) {
			return;
		}
		voices[voice] = channel;
		Mix_Volume(channel, mixer_volume(volume) * master_volume / 255);
		apply_pan(channel, pan);
	});
	return voice;
}

void stop_voice(std::uint64_t voice) {
	if (!init()) {
		return;
	}
	worker.enqueue([voice] {
		std::lock_guard<std::mutex> lock(audio_detail::mixer_mutex());
		const auto found = voices.find(voice);
		if (found != voices.end()) {
			Mix_HaltChannel(found->second);
			voices.erase(found);
		}
	});
}

void stop_sample(Sample* sample) {
	if (!sample || !init()) {
		return;
	}
	worker.enqueue([sample] {
		std::lock_guard<std::mutex> lock(audio_detail::mixer_mutex());
		for (auto voice = voices.begin(); voice != voices.end();) {
			if (Mix_GetChunk(voice->second) == sample->chunk) {
				Mix_HaltChannel(voice->second);
				voice = voices.erase(voice);
			} else {
				++voice;
			}
		}
	});
}

void stop_all_samples() {
	if (!init()) return;
	worker.enqueue([] {
		std::lock_guard<std::mutex> lock(audio_detail::mixer_mutex());
		Mix_HaltChannel(-1);
		voices.clear();
	});
}

bool voice_is_playing(std::uint64_t voice) {
	if (!init()) return false;
	return worker.call([voice] {
		std::lock_guard<std::mutex> lock(audio_detail::mixer_mutex());
		const auto found = voices.find(voice);
		return found != voices.end() && Mix_Playing(found->second) != 0;
	});
}

void set_pan(std::uint64_t voice, int pan) {
	if (!init()) return;
	worker.enqueue([voice, pan] {
		std::lock_guard<std::mutex> lock(audio_detail::mixer_mutex());
		const auto found = voices.find(voice);
		if (found != voices.end()) apply_pan(found->second, pan);
	});
}

void set_volume(int volume) {
	if (!init()) {
		return;
	}
	worker.enqueue([volume] {
		master_volume = std::clamp(volume, 0, 255);
		std::lock_guard<std::mutex> lock(audio_detail::mixer_mutex());
		Mix_Volume(-1, mixer_volume(master_volume));
	});
}

} // namespace simlib::audio_fx

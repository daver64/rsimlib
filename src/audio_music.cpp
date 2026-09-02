
#include "audio.h"

#include "audio_backend.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <type_traits>

namespace rvoid::music {

struct Track {
	Mix_Music* music = nullptr;
};

namespace {

class MusicWorker {
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

MusicWorker worker;
int master_volume = 255;

int mixer_volume(int volume) {
	return std::clamp(volume, 0, 255) * MIX_MAX_VOLUME / 255;
}

} // namespace

bool init() {
	return worker.start();
}

void shutdown() {
	worker.stop();
}

Track* load_midi(const std::string& path) {
	if (!init()) {
		return nullptr;
	}
	return worker.call([path] {
		std::lock_guard<std::mutex> lock(audio_detail::mixer_mutex());
		Mix_Music* music = Mix_LoadMUS(path.c_str());
		return music ? new Track{music} : nullptr;
	});
}

void destroy_midi(Track* track) {
	if (!track || !init()) {
		return;
	}
	worker.call([track] {
		std::lock_guard<std::mutex> lock(audio_detail::mixer_mutex());
		Mix_HaltMusic();
		Mix_FreeMusic(track->music);
		delete track;
	});
}

void play_midi(Track* track, int loops) {
	if (!track || !init()) {
		return;
	}
	worker.enqueue([track, loops] {
		std::lock_guard<std::mutex> lock(audio_detail::mixer_mutex());
		Mix_VolumeMusic(mixer_volume(master_volume));
		Mix_PlayMusic(track->music, loops);
	});
}

void stop_midi() {
	if (!init()) {
		return;
	}
	worker.enqueue([] {
		std::lock_guard<std::mutex> lock(audio_detail::mixer_mutex());
		Mix_HaltMusic();
	});
}

void pause_midi() {
	if (!init()) {
		return;
	}
	worker.enqueue([] {
		std::lock_guard<std::mutex> lock(audio_detail::mixer_mutex());
		Mix_PauseMusic();
	});
}

void resume_midi() {
	if (!init()) {
		return;
	}
	worker.enqueue([] {
		std::lock_guard<std::mutex> lock(audio_detail::mixer_mutex());
		Mix_ResumeMusic();
	});
}

void set_volume(int volume) {
	if (!init()) {
		return;
	}
	worker.enqueue([volume] {
		master_volume = std::clamp(volume, 0, 255);
		std::lock_guard<std::mutex> lock(audio_detail::mixer_mutex());
		Mix_VolumeMusic(mixer_volume(master_volume));
	});
}

} // namespace rvoid::music

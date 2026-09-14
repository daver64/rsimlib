#include "sl.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

int main()
{
    const std::filesystem::path temp_config =
        std::filesystem::temp_directory_path() / "simlib_test_config.json";
    std::error_code cleanup_error;
    std::filesystem::remove(temp_config, cleanup_error);

    sl::Config config;

    // Test 1: Set nested dotted keys
    config.set("display.width", 1280);
    config.set("display.height", 720);
    config.set("display.fullscreen", false);
    config.set("audio.volume", 0.75);
    config.set("audio.muted", false);
    config.set("player.stats.level", 5);
    config.set("player.stats.name", std::string("Knight"));

    // Test 2: Contains check
    assert(config.contains("display.width"));
    assert(config.contains("display.height"));
    assert(config.contains("player.stats.level"));
    assert(config.contains("player.stats.name"));
    assert(!config.contains("display.invalid"));
    assert(!config.contains("nonexistent.key"));

    // Test 3: Get typed values
    assert(config.get<int>("display.width", 800) == 1280);
    assert(config.get<int>("display.height", 600) == 720);
    assert(config.get<bool>("display.fullscreen", true) == false);
    assert(std::abs(config.get<double>("audio.volume", 0.0) - 0.75) < 0.001);
    assert(config.get<int>("player.stats.level", 1) == 5);
    assert(config.get<std::string>("player.stats.name", "Default") == "Knight");

    // Test 4: Fallbacks for missing/invalid keys
    assert(config.get<int>("missing.key", 999) == 999);
    assert(config.get<std::string>("audio.volume", "fallback") == "fallback");

    // Test 5: Save to disk
    assert(config.save(temp_config.string()));
    assert(std::filesystem::exists(temp_config));

    // Test 6: Load from disk into new config
    sl::Config loaded;
    assert(loaded.load(temp_config.string()));
    assert(loaded.contains("display.width"));
    assert(loaded.get<int>("display.width", 0) == 1280);
    assert(loaded.get<int>("display.height", 0) == 720);
    assert(loaded.get<int>("player.stats.level", 0) == 5);
    assert(loaded.get<std::string>("player.stats.name", "") == "Knight");

    // Cleanup
    std::filesystem::remove(temp_config, cleanup_error);

    std::cout << "All sl::Config unit tests passed successfully.\n";
    return 0;
}

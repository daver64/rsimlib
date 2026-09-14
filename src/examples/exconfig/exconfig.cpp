#include "sl.h"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    const char *CONFIG_FILE = "config_demo.json";

    void init_default_config(sl::Config &config)
    {
        config.set("video.width", 900);
        config.set("video.height", 600);
        config.set("video.fullscreen", false);
        config.set("video.vsync", true);
        config.set("audio.master_volume", 0.75);
        config.set("audio.music_volume", 0.60);
        config.set("audio.sfx_volume", 0.80);
        config.set("gameplay.difficulty", std::string("Normal"));
        config.set("gameplay.player.name", std::string("Adventurer"));
        config.set("gameplay.player.lives", 3);
        config.set("gameplay.player.score", 1250);
        config.set("ui.theme", std::string("Dark"));
        config.set("ui.show_fps", true);
    }

    struct ThemeColours
    {
        sl::Colour background;
        sl::Colour panel;
        sl::Colour border;
        sl::Colour heading;
        sl::Colour text;
        sl::Colour accent;
        sl::Colour muted;
    };

    ThemeColours get_theme(const std::string &theme_name)
    {
        if (theme_name == "Amber")
        {
            return {
                {30, 24, 16}, {45, 36, 24}, {90, 72, 45},
                {255, 205, 80}, {245, 230, 205}, {255, 175, 40}, {170, 145, 115}
            };
        }
        if (theme_name == "Forest")
        {
            return {
                {18, 28, 22}, {26, 42, 32}, {50, 85, 65},
                {150, 235, 165}, {220, 245, 225}, {100, 215, 125}, {130, 175, 145}
            };
        }
        if (theme_name == "Cyberpunk")
        {
            return {
                {20, 16, 32}, {32, 24, 52}, {85, 55, 130},
                {255, 95, 180}, {230, 235, 255}, {75, 220, 255}, {160, 145, 190}
            };
        }
        // Default Dark theme
        return {
            {20, 24, 34}, {28, 35, 48}, {60, 75, 100},
            {235, 220, 155}, {218, 226, 235}, {90, 190, 255}, {145, 160, 178}
        };
    }
}

int main(int argc, char *argv[])
{
    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 960, 640))
    {
        return -1;
    }

    sl::set_window_title("simlib - JSON Configuration (exconfig)");

    sl::Config config;
    if (!config.load(CONFIG_FILE))
    {
        init_default_config(config);
        config.save(CONFIG_FILE);
    }

    std::string status_message = "Configuration loaded from " + std::string(CONFIG_FILE);
    sl::Colour status_colour = {120, 220, 140};

    const std::vector<std::string> difficulties = {"Easy", "Normal", "Hard", "Nightmare"};
    const std::vector<std::string> themes = {"Dark", "Amber", "Forest", "Cyberpunk"};

    bool running = true;
    sl::set_fps(60);

    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape))
            {
                running = false;
            }
            else if (event.type() == sl::Event::Type::key_down && !event.key_repeat())
            {
                // S: Save configuration
                if (event.key() == sl::Event::Key::letter_s)
                {
                    if (config.save(CONFIG_FILE))
                    {
                        status_message = "Saved to " + std::string(CONFIG_FILE);
                        status_colour = {120, 220, 140};
                    }
                    else
                    {
                        status_message = "Failed to save: " + sl::last_error();
                        status_colour = {245, 100, 90};
                    }
                }
                // R: Reload configuration
                else if (event.key() == sl::Event::Key::letter_r)
                {
                    if (config.load(CONFIG_FILE))
                    {
                        status_message = "Reloaded from " + std::string(CONFIG_FILE);
                        status_colour = {120, 220, 140};
                    }
                    else
                    {
                        status_message = "Failed to load: " + sl::last_error();
                        status_colour = {245, 100, 90};
                    }
                }
                // D: Reset to defaults
                else if (event.key() == sl::Event::Key::letter_d)
                {
                    init_default_config(config);
                    status_message = "Reset to default settings (press S to save)";
                    status_colour = {255, 215, 90};
                }
                // 1/2: Adjust master volume
                else if (event.key() == sl::Event::Key::digit_1 || event.key() == sl::Event::Key::keypad_1)
                {
                    double vol = config.get<double>("audio.master_volume", 0.5);
                    vol = std::max(0.0, vol - 0.05);
                    config.set("audio.master_volume", vol);
                    status_message = "Master volume decreased";
                    status_colour = {218, 226, 235};
                }
                else if (event.key() == sl::Event::Key::digit_2 || event.key() == sl::Event::Key::keypad_2)
                {
                    double vol = config.get<double>("audio.master_volume", 0.5);
                    vol = std::min(1.0, vol + 0.05);
                    config.set("audio.master_volume", vol);
                    status_message = "Master volume increased";
                    status_colour = {218, 226, 235};
                }
                // 3: Toggle VSync
                else if (event.key() == sl::Event::Key::digit_3 || event.key() == sl::Event::Key::keypad_3)
                {
                    bool vsync = config.get<bool>("video.vsync", true);
                    config.set("video.vsync", !vsync);
                    status_message = "VSync toggled";
                    status_colour = {218, 226, 235};
                }
                // 4: Cycle difficulty
                else if (event.key() == sl::Event::Key::digit_4 || event.key() == sl::Event::Key::keypad_4)
                {
                    std::string current = config.get<std::string>("gameplay.difficulty", "Normal");
                    auto it = std::find(difficulties.begin(), difficulties.end(), current);
                    std::size_t idx = (it != difficulties.end()) ? std::distance(difficulties.begin(), it) : 0;
                    idx = (idx + 1) % difficulties.size();
                    config.set("gameplay.difficulty", difficulties[idx]);
                    status_message = "Difficulty set to " + difficulties[idx];
                    status_colour = {218, 226, 235};
                }
                // 5/6: Adjust player lives
                else if (event.key() == sl::Event::Key::digit_5 || event.key() == sl::Event::Key::keypad_5)
                {
                    int lives = config.get<int>("gameplay.player.lives", 3);
                    lives = std::max(1, lives - 1);
                    config.set("gameplay.player.lives", lives);
                    status_message = "Player lives decreased";
                    status_colour = {218, 226, 235};
                }
                else if (event.key() == sl::Event::Key::digit_6 || event.key() == sl::Event::Key::keypad_6)
                {
                    int lives = config.get<int>("gameplay.player.lives", 3);
                    lives = std::min(9, lives + 1);
                    config.set("gameplay.player.lives", lives);
                    status_message = "Player lives increased";
                    status_colour = {218, 226, 235};
                }
                // T: Cycle UI theme
                else if (event.key() == sl::Event::Key::letter_t)
                {
                    std::string current = config.get<std::string>("ui.theme", "Dark");
                    auto it = std::find(themes.begin(), themes.end(), current);
                    std::size_t idx = (it != themes.end()) ? std::distance(themes.begin(), it) : 0;
                    idx = (idx + 1) % themes.size();
                    config.set("ui.theme", themes[idx]);
                    status_message = "Theme changed to " + themes[idx];
                    status_colour = {218, 226, 235};
                }
            }
            sl::display_handle_event(event);
        }

        // Retrieve current active settings from the Config object
        const std::string theme_name = config.get<std::string>("ui.theme", "Dark");
        const ThemeColours theme = get_theme(theme_name);

        const int vid_w = config.get<int>("video.width", 800);
        const int vid_h = config.get<int>("video.height", 600);
        const bool vid_vsync = config.get<bool>("video.vsync", true);
        const double master_vol = config.get<double>("audio.master_volume", 0.75);
        const double music_vol = config.get<double>("audio.music_volume", 0.60);
        const double sfx_vol = config.get<double>("audio.sfx_volume", 0.80);
        const std::string difficulty = config.get<std::string>("gameplay.difficulty", "Normal");
        const std::string player_name = config.get<std::string>("gameplay.player.name", "Unknown");
        const int player_lives = config.get<int>("gameplay.player.lives", 3);
        const int player_score = config.get<int>("gameplay.player.score", 0);

        // Render Frame
        sl::clear_to_colour(sl::screen, theme.background);

        // Header
        sl::gprintf(24, 20, theme.heading, "JSON Configuration Subsystem (sl::Config)");
        sl::gprintf(24, 44, theme.muted, "Demonstrating nested dotted-path keys, typed serialization, and file I/O");

        // --- Left Panel: Visualized Settings ---
        const float p1_left = 24.0f;
        const float p1_top = 74.0f;
        const float p1_right = 460.0f;
        const float p1_bottom = 580.0f;

        sl::rectfill(sl::screen, p1_left, p1_top, p1_right, p1_bottom, theme.panel);
        sl::rect(sl::screen, p1_left, p1_top, p1_right, p1_bottom, theme.border);

        sl::gprintf(static_cast<int>(p1_left + 16), static_cast<int>(p1_top + 14), theme.heading, "Parsed Application Settings");

        int y = static_cast<int>(p1_top + 42);

        // Video
        sl::gprintf(static_cast<int>(p1_left + 16), y, theme.accent, "[video]");
        y += 20;
        sl::gprintf(static_cast<int>(p1_left + 24), y, theme.text, "Resolution:  %d x %d", vid_w, vid_h);
        y += 18;
        sl::gprintf(static_cast<int>(p1_left + 24), y, theme.text, "VSync (3):   %s", vid_vsync ? "Enabled" : "Disabled");
        y += 26;

        // Audio with visual meter bar
        sl::gprintf(static_cast<int>(p1_left + 16), y, theme.accent, "[audio]");
        y += 20;
        sl::gprintf(static_cast<int>(p1_left + 24), y, theme.text, "Master (1/2): %.0f %%", master_vol * 100.0);
        // Draw volume bar
        const float bar_x = p1_left + 150.0f;
        const float bar_w = 160.0f;
        sl::rectfill(sl::screen, bar_x, static_cast<float>(y + 2), bar_x + bar_w, static_cast<float>(y + 12), {15, 18, 24});
        sl::rectfill(sl::screen, bar_x, static_cast<float>(y + 2), bar_x + bar_w * static_cast<float>(master_vol), static_cast<float>(y + 12), theme.accent);
        sl::rect(sl::screen, bar_x, static_cast<float>(y + 2), bar_x + bar_w, static_cast<float>(y + 12), theme.border);
        y += 18;
        sl::gprintf(static_cast<int>(p1_left + 24), y, theme.text, "Music:       %.0f %%", music_vol * 100.0);
        y += 18;
        sl::gprintf(static_cast<int>(p1_left + 24), y, theme.text, "SFX:         %.0f %%", sfx_vol * 100.0);
        y += 26;

        // Gameplay
        sl::gprintf(static_cast<int>(p1_left + 16), y, theme.accent, "[gameplay]");
        y += 20;
        sl::gprintf(static_cast<int>(p1_left + 24), y, theme.text, "Difficulty (4): %s", difficulty.c_str());
        y += 18;
        sl::gprintf(static_cast<int>(p1_left + 24), y, theme.text, "Player Name:    %s", player_name.c_str());
        y += 18;
        sl::gprintf(static_cast<int>(p1_left + 24), y, theme.text, "Lives (5/6):    %d", player_lives);
        y += 18;
        sl::gprintf(static_cast<int>(p1_left + 24), y, theme.text, "Score:          %d", player_score);
        y += 26;

        // UI & Theme
        sl::gprintf(static_cast<int>(p1_left + 16), y, theme.accent, "[ui]");
        y += 20;
        sl::gprintf(static_cast<int>(p1_left + 24), y, theme.text, "Theme (T):      %s", theme_name.c_str());
        // Theme swatch preview
        sl::rectfill(sl::screen, p1_left + 160.0f, static_cast<float>(y + 2), p1_left + 200.0f, static_cast<float>(y + 14), theme.accent);
        sl::rect(sl::screen, p1_left + 160.0f, static_cast<float>(y + 2), p1_left + 200.0f, static_cast<float>(y + 14), theme.border);
        y += 34;

        // Hotkey help summary
        sl::gprintf(static_cast<int>(p1_left + 16), y, theme.heading, "Controls:");
        y += 18;
        sl::gprintf(static_cast<int>(p1_left + 16), y, theme.muted, "S: Save file    R: Reload file    D: Reset defaults");
        y += 16;
        sl::gprintf(static_cast<int>(p1_left + 16), y, theme.muted, "1/2: Volume     3: VSync          4: Difficulty");
        y += 16;
        sl::gprintf(static_cast<int>(p1_left + 16), y, theme.muted, "5/6: Lives      T: Cycle theme    Escape: Exit");

        // --- Right Panel: Raw JSON Document Dump ---
        const float p2_left = 480.0f;
        const float p2_top = 74.0f;
        const float p2_right = 936.0f;
        const float p2_bottom = 580.0f;

        sl::rectfill(sl::screen, p2_left, p2_top, p2_right, p2_bottom, {14, 17, 24});
        sl::rect(sl::screen, p2_left, p2_top, p2_right, p2_bottom, theme.border);

        sl::gprintf(static_cast<int>(p2_left + 16), static_cast<int>(p2_top + 14), theme.heading, "Raw JSON Document: %s", CONFIG_FILE);

        // Dump formatted JSON lines
        const std::string json_dump = config.document().dump(2);
        std::istringstream stream(json_dump);
        std::string line;
        int json_y = static_cast<int>(p2_top + 40);
        int line_count = 0;

        while (std::getline(stream, line) && line_count < 28)
        {
            sl::Colour line_col = {180, 195, 215};
            if (line.find(':') != std::string::npos)
            {
                line_col = {210, 225, 245};
            }
            if (line.find('{') != std::string::npos || line.find('}') != std::string::npos)
            {
                line_col = theme.accent;
            }
            sl::gprintf(static_cast<int>(p2_left + 16), json_y, line_col, "%s", line.c_str());
            json_y += 17;
            ++line_count;
        }

        // Bottom status bar
        sl::gprintf(24, 600, status_colour, "Status: %s", status_message.c_str());

        sl::show_video_bitmap();
        sl::end_frame();
    }

    sl::wait_for_graphics();
    sl::shutdown();
    return 0;
}

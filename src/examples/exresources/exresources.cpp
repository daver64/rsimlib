#include "sl.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
    void process_input(bool &running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit ||
                (event.type() == sl::Event::Type::key_down &&
                 event.key() == sl::Event::Key::escape))
                running = false;
            sl::display_handle_event(event);
        }
    }

    void draw_screen(const std::string &archive_path, const sl::Archive &archive,
                     sl::Bitmap *texture, const std::string &script_preview)
    {
        sl::clear_to_colour(sl::screen, {20, 26, 36});
        const sl::Colour heading{235, 220, 155};
        const sl::Colour text{218, 226, 235};
        const sl::Colour muted{145, 160, 178};
        const std::vector<std::string> entries = archive.entries();

        sl::gprintf(24, 20, heading, "Resource archive example");
        sl::gprintf(24, 48, muted, "Archive: %s", archive_path.c_str());
        sl::gprintf(24, 72, muted, "Entries: %d    Press Escape to exit", static_cast<int>(entries.size()));
        sl::gprintf(24, 108, heading, "Archive directory");

        const int visible_entries = std::min(13, static_cast<int>(entries.size()));
        for (int index = 0; index < visible_entries; ++index)
            sl::gprintf(24, 134 + index * 22, text, "%s", entries[static_cast<std::size_t>(index)].c_str());
        if (entries.size() > static_cast<std::size_t>(visible_entries))
            sl::gprintf(24, 134 + visible_entries * 22, muted, "... and %d more",
                static_cast<int>(entries.size()) - visible_entries);

        if (texture)
        {
            sl::draw_sprite_stretched(texture, 600.0f, 170.0f, 160, 160);
            sl::gprintf_center(350, text, "Texture loaded from archive");
        }
        else
        {
            sl::gprintf(520, 220, {240, 130, 120}, "Texture entry unavailable");
        }
        sl::gprintf(24, 510, muted, "scene.lua preview: %s", script_preview.c_str());
        sl::show_video_bitmap();
        sl::end_frame();
    }
}

int main(int argc, char *argv[])
{
    const std::string archive_path = argc > 1 ? argv[1] : "resource_demo.zip";
    sl::Archive archive;
    if (!archive.open(archive_path))
    {
        std::fprintf(stderr, "Unable to open %s: %s\n", archive_path.c_str(), sl::last_error().c_str());
        return -1;
    }

    const std::string texture_entry = archive.contains("assets/textures/balloon_red.png")
        ? "assets/textures/balloon_red.png" : "textures/balloon_red.png";
    const std::string script_entry = archive.contains("assets/scripts/scene.lua")
        ? "assets/scripts/scene.lua" : "scripts/scene.lua";
    sl::Bitmap *texture = sl::load_bitmap(archive, texture_entry);
    const std::vector<std::uint8_t> script_bytes = archive.read(script_entry);
    std::string script_preview(script_bytes.begin(), script_bytes.begin() +
        std::min<std::size_t>(script_bytes.size(), 70));
    std::replace(script_preview.begin(), script_preview.end(), '\n', ' ');
    if (script_preview.empty()) script_preview = "not found";

    if (!sl::configure_graphics_backend_from_args(argc, argv) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 800, 600))
    {
        sl::destroy_bitmap(texture);
        return -1;
    }

    bool running = true;
    while (running)
    {
        process_input(running);
        draw_screen(archive_path, archive, texture, script_preview);
    }
    sl::wait_for_graphics();
    sl::destroy_bitmap(texture);
    sl::shutdown();
    archive.close();
    return 0;
}
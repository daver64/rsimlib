// Converts AC3D / FlightGear models (and re-saves glTF) to a binary glTF (.glb) that Blender 4.x can import.
//   simlib_convert in.ac|in.xml|in.obj|in.glb out.glb [--animations out.json] [--gl|--vulkan]
// A small window is opened because meshes are created through the graphics backend.
#include "sl.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    std::string input, output, animations;
    char default_backend[] = "--gl";
    std::vector<char *> backend_arguments{argv[0], default_backend};
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument(argv[i]);
        if (argument == "--gl" || argument == "--vulkan" || argument == "--d3d12")
            backend_arguments.push_back(argv[i]);
        else if (argument == "--animations" && i + 1 < argc)
            animations = argv[++i];
        else if (input.empty())
            input = argument;
        else if (output.empty())
            output = argument;
        else
        {
            std::fprintf(stderr, "Unexpected argument: %s\n", argument.c_str());
            return 2;
        }
    }
    if (input.empty() || output.empty())
    {
        std::fprintf(stderr, "Usage: simlib_convert <in.ac|in.xml|in.obj|in.glb> <out.glb> [--animations out.json] [--gl|--vulkan]\n");
        return 2;
    }
    if (!sl::configure_graphics_backend_from_args(static_cast<int>(backend_arguments.size()), backend_arguments.data()) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 64, 64))
    {
        std::fprintf(stderr, "Graphics initialization failed: %s\n", sl::last_error().c_str());
        return 1;
    }
    sl::set_mesh_cpu_data_retained(true);
    std::string extension = std::filesystem::path(input).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    sl::Model *model = extension == ".xml" ? sl::load_flightgear_model(input) : sl::load_model(input);
    int status = 0;
    if (!model)
    {
        std::fprintf(stderr, "Load failed: %s\n", sl::last_error().c_str());
        status = 1;
    }
    else
    {
        if (!sl::save_model_gltf(model, output))
        {
            std::fprintf(stderr, "Save failed: %s\n", sl::last_error().c_str());
            status = 1;
        }
        else
        {
            std::printf("Wrote %s (%zu parts, %zu clips)\n", output.c_str(), model->parts.size(), model->clips.size());
            if (!model->animations.empty() || !model->visibility.empty())
                std::printf("Note: %zu FlightGear property animation(s) and %zu visibility rule(s) have no glTF equivalent%s\n",
                            model->animations.size(), model->visibility.size(),
                            animations.empty() ? "; pass --animations file.json to keep them in simlib's JSON format" : "");
            if (!model->unsupported_animations.empty())
                std::printf("Note: %zu animation(s) were not imported from the source model\n", model->unsupported_animations.size());
        }
        if (status == 0 && !animations.empty() && !sl::save_model_animations_json(model, animations))
        {
            std::fprintf(stderr, "Animation JSON save failed: %s\n", sl::last_error().c_str());
            status = 1;
        }
        sl::destroy_model(model);
    }
    sl::wait_for_graphics();
    sl::shutdown();
    return status;
}

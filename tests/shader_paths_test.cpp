#include "shader_paths.h"

#include <SDL2/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

int main()
{
    const fs::path root = fs::temp_directory_path() / "simlib_shader_paths_test";
    fs::remove_all(root);
    fs::create_directories(root / "shaders" / "glsl");
    std::ofstream(root / "shaders" / "glsl" / "relocated_probe.frag") << "// probe\n";

    SDL_setenv("SIMLIB_DATA_DIR", root.string().c_str(), 1);

    const fs::path found = sl::detail::find_shader_root("glsl/relocated_probe.frag");
    const bool env_first = sl::detail::data_dirs().front() == root.lexically_normal();
    const bool found_ok = found == (root / "shaders").lexically_normal();
    const bool builtin_ok = fs::exists(sl::detail::find_shader_root("glsl/default_2d.vert") / "glsl/default_2d.vert");

    fs::remove_all(root);
    if (!env_first || !found_ok || !builtin_ok)
    {
        std::fprintf(stderr, "shader_paths_test failed (env_first=%d found=%d builtin=%d)\n", env_first, found_ok, builtin_ok);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

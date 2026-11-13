#include "shader_paths.h"

#include <SDL2/SDL.h>

#include <system_error>
#include <fstream>
#include <iterator>

namespace sl::detail
{
    namespace
    {
        std::vector<std::filesystem::path> build_data_dirs()
        {
            std::vector<std::filesystem::path> dirs;
            auto add = [&dirs](const std::filesystem::path &path)
            {
                if (!path.empty())
                    dirs.push_back(path.lexically_normal());
            };

            if (const char *env = SDL_getenv("SIMLIB_DATA_DIR"))
                add(env);

            if (char *base = SDL_GetBasePath())
            {
                const std::filesystem::path exe_dir = base;
                SDL_free(base);
                add(exe_dir);
                add(exe_dir / "share" / "simlib");
                add(exe_dir / ".." / "share" / "simlib");
            }

#ifdef SIMLIB_INSTALL_DATA_DIR
            add(SIMLIB_INSTALL_DATA_DIR);
#endif
#ifdef SIMLIB_BUILD_SOURCE_DIR
            add(SIMLIB_BUILD_SOURCE_DIR);
#endif
#ifdef SIMLIB_BUILD_BINARY_DIR
            add(SIMLIB_BUILD_BINARY_DIR);
#endif
            return dirs;
        }
    }

    const std::vector<std::filesystem::path> &data_dirs()
    {
        static const std::vector<std::filesystem::path> dirs = build_data_dirs();
        return dirs;
    }

    std::filesystem::path find_shader_root(const std::filesystem::path &probe)
    {
        std::error_code ec;
        for (const auto &dir : data_dirs())
        {
            if (std::filesystem::exists(dir / "shaders" / probe, ec))
                return dir / "shaders";
        }
        const auto &dirs = data_dirs();
        return dirs.empty() ? std::filesystem::path("shaders") : dirs.front() / "shaders";
    }

    std::string load_glsl_shader(const char *name)
    {
        const std::filesystem::path relative = std::filesystem::path("glsl") / name;
#ifdef __ANDROID__
        std::ifstream external_file(find_shader_root(relative) / relative);
        if (external_file)
            return std::string(std::istreambuf_iterator<char>(external_file), {});
        const std::string asset_path = (std::filesystem::path("shaders") / relative).generic_string();
        std::size_t size = 0;
        void *data = SDL_LoadFile(asset_path.c_str(), &size);
        if (!data)
            return {};
        std::string source(static_cast<const char *>(data), size);
        SDL_free(data);
        return source;
#else
        std::ifstream file(find_shader_root(relative) / relative);
        return file ? std::string(std::istreambuf_iterator<char>(file), {}) : std::string{};
#endif
    }
} // namespace sl::detail

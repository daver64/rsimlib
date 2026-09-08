#pragma once

#include "draw.h"
#include "lua_runtime.h"

#include <string>

namespace simlib
{
    class LuaCanvas
    {
    public:
        LuaCanvas();
        ~LuaCanvas();

        LuaCanvas(const LuaCanvas &) = delete;
        LuaCanvas &operator=(const LuaCanvas &) = delete;

        void initialise(LuaRuntime::OutputHandler output_handler = {});
        bool is_initialised() const;
        LuaScriptResult run_text(const std::string &script);
        LuaScriptResult run_file(const std::string &path);
        LuaRuntime &runtime();
        void set_asset_root(const std::string &path);
        void render(Bitmap *target) const;
        void clear();
        void reset();

    private:
        struct Implementation;
        Implementation *implementation_;
    };
}
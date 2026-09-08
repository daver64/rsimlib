#pragma once

#include <functional>
#include <memory>
#include <string>

namespace sol
{
    class state;
}

namespace simlib
{
    struct LuaScriptResult
    {
        bool success = false;
        std::string error;
    };

    class LuaRuntime
    {
    public:
        using OutputHandler = std::function<void(const std::string &)>;

        LuaRuntime();
        ~LuaRuntime();

        LuaRuntime(const LuaRuntime &) = delete;
        LuaRuntime &operator=(const LuaRuntime &) = delete;

        void initialise(OutputHandler output_handler = {});
        bool is_initialised() const;
        LuaScriptResult execute(const std::string &script);
        sol::state &state();
        void reset();

    private:
        std::unique_ptr<sol::state> state_;
        OutputHandler output_handler_;
    };
}
#pragma once

#include <functional>
#include <memory>
#include <string>

namespace sol
{
    class state;
}

namespace sl
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
        /** Invoke an optional global Lua callback with a string argument. */
        LuaScriptResult call(const std::string &function_name, const std::string &argument);
        /** Emit an optional Lua callback with no arguments. */
        LuaScriptResult emit(const std::string &event_name);
        /** Emit an optional Lua callback with an integer argument. */
        LuaScriptResult emit(const std::string &event_name, int value);
        /** Emit an optional Lua callback with a floating-point argument. */
        LuaScriptResult emit(const std::string &event_name, float value);
        /** Emit an optional Lua callback with a boolean argument. */
        LuaScriptResult emit(const std::string &event_name, bool value);
        /** Emit an optional Lua callback with a string argument. */
        LuaScriptResult emit(const std::string &event_name, const std::string &value);
        /** Emit an optional Lua callback with a named two-dimensional position. */
        LuaScriptResult emit(const std::string &event_name, const std::string &name, float x, float y);
        sol::state &state();
        void reset();

    private:
        std::unique_ptr<sol::state> state_;
        OutputHandler output_handler_;
    };
}
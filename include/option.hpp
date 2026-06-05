#pragma once

#include <functional>
#include <string>

class ConsoleHelper;

struct RestartException {};

namespace OptionContext {
    inline std::string webhook_url = "";
    inline std::string bot_token   = "";
    inline std::string bot_guild   = "";
}

struct Option {
    std::string name;
    int type = 0;
    std::function<void(ConsoleHelper*)> run;
    std::string plugin_name = ""; 
    int lua_callback_ref = -1;

    Option() = default;
    Option(std::string name, int type, std::function<void(ConsoleHelper*)> run)
        : name(std::move(name)), type(type), run(std::move(run)) {}
};
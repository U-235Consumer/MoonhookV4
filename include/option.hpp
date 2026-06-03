// option.hpp

#pragma once

#include <functional>
#include <console.hpp>
#include <exception>

struct RestartException : std::exception {
    const char* what() const noexcept override { return "restart"; }
};

struct OptionContext {
    inline static std::string webhook_url;
    inline static std::string bot_token;
    inline static std::string bot_guild;
};

struct Option {
    std::string name;
    int type; // 0 - main menu, 1 - webhooks, 2 - bots
    std::function<void(ConsoleHelper*)> action;

    Option(const std::string& name, int type, std::function<void(ConsoleHelper*)> action)
        : name(name), action(action), type(type) {}

    void run(ConsoleHelper* console) {
        action(console);
    }

    int lua_callback_ref;
};

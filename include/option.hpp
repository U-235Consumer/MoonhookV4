// option.hpp

#pragma once

#include <functional>
#include <console.hpp>
#include <exception>

struct RestartException : std::exception {
    const char* what() const noexcept override { return "restart"; }
};

struct Option {
    std::string name;
    std::function<void(ConsoleHelper*)> action;

    Option(const std::string& name, std::function<void(ConsoleHelper*)> action)
        : name(name), action(action) {}

    void run(ConsoleHelper* console) {
        action(console);
    }
};

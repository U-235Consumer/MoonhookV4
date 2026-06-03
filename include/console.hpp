#pragma once

#include <iostream>
#include <ansi_terminal.hpp>
#include <chrono>
#include <string>

class ConsoleHelper {
public:
    ansi::Gradient gmain;
    std::string banner;

    ConsoleHelper(const ansi::Gradient& gradient, const std::string& banner)
        : gmain(gradient), banner(banner) {}

    void set_gradient(const ansi::Gradient& gradient) { gmain = gradient; }
    void set_banner(const std::string& new_banner) { banner = new_banner; }

    inline void log(const std::string& text)
    {
        std::cout << gmain.start << "(" << get_timestamp() << ")[MoonHook]: " << ansi::ColorReset() << text << std::endl;
    }

    inline void error(const std::string& text)
    {
        std::cout << ansi::rgb_to_ansi(255, 0, 0) << "(" << get_timestamp() << ")[MoonHook]: " << ansi::ColorReset() << text << std::endl;
    }

    inline void printbanner()
    {
        ansi::print_gradient_ascii(banner, gmain);
    }

    inline std::string input(const std::string& text)
    {
        std::cout << gmain.start << "[>>][MoonHook]: " << ansi::ColorReset() << text;
        std::string out;
        std::getline(std::cin, out);
        return out;
    }

    inline int int_input(const std::string& text)
    {
        std::cout << gmain.start << "[>>][MoonHook]: " << ansi::ColorReset() << text;
        std::string txt;
        std::getline(std::cin, txt);
        
        try {
            return std::stoi(txt);
        }
        catch (...) {
            std::cout << "Invalid number! Using 0 instead.\n";
            return 0;
        }
    }

private:
    inline std::string get_timestamp() 
    {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm* tm = std::localtime(&t);
        
        char buf[6]; 
        if (std::strftime(buf, sizeof(buf), "%H:%M", tm)) {
            return std::string(buf);
        }
        return "00:00";
    }
};

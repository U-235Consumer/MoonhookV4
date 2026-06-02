#ifndef COLORS
#define COLORS

#define NOMINMAX

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

#if defined(_WIN32)
    #include <windows.h>
    #include <conio.h>
#elif defined(__unix__) || defined(__APPLE__)
    #include <unistd.h>
    #include <termios.h>
    #include <cstdlib>
#endif

namespace ansi {

    struct Gradient {
        std::string start;
        std::string end;
    };

    inline void enableANSI() {
#if defined(_WIN32)
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
#endif
    }

    inline bool supportsColor() {
#if defined(_WIN32)
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        return GetConsoleMode(hOut, &dwMode) &&
               (dwMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#elif defined(__unix__) || defined(__APPLE__)
        const char* term = std::getenv("TERM");
        const char* colorterm = std::getenv("COLORTERM");
        bool hasTerm = term && std::string(term) != "dumb";
        bool hasColor = colorterm != nullptr;
        return isatty(STDOUT_FILENO) && (hasTerm || hasColor);
#else
        return false;
#endif
    }

    inline std::string rgb_to_ansi(int r, int g, int b) {
        return "\033[38;2;" + std::to_string(r) + ";" +
               std::to_string(g) + ";" +
               std::to_string(b) + "m";
    }

    inline std::string ColorReset() {
        return "\033[0m";
    }

    inline void print_gradient_ascii(const std::string& text, const Gradient& gradient) {
        auto parseRGB = [](const std::string& ansi_str, int& r, int& g, int& b) {
            size_t pos = ansi_str.find("38;2;");
            if (pos == std::string::npos) { r = g = b = 255; return; }
            pos += 5;
            std::sscanf(ansi_str.c_str() + pos, "%d;%d;%dm", &r, &g, &b);
        };

        int r1, g1, b1, r2, g2, b2;
        parseRGB(gradient.start, r1, g1, b1);
        parseRGB(gradient.end,   r2, g2, b2);

        std::stringstream ss(text);
        std::string line;
        std::vector<std::string> lines;

        while (std::getline(ss, line))
            lines.push_back(line);

        size_t max_length = 0;
        for (const auto& l : lines) {
            size_t count = 0;
            for (size_t i = 0; i < l.size(); ) {
                unsigned char c = (unsigned char)l[i];
                if      (c >= 0xF0) i += 4;
                else if (c >= 0xE0) i += 3;
                else if (c >= 0xC0) i += 2;
                else                i += 1;
                count++;
            }
            max_length = (std::max)(max_length, count);
        }

        for (const auto& l : lines) {
            if (l.empty()) {
                std::cout << "\n";
                continue;
            }

            std::string gradient_line;
            size_t char_index = 0;
            size_t i = 0;

            while (i < l.size()) {
                unsigned char c = (unsigned char)l[i];
                int char_bytes = 1;
                if      (c >= 0xF0) char_bytes = 4;
                else if (c >= 0xE0) char_bytes = 3;
                else if (c >= 0xC0) char_bytes = 2;

                float progress = max_length > 1
                    ? (float)char_index / (float)(max_length - 1)
                    : 0.0f;

                int r = (int)(r1 + (r2 - r1) * progress);
                int g = (int)(g1 + (g2 - g1) * progress);
                int b = (int)(b1 + (b2 - b1) * progress);

                gradient_line += rgb_to_ansi(r, g, b);
                gradient_line += l.substr(i, char_bytes); 

                i += char_bytes;
                char_index++;
            }

            std::cout << gradient_line << ColorReset() << "\n";
        }
    }

    inline void clearConsole() {
        std::cout << "\033[H\033[2J\033[3J" << std::flush;
    }

    inline void pause(const std::string& message = "Press any key to continue...") {
        if (!message.empty())
            std::cout << message << std::flush;

#if defined(_WIN32)
        _getch();
#else
        struct termios oldt, newt;
        if (tcgetattr(STDIN_FILENO, &oldt) == 0) {
            newt = oldt;
            newt.c_lflag &= ~(ICANON | ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &newt);
            std::getchar();
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        } else {
            std::string fallback;
            std::getline(std::cin, fallback);
        }
#endif
        std::cout << "\n";
    }
}

#endif // COLORS
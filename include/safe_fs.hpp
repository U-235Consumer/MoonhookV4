#pragma once

#include <iostream>
#include <vector>
#include <filesystem>

namespace SafeFilesystem
{
    inline constexpr std::size_t MAX_FILE_SIZE = 10 * 1024 * 1024; // 10MB
    inline const std::vector<std::string> BLOCKED_EXTENSIONS = {
        ".exe", ".scr", ".vbs", ".bat", ".cmd", ".com", ".msi",
        ".ps1", ".psm1", ".psd1",
        ".vbe", ".wsf", ".wsh", ".js", ".jse", ".hta",
        ".cpl", ".msc", ".reg", ".inf",
        ".lnk", ".pif",
        ".dll", ".sys",
        ".jar", ".py", ".sh", ".rb", ".application", ".gadget",
        ".ws", ".sct", ".crt"
    };
    std::string& get_workspace();
    bool is_blocked_extension(const std::string& path);
    bool resolve_safe_path(const std::string& relative, std::filesystem::path& out);

    bool write(const std::string& path, const std::string& content);
    std::string read(const std::string& path);
    bool append(const std::string& path, const std::string& content);

    bool make_directories(const std::string& path);
    bool delete_directory(const std::string& path);

    extern std::string last_error;
}

#include "safe_fs.hpp"

#include <filesystem>
#include <fstream>

std::string& get_workspace()
{
    static std::string workspace;
    if (!workspace.empty())
        return workspace;

    workspace = "./Plugins/Workspace";
    std::filesystem::create_directories(workspace);

    return workspace;
}

bool SafeFilesystem::is_blocked_extension(const std::string &path)
{
    try {
        std::filesystem::path p(path);
        std::string filename = p.filename().string();

        if (filename.find(':') != std::string::npos)
            return true;

        while (!filename.empty() &&
            (filename.back() == ' ' || filename.back() == '.'))
        {
            filename.pop_back();
        }

        std::filesystem::path normalized(filename);
        std::string ext = normalized.extension().string();

        for (char& c : ext)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        for (const auto& blocked : BLOCKED_EXTENSIONS)
            if (ext == blocked)
                return true;

        return false;
    } catch (...) { return true; }
}

bool SafeFilesystem::resolve_safe_path(const std::string &relative, std::filesystem::path &out)
{
    try {
        auto base = std::filesystem::weakly_canonical(get_workspace());
        auto target = std::filesystem::weakly_canonical(base / relative);

        if (std::mismatch(base.begin(), base.end(),
            target.begin(), target.end()).first != base.end())
        return false;

        if (target.filename().string().find(':') != std::string::npos)
            return false;

        out = target;
        return true;
    } catch (...) { return false; }
}

bool SafeFilesystem::write(const std::string &path, const std::string &content)
{
    bool success = false;
    try {
        std::filesystem::path resolved;
        resolve_safe_path(path, resolved);

        std::ofstream outfile(resolved.string());
        if (!outfile.is_open())
        {
            last_error = "Failed to open file!";
            return;
        }
        if (is_blocked_extension(path))
        {
            last_error = "Attempted to write a file with a blocked extension!";
            return;
        }

        outfile << content;
        outfile.close();
        success = true;
    }
    catch (...)
    {
        last_error = "Failed to write to file!";
        success = false;
    }
    return success;
}

std::string SafeFilesystem::read(const std::string &path)
{
    std::string out = "ERROR";
    try {
        std::filesystem::path resolved;
        resolve_safe_path(path, resolved);

        std::ifstream file(resolved.string());
        if (!file.is_open())
        {
            last_error = "Failed to open file for reading!";
            return "ERROR";
        }
        if (is_blocked_extension(path))
        {
            last_error = "Attempted to read file with a blocked extension!";
            return "ERROR";
        }

        std::stringstream buff;
        buff << file.rdbuf();
        out = buff.str();
        file.close();
        return out;
    }
    catch (...)
    {
        last_error = "Failed to read file!";
        return "ERROR";
    }
    return out;
}

bool SafeFilesystem::append(const std::string &path, const std::string &content)
{
    bool success = false;
    try {
        std::filesystem::path resolved;
        resolve_safe_path(path, resolved);

        std::ofstream outfile(resolved.string(), std::ios::app);
        if (!outfile.is_open())
        {
            last_error = "Failed to open file!";
            return;
        }
        if (is_blocked_extension(path))
        {
            last_error = "Attempted to append to a file with a blocked extension!";
            return;
        }

        outfile << content;
        outfile.close();
        success = true;
    }
    catch (...)
    {
        last_error = "Failed to append to file!";
    }
    return success;
}

bool SafeFilesystem::make_directories(const std::string &path)
{
    bool success = false;
    try {
        std::filesystem::path resolved;
        resolve_safe_path(path, resolved);
        std::filesystem::create_directories(resolved);
        success = true;
    }
    catch (...)
    {
        last_error = "Failed to create directory!";
        success = false;
    }
    return success;
}

bool SafeFilesystem::delete_directory(const std::string &path)
{
    bool success = false;
    try {
        std::filesystem::path resolved;
        resolve_safe_path(path, resolved);
        std::filesystem::remove(resolved);
        success = true;
    }
    catch (...)
    {
        last_error = "Failed to delete directory!";
    }
    return success;
}

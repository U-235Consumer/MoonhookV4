// omg moonhook v4
#define MOONHOOK_VERSION "v4.1.0"

#include <iostream>
#include <functional>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#include <ansi_terminal.hpp>
#include <consolerandom.hpp>
#include <console.hpp>
#include <option.hpp>
#include <internal_options.hpp>
#include <plugins.hpp>
#include <pluginregistry.hpp>

#include <lua.h>
#include <lualib.h>

const std::filesystem::path PluginsDir = "./Plugins";
const std::filesystem::path WorkspaceDir = PluginsDir / "Workspace";
const std::vector<std::string> PluginExtensions = {
    ".luau", ".lua", ".luauc", ".luac"
};
std::vector<MoonhookPlugin> UserPlugins = {};

std::vector<std::string> UserPluginNames = {};

const char* LUA_blocked[] = {
    "load", "loadstring", "dofile", "loadfile",
    "require", "collectgarbage", "newproxy", nullptr
};

int main()
{
    // std::filesystem::create_directories(PluginsDir);
    std::filesystem::create_directories(WorkspaceDir);

    lua_State* L = luaL_newstate();
    luaopen_base(L);
    luaopen_math(L);
    luaopen_string(L);
    luaopen_table(L);

    for (int i = 0; LUA_blocked[i] != nullptr; i++)
    {
        lua_pushnil(L);
        lua_setglobal(L, LUA_blocked[i]);
    }

    ansi::enableANSI();
    if (!ansi::supportsColor())
    {
        std::cout << "Your terminal does not support ANSI, please use a different terminal emulator to get better output.\n";
    }

    const std::string& MoonhookBanner = ConsoleRandom::GetBanner();
    const ansi::Gradient& MainGradient = ConsoleRandom::GetGradient();
    ConsoleHelper console(MainGradient, MoonhookBanner);

    if (!std::filesystem::is_empty(PluginsDir))
    {
        console.log("Getting plugins...");

        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(PluginsDir))
        {
            std::string ext = entry.path().extension().string();
            if (std::find(PluginExtensions.begin(), PluginExtensions.end(), ext) == PluginExtensions.end())
                continue;

            int type = 0;
            if (ext == ".luauc" || ext == ".luac")
                type = 1;

            std::ifstream plugin_file(entry.path().string());
            if (!plugin_file.is_open())
            {
                console.log("Failed to read plugin file: " + entry.path().string());
                ansi::pause();
                continue;
            }
            std::stringstream buff;
            buff << plugin_file.rdbuf();
            std::string content = buff.str();

            UserPlugins.push_back(MoonhookPlugin(content, type));
            console.log("Added plugin: " + entry.path().string());
        }
    }

    if (!UserPlugins.empty())
    {
        console.log("Loading plugins...");

        PluginEnvironment::install(L);

        for (MoonhookPlugin& plugin : UserPlugins)
        {
            auto header = plugin.parse_plugin_header();
            std::string plugin_name = header.has_value() && !header->name.empty()
                ? header->name
                : "Unknown Plugin";

            std::string result = plugin.get_bytecode();
            if (result.empty())
            {
                console.error("Plugin compile error: " + plugin.last_error());
                ansi::pause();
                continue;
            }

            if (luau_load(L, "plugin", result.data(), result.size(), 0) != LUA_OK)
            {
                console.error("Plugin load error: " + std::string(lua_tostring(L, -1)));
                lua_pop(L, 1);
                ansi::pause();
                continue;
            }

            PluginEnvironment::SetCurrentPluginName(plugin_name);

            if (lua_pcall(L, 0, 0, 0) != LUA_OK)
            {
                console.error("Plugin runtime error: " + std::string(lua_tostring(L, -1)));
                lua_pop(L, 1);
                PluginEnvironment::SetCurrentPluginName("");
                ansi::pause();
                continue;
            }

            PluginEnvironment::SetCurrentPluginName("");
            console.log("Plugin loaded successfully: " + plugin_name);
        }

        console.log("Loaded " + std::to_string(Registry::Get().GetOptions().size()) + " plugin options.");
    }

    while (true)
    {
        ansi::clearConsole();
        ansi::set_title("MoonHook V4");
        console.printbanner();

        std::cout << "----------------------------------------------------------------\n";
        std::cout << "Moonhook " << std::string(MOONHOOK_VERSION) << "!\n";
        std::cout << ConsoleRandom::GetText() << "\n";
        std::cout << "----------------------------------------------------------------\n\n";

        std::vector<Option> main_menu_options = {
            InternalOptions::Webhooks,
            InternalOptions::Bots
        };

        for (Option& op : Registry::Get().GetOptions())
        {
            if (op.type == 0)
                main_menu_options.push_back(op);
        }

        int counter = 1;
        std::cout << ansi::rgb_to_ansi(19, 195, 235) << "Internal\n" << ansi::ColorReset();
        for (int i = 0; i < 2 && i < (int)main_menu_options.size(); i++)
        {
            std::cout << "  " << counter++ << ". " << main_menu_options[i].name << "\n";
        }

        std::string last_group = "";
        for (int i = 2; i < (int)main_menu_options.size(); i++)
        {
            const std::string& group = main_menu_options[i].plugin_name;
            if (group != last_group)
            {
                std::string label = group.empty() ? "Internal" : ansi::rgb_to_ansi(19, 195, 235) + group + ansi::ColorReset();
                std::cout << label << "\n";
                last_group = group;
            }
            std::cout << "  " << counter++ << ". " << main_menu_options[i].name << "\n";
        }

        int exit_option_num = counter;
        std::cout << ansi::rgb_to_ansi(19, 195, 235) << "Exit" << ansi::ColorReset() << std::endl;
        std::cout << "  " << exit_option_num << ". Exit\n\n";

        std::cout << ansi::rgb_to_ansi(255, 255, 0)
                  << "Select an option (1-" << exit_option_num << "): "
                  << ansi::ColorReset();

        std::string selection;
        std::getline(std::cin, selection);

        int idx = 0;
        try {
            idx = std::stoi(selection) - 1;

            if (idx == exit_option_num - 1)
            {
                std::cout << "Exiting...\n";
                break;
            }

            if (idx < 0 || idx >= (int)main_menu_options.size())
            {
                console.error("Invalid selection!");
                ansi::pause();
                continue;
            }
        }
        catch (...) {
            console.error("Invalid selection!");
            ansi::pause();
            continue;
        }

        Option selected = main_menu_options[idx];
        try {
            ConsoleHelper* c = &console;
            selected.run(c);
        }
        catch (const RestartException&) {

        }
        catch (...) {
            std::cout << ansi::rgb_to_ansi(255, 0, 0) << "This option had errored!\n" << ansi::ColorReset();
            ansi::pause();
        }
    }

    lua_close(L);
    return 0;
}
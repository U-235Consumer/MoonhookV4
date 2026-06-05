#pragma once

#include <iostream>
#include <optional>

#include <option.hpp>

#include <lua.h>
#include <lualib.h>

struct MoonhookPlugin
{
    int type = 0; // 0 - source, 1 - compiled

    MoonhookPlugin(std::string content, int type = 0)
        : content(std::move(content)), type(type) {}

    std::string get_bytecode();
    std::string last_error();

    struct PluginHeader {
        std::string name;
        std::string description;
        std::string author;
        std::string version;
    };

    std::optional<PluginHeader> parse_plugin_header();
    std::string strip_plugin_header();

private:
    const std::string content;
    std::string bc;
    std::string last_err;
};

namespace PluginEnvironment
{
    extern const luaL_Reg MoonhookLibrary[];
    extern const luaL_Reg ConsoleLibrary[];
    extern const luaL_Reg FilesystemLibrary[];

    void install(lua_State* L);
    void SetCurrentPluginName(const std::string& name);
}
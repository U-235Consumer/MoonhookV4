#pragma once

#include <iostream>
#include <option.hpp>
#include <lua.h>
#include <lualib.h>

struct MoonhookPlugin
{
    int type = 0; // 0 - source, 1 - compiled
    
    MoonhookPlugin(std::string content, int type = 0)
        : content(std::move(content)), type(type) {}

    std::string get_bytecode();
    bool valid_bytecode();
    Option get_option();

    std::string last_error();

private:
    const std::string content;
    std::string bc;
    std::string last_err;
};

namespace PluginEnvironment
{
    extern const luaL_Reg MoonhookLibrary[];
    void install(lua_State* L);
};

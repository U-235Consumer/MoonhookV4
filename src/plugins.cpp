#include <plugins.hpp>
#include <option.hpp>
#include <pluginregistry.hpp>

#include <lua.h>
#include <lualib.h>
#include <Luau/Compiler.h>
#include <Luau/BytecodeBuilder.h>

std::string MoonhookPlugin::get_bytecode()
{
    if (type == 1)
        return content; 

    if (!bc.empty())
        return bc; 

    Luau::CompileOptions options;
    options.optimizationLevel = 1;
    options.debugLevel = 1;

    std::string bytecode = Luau::compile(content, options);

    if (!bytecode.empty() && bytecode[0] == '\0')
    {
        last_err = bytecode.substr(1);  
        return "";                        
    }

    bc = bytecode;
    return bc;
}

std::string MoonhookPlugin::last_error()
{
    return last_err;
}

//------------------------------------ LUAU FUNCTIONS ------------------------------------//

namespace option_index
{
    constexpr int MAIN_MENU_OPTION = 0;
    constexpr int WEBHOOKS_OPTION = 1;
    constexpr int BOTS_OPTION = 2;
}

// moonhook.Option("Name", moonhook.MAIN_MENU_OPTION, function (...) end)
static int l_moonhook_option(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    int type         = luaL_checkinteger(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    lua_pushvalue(L, 3);
    int ref = lua_ref(L, LUA_REGISTRYINDEX);
    Option opt(name, type, [L, ref](ConsoleHelper* console)
    {
        lua_getref(L, ref);
        if (lua_pcall(L, 0, 0, 0) != LUA_OK)
        {
            console->error("Option failed! Lua error: "+std::string(lua_tostring(L, -1)));
            lua_pop(L, 1);
        }
    });
    opt.lua_callback_ref = ref;
    Registry::Get().AddOption(std::move(opt));
    return 0;
}

const luaL_Reg PluginEnvironment::MoonhookLibrary[] = {
    {"Option", l_moonhook_option},
    {nullptr, nullptr}
};

//----------------------------------------------------------------------------------------//

void PluginEnvironment::install(lua_State *L)
{
    lua_newtable(L);

    for (const luaL_Reg* reg = MoonhookLibrary; reg->name != nullptr; reg++)
    {
        lua_pushcfunction(L, reg->func, reg->name);
        lua_setfield(L, -2, reg->name);
    }

    lua_pushinteger(L, option_index::MAIN_MENU_OPTION);
    lua_setfield(L, -2, "MAIN_MENU_OPTION");

    lua_pushinteger(L, option_index::BOTS_OPTION);
    lua_setfield(L, -2, "BOTS_OPTION");

    lua_pushinteger(L, option_index::WEBHOOKS_OPTION);
    lua_setfield(L, -2, "WEBHOOKS_OPTION");

    lua_setglobal(L, "moonhook");
}

#include <unordered_map>

#include <plugins.hpp>
#include <option.hpp>
#include <pluginregistry.hpp>
#include <option.hpp>
#include <ansi_terminal.hpp>
#include <console.hpp>
#include <safe_fs.hpp>

#include <lua.h>
#include <lualib.h>
#include <Luau/Compiler.h>
#include <Luau/BytecodeBuilder.h>

#ifdef lua_pushcfunction
#undef lua_pushcfunction
#endif
#define lua_pushcfunction(L, f) (lua_pushcclosure(L, (f), NULL, 0))

#ifdef luaL_error
#undef luaL_error
#endif
#define luaL_error(L, fmt, ...) (luaL_errorL(L, fmt, ##__VA_ARGS__), 0)

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <webhook.hpp>
#include <bot.hpp>
#include <thread>
#include <chrono>
using json = nlohmann::json;

ConsoleHelper chelper(ansi::Gradient {
    ansi::rgb_to_ansi(19, 195, 235),
    ansi::rgb_to_ansi(19, 170, 235)
}, "");

static std::string g_current_plugin_name = "";

void PluginEnvironment::SetCurrentPluginName(const std::string& name)
{
    g_current_plugin_name = name;
}

std::string MoonhookPlugin::get_bytecode()
{
    if (type == 1)
        return content;

    if (!bc.empty())
        return bc;

    Luau::CompileOptions options;
    options.optimizationLevel = 1;
    options.debugLevel = 1;

    std::string bytecode = Luau::compile(strip_plugin_header(), options);

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

std::optional<MoonhookPlugin::PluginHeader> MoonhookPlugin::parse_plugin_header()
{
    const std::string start_tag = "--!plugin";
    const std::string end_tag   = "--!end";

    std::size_t start_pos = content.find(start_tag);
    if (start_pos == std::string::npos) return std::nullopt;

    std::size_t block_start = start_pos + start_tag.size();
    std::size_t end_pos     = content.find(end_tag);
    if (end_pos == std::string::npos) return std::nullopt;

    std::string block = content.substr(block_start, end_pos - block_start);

    std::unordered_map<std::string, std::string> fields;
    std::istringstream stream(block);
    std::string line;

    while (std::getline(stream, line))
    {
        std::size_t colon = line.find(':');
        if (colon == std::string::npos) continue; // was = before, bug fixed

        std::string key   = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        auto trim = [](std::string& s) {
            size_t start = s.find_first_not_of(" \t\r\n");
            size_t end   = s.find_last_not_of(" \t\r\n");
            s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
        };

        trim(key);
        trim(value);

        if (!key.empty() && !value.empty())
            fields[key] = value;
    }

    PluginHeader header;
    if (fields.count("Name"))        header.name        = fields["Name"];
    if (fields.count("Description")) header.description = fields["Description"];
    if (fields.count("Author"))      header.author      = fields["Author"];
    if (fields.count("Version"))     header.version     = fields["Version"];

    return header;
}

std::string MoonhookPlugin::strip_plugin_header()
{
    const std::string startTag = "--!plugin";
    const std::string endTag   = "--!end";

    size_t startPos = content.find(startTag);
    if (startPos == std::string::npos) return content;

    size_t endPos = content.find(endTag, startPos);
    if (endPos == std::string::npos) return content;

    size_t stripEnd = endPos + endTag.size();

    if (stripEnd < content.size() && content[stripEnd] == '\n')
        stripEnd++;

    return content.substr(0, startPos) + content.substr(stripEnd);
}

namespace option_index
{
    constexpr int MAIN_MENU_OPTION = 0;
    constexpr int WEBHOOKS_OPTION  = 1;
    constexpr int BOTS_OPTION      = 2;
}

static moon::Webhook* check_webhook(lua_State* L, int idx) {
    return (moon::Webhook*)luaL_checkudata(L, idx, "WebhookMeta");
}

static void webhook_dtor(void* ud) {
    ((moon::Webhook*)ud)->~Webhook();
}

static int l_webhook_new(lua_State* L) {
    const char* url = luaL_checkstring(L, 1);
    void* ud = lua_newuserdatadtor(L, sizeof(moon::Webhook), webhook_dtor);
    new(ud) moon::Webhook(url);
    luaL_getmetatable(L, "WebhookMeta");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_webhook_send(lua_State* L) {
    moon::Webhook* wh = check_webhook(L, 1);
    if (lua_isstring(L, 2)) {
        bool res = wh->send(lua_tostring(L, 2));
        lua_pushboolean(L, res);
        return 1;
    }
    return 0;
}

static int l_webhook_set_proxy(lua_State* L) {
    moon::Webhook* wh = check_webhook(L, 1);
    wh->set_proxy(luaL_checkstring(L, 2));
    return 0;
}

static int l_webhook_get_proxy(lua_State* L) {
    moon::Webhook* wh = check_webhook(L, 1);
    lua_pushstring(L, wh->get_proxy().c_str());
    return 1;
}

static int l_webhook_last_error(lua_State* L) {
    moon::Webhook* wh = check_webhook(L, 1);
    lua_pushstring(L, wh->last_error().c_str());
    return 1;
}

static int l_webhook_retry_after_ms(lua_State* L) {
    moon::Webhook* wh = check_webhook(L, 1);
    lua_pushinteger(L, wh->retry_after_ms());
    return 1;
}

static int l_webhook_set_name(lua_State* L) {
    moon::Webhook* wh = check_webhook(L, 1);
    bool res = wh->set_name(luaL_checkstring(L, 2));
    lua_pushboolean(L, res);
    return 1;
}

static int l_webhook_set_avatar_from_url(lua_State* L) {
    moon::Webhook* wh = check_webhook(L, 1);
    bool res = wh->set_avatar_from_url(luaL_checkstring(L, 2));
    lua_pushboolean(L, res);
    return 1;
}

static int l_webhook_set_avatar_from_file(lua_State* L) {
    moon::Webhook* wh = check_webhook(L, 1);
    bool res = wh->set_avatar_from_file(luaL_checkstring(L, 2));
    lua_pushboolean(L, res);
    return 1;
}

static int l_webhook_get_name(lua_State* L) {
    moon::Webhook* wh = check_webhook(L, 1);
    lua_pushstring(L, wh->get_name().c_str());
    return 1;
}

static int l_webhook_delete_webhook(lua_State* L) {
    moon::Webhook* wh = check_webhook(L, 1);
    bool res = wh->delete_webhook();
    lua_pushboolean(L, res);
    return 1;
}

static const luaL_Reg webhook_methods[] = {
    {"send",                l_webhook_send},
    {"set_proxy",           l_webhook_set_proxy},
    {"get_proxy",           l_webhook_get_proxy},
    {"last_error",          l_webhook_last_error},
    {"retry_after_ms",      l_webhook_retry_after_ms},
    {"set_name",            l_webhook_set_name},
    {"set_avatar_from_url", l_webhook_set_avatar_from_url},
    {"set_avatar_from_file",l_webhook_set_avatar_from_file},
    {"get_name",            l_webhook_get_name},
    {"delete_webhook",      l_webhook_delete_webhook},
    {nullptr, nullptr}
};

static moon::DiscordBot* check_bot(lua_State* L, int idx) {
    return (moon::DiscordBot*)luaL_checkudata(L, idx, "BotMeta");
}

static void bot_dtor(void* ud) {
    ((moon::DiscordBot*)ud)->~DiscordBot();
}

static int l_bot_new(lua_State* L) {
    const char* token = luaL_checkstring(L, 1);
    void* ud = lua_newuserdatadtor(L, sizeof(moon::DiscordBot), bot_dtor);
    new(ud) moon::DiscordBot(token);
    luaL_getmetatable(L, "BotMeta");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_bot_validate_guild(lua_State* L) {
    moon::DiscordBot* bot = check_bot(L, 1);
    std::string res = bot->validate_guild(luaL_checkstring(L, 2));
    lua_pushstring(L, res.c_str());
    return 1;
}

static int l_bot_get_channels(lua_State* L) {
    moon::DiscordBot* bot = check_bot(L, 1);
    auto channels = bot->get_channels(luaL_checkstring(L, 2));
    lua_newtable(L);
    int i = 1;
    for (const auto& c : channels) {
        lua_pushinteger(L, i++);
        lua_newtable(L);
        lua_pushstring(L, c.id.c_str());   lua_setfield(L, -2, "id");
        lua_pushstring(L, c.name.c_str()); lua_setfield(L, -2, "name");
        lua_pushinteger(L, c.type);        lua_setfield(L, -2, "type");
        lua_settable(L, -3);
    }
    return 1;
}

static int l_bot_delete_channel(lua_State* L) {
    moon::DiscordBot* bot = check_bot(L, 1);
    bool res = bot->delete_channel(luaL_checkstring(L, 2));
    lua_pushboolean(L, res);
    return 1;
}

static int l_bot_create_text_channel(lua_State* L) {
    moon::DiscordBot* bot = check_bot(L, 1);
    bool res = bot->create_text_channel(luaL_checkstring(L, 2), luaL_checkstring(L, 3));
    lua_pushboolean(L, res);
    return 1;
}

static int l_bot_get_roles(lua_State* L) {
    moon::DiscordBot* bot = check_bot(L, 1);
    auto roles = bot->get_roles(luaL_checkstring(L, 2));
    lua_newtable(L);
    int i = 1;
    for (const auto& r : roles) {
        lua_pushinteger(L, i++);
        lua_newtable(L);
        lua_pushstring(L, r.id.c_str());   lua_setfield(L, -2, "id");
        lua_pushstring(L, r.name.c_str()); lua_setfield(L, -2, "name");
        lua_settable(L, -3);
    }
    return 1;
}

static int l_bot_delete_role(lua_State* L) {
    moon::DiscordBot* bot = check_bot(L, 1);
    bool res = bot->delete_role(luaL_checkstring(L, 2), luaL_checkstring(L, 3));
    lua_pushboolean(L, res);
    return 1;
}

static int l_bot_create_role(lua_State* L) {
    moon::DiscordBot* bot = check_bot(L, 1);
    bool res = bot->create_role(luaL_checkstring(L, 2), luaL_checkstring(L, 3));
    lua_pushboolean(L, res);
    return 1;
}

static int l_bot_get_members(lua_State* L) {
    moon::DiscordBot* bot = check_bot(L, 1);
    auto members = bot->get_members(luaL_checkstring(L, 2), luaL_checkstring(L, 3));
    lua_newtable(L);
    int i = 1;
    for (const auto& m : members) {
        lua_pushinteger(L, i++);
        lua_newtable(L);
        lua_pushstring(L, m.user_id.c_str());       lua_setfield(L, -2, "user_id");
        lua_pushstring(L, m.username.c_str());       lua_setfield(L, -2, "username");
        lua_pushstring(L, m.discriminator.c_str());  lua_setfield(L, -2, "discriminator");
        lua_pushboolean(L, m.is_owner);              lua_setfield(L, -2, "is_owner");
        lua_settable(L, -3);
    }
    return 1;
}

static int l_bot_ban_member(lua_State* L) {
    moon::DiscordBot* bot = check_bot(L, 1);
    bool res = bot->ban_member(luaL_checkstring(L, 2), luaL_checkstring(L, 3), luaL_checkstring(L, 4));
    lua_pushboolean(L, res);
    return 1;
}

static int l_bot_create_webhook(lua_State* L) {
    moon::DiscordBot* bot = check_bot(L, 1);
    auto wh = bot->create_webhook(luaL_checkstring(L, 2), luaL_checkstring(L, 3));
    lua_newtable(L);
    lua_pushstring(L, wh.id.c_str());    lua_setfield(L, -2, "id");
    lua_pushstring(L, wh.token.c_str()); lua_setfield(L, -2, "token");
    lua_pushstring(L, wh.url().c_str()); lua_setfield(L, -2, "url");
    return 1;
}

static int l_bot_last_error(lua_State* L) {
    moon::DiscordBot* bot = check_bot(L, 1);
    lua_pushstring(L, bot->last_error().c_str());
    return 1;
}

static const luaL_Reg bot_methods[] = {
    {"validate_guild",      l_bot_validate_guild},
    {"get_channels",        l_bot_get_channels},
    {"delete_channel",      l_bot_delete_channel},
    {"create_text_channel", l_bot_create_text_channel},
    {"get_roles",           l_bot_get_roles},
    {"delete_role",         l_bot_delete_role},
    {"create_role",         l_bot_create_role},
    {"get_members",         l_bot_get_members},
    {"ban_member",          l_bot_ban_member},
    {"create_webhook",      l_bot_create_webhook},
    {"last_error",          l_bot_last_error},
    {nullptr, nullptr}
};

static int l_moonhook_option(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    int type         = luaL_checkinteger(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    int ref = lua_ref(L, 3);

    Option opt(name, type, [L, ref, type](ConsoleHelper* console)
    {
        lua_getref(L, ref);
        int nargs = 0;

        if (type == option_index::WEBHOOKS_OPTION) {
            lua_newtable(L);
            lua_pushcfunction(L, l_webhook_new);
            lua_setfield(L, -2, "new");
            lua_pushstring(L, OptionContext::webhook_url.c_str());
            lua_setfield(L, -2, "url");
            nargs = 1;
        } else if (type == option_index::BOTS_OPTION) {
            lua_newtable(L);
            lua_pushcfunction(L, l_bot_new);
            lua_setfield(L, -2, "new");
            lua_pushstring(L, OptionContext::bot_token.c_str());
            lua_setfield(L, -2, "token");
            lua_pushstring(L, OptionContext::bot_guild.c_str());
            lua_setfield(L, -2, "guild_id");
            nargs = 1;
        }

        if (lua_pcall(L, nargs, 0, 0) != LUA_OK)
        {
            console->error("Option failed! Lua error: " + std::string(lua_tostring(L, -1)));
            lua_pop(L, 1);
        }
    });

    opt.plugin_name      = g_current_plugin_name;
    opt.lua_callback_ref = ref;
    Registry::Get().AddOption(std::move(opt));
    return 0;
}

static json lua_to_json(lua_State* L, int index)
{
    json j = json::object();

    luaL_checktype(L, index, LUA_TTABLE);
    lua_pushnil(L);

    bool is_array = true;

    while (lua_next(L, index))
    {
        if (lua_type(L, -2) == LUA_TSTRING)
        {
            if (is_array && !j.empty())
            {
                json new_j = json::object();
                int idx = 0;
                for (auto& item : j)
                    new_j[std::to_string(idx++)] = item;
                j = std::move(new_j);
            }
            is_array = false;
            std::string key = lua_tostring(L, -2);

            if (lua_isstring(L, -1))       j[key] = lua_tostring(L, -1);
            else if (lua_isnumber(L, -1))  j[key] = lua_tonumber(L, -1);
            else if (lua_isboolean(L, -1)) j[key] = (bool)lua_toboolean(L, -1);
            else if (lua_istable(L, -1))   j[key] = lua_to_json(L, lua_gettop(L));
        }
        else if (lua_type(L, -2) == LUA_TNUMBER)
        {
            if (is_array)
            {
                int idx = (int)lua_tonumber(L, -2) - 1;
                if (idx < 0) idx = 0;

                if (lua_isstring(L, -1))       j[idx] = lua_tostring(L, -1);
                else if (lua_isnumber(L, -1))  j[idx] = lua_tonumber(L, -1);
                else if (lua_isboolean(L, -1)) j[idx] = (bool)lua_toboolean(L, -1);
                else if (lua_istable(L, -1))   j[idx] = lua_to_json(L, lua_gettop(L));
            }
            else
            {
                std::string key = std::to_string((int)lua_tonumber(L, -2));

                if (lua_isstring(L, -1))       j[key] = lua_tostring(L, -1);
                else if (lua_isnumber(L, -1))  j[key] = lua_tonumber(L, -1);
                else if (lua_isboolean(L, -1)) j[key] = (bool)lua_toboolean(L, -1);
                else if (lua_istable(L, -1))   j[key] = lua_to_json(L, lua_gettop(L));
            }
        }

        lua_pop(L, 1);
    }

    return j;
}

static int l_json_encode(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    json j = lua_to_json(L, 1);
    std::string out = j.dump();
    lua_pushlstring(L, out.c_str(), out.size());
    return 1;
}

static void json_to_lua(lua_State* L, const json& j)
{
    if (j.is_object())
    {
        lua_newtable(L);
        for (auto& [key, value] : j.items())
        {
            lua_pushstring(L, key.c_str());

            if (value.is_string())                   lua_pushstring(L, value.get<std::string>().c_str());
            else if (value.is_number())               lua_pushnumber(L, value.get<double>());
            else if (value.is_boolean())              lua_pushboolean(L, value.get<bool>());
            else if (value.is_object() || value.is_array()) json_to_lua(L, value);
            else                                      lua_pushnil(L);

            lua_settable(L, -3);
        }
    }
    else if (j.is_array())
    {
        lua_newtable(L);
        int i = 1;
        for (auto& v : j)
        {
            lua_pushinteger(L, i++);

            if (v.is_string())                   lua_pushstring(L, v.get<std::string>().c_str());
            else if (v.is_number())               lua_pushnumber(L, v.get<double>());
            else if (v.is_boolean())              lua_pushboolean(L, v.get<bool>());
            else if (v.is_object() || v.is_array()) json_to_lua(L, v);
            else                                  lua_pushnil(L);

            lua_settable(L, -3);
        }
    }
    else
    {
        lua_pushnil(L);
    }
}

static int l_json_decode(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);
    json j;
    try {
        j = json::parse(str);
    } catch (...) {
        lua_pushnil(L);
        return 1;
    }
    json_to_lua(L, j);
    return 1;
}

static size_t curl_write_cb(void* contents, size_t size, size_t nmemb, std::string* out)
{
    size_t total = size * nmemb;
    out->append((char*)contents, total);
    return total;
}

static size_t curl_header_cb(char* buffer, size_t size, size_t nitems, std::map<std::string, std::string>* headers)
{
    size_t total = size * nitems;
    std::string line(buffer, total);

    while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
        line.pop_back();

    auto colon = line.find(':');
    if (colon != std::string::npos)
    {
        std::string key   = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        if (!value.empty() && value.front() == ' ')
            value.erase(value.begin());
        (*headers)[key] = value;
    }

    return total;
}

static int l_moonhook_request(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_rawgetfield(L, 1, "Url");
    if (!lua_isstring(L, -1))
        return luaL_error(L, "Request: 'Url' must be a string");
    std::string url = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_rawgetfield(L, 1, "Method");
    std::string method = lua_isstring(L, -1) ? lua_tostring(L, -1) : "GET";
    lua_pop(L, 1);

    struct curl_slist* req_headers = nullptr;
    lua_rawgetfield(L, 1, "Headers");
    if (lua_istable(L, -1))
    {
        int headers_idx = lua_gettop(L);
        lua_pushnil(L);
        while (lua_next(L, headers_idx))
        {
            if (lua_isstring(L, -2) && lua_isstring(L, -1))
            {
                std::string h = std::string(lua_tostring(L, -2)) + ": " + lua_tostring(L, -1);
                req_headers = curl_slist_append(req_headers, h.c_str());
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    std::string body;
    lua_rawgetfield(L, 1, "Body");
    if (lua_isstring(L, -1))
        body = lua_tostring(L, -1);
    lua_pop(L, 1);

    CURL* curl = curl_easy_init();
    if (!curl)
    {
        if (req_headers) curl_slist_free_all(req_headers);
        return luaL_error(L, "Request: failed to initialize curl");
    }

    std::string response_body;
    std::map<std::string, std::string> response_headers;
    long status_code = 0;

    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &response_body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_header_cb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA,     &response_headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        30L);

    if (req_headers)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, req_headers);

    if (method == "POST")
    {
        curl_easy_setopt(curl, CURLOPT_POST,          1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
    }
    else if (method == "PUT" || method == "PATCH" || method == "DELETE")
    {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
        if (!body.empty())
        {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
        }
    }

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

    const char* status_msg = "";
    switch (status_code)
    {
        case 200: status_msg = "OK";                    break;
        case 201: status_msg = "Created";               break;
        case 204: status_msg = "No Content";            break;
        case 400: status_msg = "Bad Request";           break;
        case 401: status_msg = "Unauthorized";          break;
        case 403: status_msg = "Forbidden";             break;
        case 404: status_msg = "Not Found";             break;
        case 405: status_msg = "Method Not Allowed";    break;
        case 429: status_msg = "Too Many Requests";     break;
        case 500: status_msg = "Internal Server Error"; break;
        case 502: status_msg = "Bad Gateway";           break;
        case 503: status_msg = "Service Unavailable";   break;
        default:  status_msg = "Unknown";               break;
    }

    curl_easy_cleanup(curl);
    if (req_headers) curl_slist_free_all(req_headers);

    lua_newtable(L);

    bool success = (res == CURLE_OK && status_code >= 200 && status_code < 300);
    lua_pushboolean(L, success);
    lua_setfield(L, -2, "Success");

    lua_pushinteger(L, (int)status_code);
    lua_setfield(L, -2, "StatusCode");

    lua_pushstring(L, status_msg);
    lua_setfield(L, -2, "StatusMessage");

    lua_newtable(L);
    for (auto& [k, v] : response_headers)
    {
        lua_pushstring(L, v.c_str());
        lua_setfield(L, -2, k.c_str());
    }
    lua_setfield(L, -2, "Headers");

    lua_pushlstring(L, response_body.c_str(), response_body.size());
    lua_setfield(L, -2, "Body");

    return 1;
}

static int l_console_log(lua_State* L)
{
    const char* text = luaL_checkstring(L, 1);
    chelper.log(text);
    return 0;
}

static int l_console_error(lua_State* L)
{
    const char* text = luaL_checkstring(L, 1);
    chelper.error(text);
    return 0;
}

static int l_console_input(lua_State* L)
{
    std::string text = luaL_checkstring(L, 1);
    std::string resp = chelper.input(text);
    lua_pushstring(L, resp.c_str());
    return 1;
}

static int l_console_int_input(lua_State* L)
{
    std::string text = luaL_checkstring(L, 1);
    int resp = chelper.int_input(text);
    lua_pushinteger(L, resp);
    return 1;
}

static int l_console_printbanner(lua_State* L)
{
    chelper.printbanner();
    return 0;
}

static int l_console_setcolors(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TTABLE);

    lua_rawgeti(L, 1, 1); int s_r = luaL_checkinteger(L, -1); lua_pop(L, 1);
    lua_rawgeti(L, 1, 2); int s_g = luaL_checkinteger(L, -1); lua_pop(L, 1);
    lua_rawgeti(L, 1, 3); int s_b = luaL_checkinteger(L, -1); lua_pop(L, 1);

    lua_rawgeti(L, 2, 1); int e_r = luaL_checkinteger(L, -1); lua_pop(L, 1);
    lua_rawgeti(L, 2, 2); int e_g = luaL_checkinteger(L, -1); lua_pop(L, 1);
    lua_rawgeti(L, 2, 3); int e_b = luaL_checkinteger(L, -1); lua_pop(L, 1);

    ansi::Gradient grad = {
        ansi::rgb_to_ansi(s_r, s_g, s_b),
        ansi::rgb_to_ansi(e_r, e_g, e_b)
    };
    chelper.set_gradient(grad);
    return 0;
}

static int l_console_setbanner(lua_State* L)
{
    const char* banner = luaL_checkstring(L, 1);
    chelper.set_banner(banner);
    return 0;
}

static int l_console_wait(lua_State* L)
{
    int ms = luaL_checkinteger(L, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return 0;
}

static int l_console_pause(lua_State* L)
{
    ansi::pause();
    return 0;
}

static int l_filesystem_readfile(lua_State* L)
{
    const std::string& path = luaL_checkstring(L, 1);
    std::string output = SafeFilesystem::read(path);
    if (output == "ERROR")
    {
        luaL_error(L, ("Failed to open file! Error: " + SafeFilesystem::last_error).c_str());
        return 0;
    }
    lua_pushstring(L, output.c_str());
    return 1;
}

static int l_filesystem_writefile(lua_State* L)
{
    const std::string& path = luaL_checkstring(L, 1);
    const std::string& content = luaL_checkstring(L, 2);
    bool success = SafeFilesystem::write(path, content);
    if (!success)
    {
        luaL_error(L, ("Failed to write to file! Error: " + SafeFilesystem::last_error).c_str());
        return 0;
    }
    return 0;
}

static int l_filesystem_appendfile(lua_State* L)
{
    const std::string& path = luaL_checkstring(L, 1);
    const std::string& content = luaL_checkstring(L, 2);
    bool success = SafeFilesystem::append(path, content);
    if (!success)
    {
        luaL_error(L, ("Failed to append to file! Error: " + SafeFilesystem::last_error).c_str());
        return 0;
    }
    return 0;
}

static int l_filesystem_makefolder(lua_State* L)
{
    const std::string& path = luaL_checkstring(L, 1);
    bool success = SafeFilesystem::make_directories(path);
    if (!success)
    {
        luaL_error(L, ("Failed to create folder! Error: " + SafeFilesystem::last_error).c_str());
        return 0;
    }
    return 0;
}

static int l_filesystem_delfolder(lua_State* L)
{
    const std::string& path = luaL_checkstring(L, 1);
    bool success = SafeFilesystem::delete_directory(path);
    if (!success)
    {
        luaL_error(L, ("Failed to delete folder! Error: " + SafeFilesystem::last_error).c_str());
        return 0;
    }
    return 0;
}

const luaL_Reg PluginEnvironment::FilesystemLibrary[] = {
    {"readfile", l_filesystem_readfile},
    {"writefile", l_filesystem_writefile},
    {"appendfile", l_filesystem_appendfile},
    {"makefolder", l_filesystem_makefolder},
    {"delfolder", l_filesystem_delfolder},
    {nullptr, nullptr}
};

const luaL_Reg PluginEnvironment::MoonhookLibrary[] = {
    {"Option",  l_moonhook_option},
    {"Request", l_moonhook_request},
    {nullptr, nullptr}
};

const luaL_Reg PluginEnvironment::ConsoleLibrary[] = {
    {"log",         l_console_log},
    {"error",       l_console_error},
    {"input",       l_console_input},
    {"int_input",   l_console_int_input},
    {"printbanner", l_console_printbanner},
    {"setcolors",   l_console_setcolors},
    {"setbanner",   l_console_setbanner},
    {"wait",        l_console_wait},
    {"pause",       l_console_pause},
    {nullptr, nullptr}
};

void PluginEnvironment::install(lua_State* L)
{
    lua_newtable(L);

    luaL_newmetatable(L, "WebhookMeta");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    for (const luaL_Reg* reg = webhook_methods; reg->name != nullptr; reg++) {
        lua_pushcfunction(L, reg->func);
        lua_setfield(L, -2, reg->name);
    }
    lua_pop(L, 1);

    luaL_newmetatable(L, "BotMeta");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    for (const luaL_Reg* reg = bot_methods; reg->name != nullptr; reg++) {
        lua_pushcfunction(L, reg->func);
        lua_setfield(L, -2, reg->name);
    }
    lua_pop(L, 1);

    for (const luaL_Reg* reg = MoonhookLibrary; reg->name != nullptr; reg++)
    {
        lua_pushcfunction(L, reg->func);
        lua_setfield(L, -2, reg->name);
    }

    for (const luaL_Reg* reg = ConsoleLibrary; reg->name != nullptr; reg++)
    {
        lua_pushcfunction(L, reg->func);
        lua_setfield(L, -2, reg->name);
    }

    for (const luaL_Reg* reg = FilesystemLibrary; reg-> name != nullptr; reg++)
    {
        lua_pushcfunction(L, reg->func);
        lua_setfield(L, -2, reg->name);
    }

    lua_pushcfunction(L, l_json_encode);
    lua_setfield(L, -2, "encode");

    lua_pushcfunction(L, l_json_decode);
    lua_setfield(L, -2, "decode");

    lua_pushinteger(L, option_index::MAIN_MENU_OPTION);
    lua_setfield(L, -2, "MAIN_MENU_OPTION");

    lua_pushinteger(L, option_index::BOTS_OPTION);
    lua_setfield(L, -2, "BOTS_OPTION");

    lua_pushinteger(L, option_index::WEBHOOKS_OPTION);
    lua_setfield(L, -2, "WEBHOOKS_OPTION");

    lua_setglobal(L, "moonhook");
}
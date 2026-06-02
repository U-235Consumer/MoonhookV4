#include "bot.hpp"
#include <curl/curl.h>
#include <iostream>
#include <sstream>

namespace moon {

static size_t write_cb(char* data, size_t size, size_t nmemb, void* clientdata) {
    reinterpret_cast<std::string*>(clientdata)->append(data, size * nmemb);
    return size * nmemb;
}

DiscordBot::DiscordBot(const std::string& token) : m_token(token) {}

const std::string& DiscordBot::last_error() const {
    return m_last_error;
}

nlohmann::json DiscordBot::api_request(const std::string& method,
                                        const std::string& endpoint,
                                        const std::string& body,
                                        long* out_code) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        m_last_error = "Failed to init cURL";
        return nullptr;
    }

    std::string url = "https://discord.com/api/v10" + endpoint;
    std::string auth = "Authorization: Bot " + m_token;
    std::string response_body;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, auth.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "User-Agent: Roblox/WinInet");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
    } else if (method == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else if (method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        if (!body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
        }
    }

    CURLcode res = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (out_code) *out_code = code;

    if (res != CURLE_OK) {
        m_last_error = std::string("cURL error: ") + curl_easy_strerror(res);
        return nullptr;
    }

    if (response_body.empty()) return nlohmann::json::object();

    try {
        return nlohmann::json::parse(response_body);
    } catch (...) {
        return nlohmann::json::object();
    }
}

std::string DiscordBot::validate_guild(const std::string& guild_id) {
    long code = 0;
    auto j = api_request("GET", "/guilds/" + guild_id, "", &code);
    if (code == 200 && j.contains("name")) {
        return j["name"].get<std::string>();
    }
    m_last_error = "Guild not found or missing permissions (HTTP " + std::to_string(code) + ")";
    return "";
}

std::vector<DiscordBot::Channel> DiscordBot::get_channels(const std::string& guild_id) {
    std::vector<Channel> result;
    long code = 0;
    auto j = api_request("GET", "/guilds/" + guild_id + "/channels", "", &code);
    if (!j.is_array()) return result;
    for (auto& ch : j) {
        Channel c;
        c.id   = ch.value("id", "");
        c.name = ch.value("name", "");
        c.type = ch.value("type", 0);
        if (!c.id.empty()) result.push_back(c);
    }
    return result;
}

bool DiscordBot::delete_channel(const std::string& channel_id) {
    long code = 0;
    api_request("DELETE", "/channels/" + channel_id, "", &code);
    return (code == 200 || code == 204);
}

bool DiscordBot::create_text_channel(const std::string& guild_id, const std::string& name) {
    nlohmann::json body;
    body["name"] = name;
    body["type"] = 0;
    long code = 0;
    api_request("POST", "/guilds/" + guild_id + "/channels", body.dump(), &code);
    return (code == 201 || code == 200);
}

std::vector<DiscordBot::Role> DiscordBot::get_roles(const std::string& guild_id) {
    std::vector<Role> result;
    long code = 0;
    auto j = api_request("GET", "/guilds/" + guild_id + "/roles", "", &code);
    if (!j.is_array()) return result;
    for (auto& r : j) {
        Role role;
        role.id   = r.value("id", "");
        role.name = r.value("name", "");
        if (!role.id.empty()) result.push_back(role);
    }
    return result;
}

bool DiscordBot::delete_role(const std::string& guild_id, const std::string& role_id) {
    long code = 0;
    api_request("DELETE", "/guilds/" + guild_id + "/roles/" + role_id, "", &code);
    return (code == 204 || code == 200);
}

bool DiscordBot::create_role(const std::string &guild_id, const std::string &role_name)
{
    nlohmann::json body;
    body["name"] = role_name;
    long code = 0;
    api_request("POST", "/guilds/" + guild_id + "/roles", body.dump(), &code);
    if (code == 200 || code == 201) return true;
    m_last_error = "Failed to create role (HTTP " + std::to_string(code) + ")";
    return false;
}

std::vector<DiscordBot::Member> DiscordBot::get_members(const std::string& guild_id, const std::string& owner_id) {
    std::vector<Member> result;
    std::string after = "";
    while (true) {
        std::string ep = "/guilds/" + guild_id + "/members?limit=1000";
        if (!after.empty()) ep += "&after=" + after;
        long code = 0;
        auto j = api_request("GET", ep, "", &code);
        if (!j.is_array() || j.empty()) break;
        for (auto& m : j) {
            if (!m.contains("user")) continue;
            Member mem;
            mem.user_id       = m["user"].value("id", "");
            mem.username      = m["user"].value("username", "");
            mem.discriminator = m["user"].value("discriminator", "0");
            mem.is_owner      = (mem.user_id == owner_id);
            if (!mem.user_id.empty()) result.push_back(mem);
            after = mem.user_id;
        }
        if ((int)j.size() < 1000) break;
    }
    return result;
}

bool DiscordBot::ban_member(const std::string& guild_id, const std::string& user_id, const std::string& reason) {
    nlohmann::json body;
    if (!reason.empty()) body["delete_message_seconds"] = 0;
    std::string ep = "/guilds/" + guild_id + "/bans/" + user_id;

    long code = 0;
    api_request("PUT", ep, body.dump(), &code);
    return (code == 204 || code == 200);
}

DiscordBot::WebhookInfo DiscordBot::create_webhook(const std::string& channel_id, const std::string& name) {
    nlohmann::json body;
    body["name"] = name;
    long code = 0;
    auto j = api_request("POST", "/channels/" + channel_id + "/webhooks", body.dump(), &code);
    WebhookInfo wh;
    if (code == 200 || code == 201) {
        wh.id    = j.value("id", "");
        wh.token = j.value("token", "");
    } else {
        m_last_error = "Failed to create webhook (HTTP " + std::to_string(code) + ")";
    }
    return wh;
}

} // namespace moon
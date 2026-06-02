#pragma once
#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>

namespace moon {

class DiscordBot {
public:
    struct Channel {
        std::string id;
        std::string name;
        int type = 0; // 0 = text, 2 = voice, 4 = category
    };

    struct Role {
        std::string id;
        std::string name;
    };

    struct Member {
        std::string user_id;
        std::string username;
        std::string discriminator;
        bool is_owner = false;
    };

    struct WebhookInfo {
        std::string id;
        std::string token;
        std::string url() const { return "https://discord.com/api/webhooks/" + id + "/" + token; }
    };

    explicit DiscordBot(const std::string& token);

    std::string validate_guild(const std::string& guild_id);

    std::vector<Channel> get_channels(const std::string& guild_id);
    bool delete_channel(const std::string& channel_id);
    bool create_text_channel(const std::string& guild_id, const std::string& name);

    std::vector<Role> get_roles(const std::string& guild_id);
    bool delete_role(const std::string& guild_id, const std::string& role_id);
    bool create_role(const std::string& guild_id, const std::string& role_name);

    std::vector<Member> get_members(const std::string& guild_id, const std::string& owner_id);
    bool ban_member(const std::string& guild_id, const std::string& user_id, const std::string& reason);

    WebhookInfo create_webhook(const std::string& channel_id, const std::string& name);

    const std::string& last_error() const;

private:
    std::string m_token;
    std::string m_last_error;

    nlohmann::json api_request(const std::string& method,
                               const std::string& endpoint,
                               const std::string& body = "",
                               long* out_code = nullptr);
};

} // namespace moon
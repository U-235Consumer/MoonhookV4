#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace moon {

class Webhook {
public:
    struct Embed {
        std::string title;
        std::string description;
        int color = 0x000000;
        std::string footer;
        std::string thumbnail;
        std::string image;
        
        struct Field {
            std::string name;
            std::string value;
            bool is_inline = false;
        };
        std::vector<Field> fields;

        nlohmann::json to_json() const;
    };

    explicit Webhook(const std::string& url);

    bool send(const std::string& content);
    bool send(const Embed& embed);
    bool send(const std::string& content, const std::vector<Embed>& embeds);

    void set_proxy(const std::string& proxy);
    const std::string& get_proxy() const;
    const std::string& last_error() const;
    int retry_after_ms() const;

    bool set_name(const std::string& name);
    bool set_avatar_from_url(const std::string& image_url);
    bool set_avatar_from_file(const std::string& file_path);

    std::string get_name();

    bool delete_webhook();

private:
    std::string m_url;
    std::string m_proxy;
    std::string m_last_error;
    int m_retry_after_ms = 0;
    bool patch_avatar(const std::string& base64_data, const std::string& mime_type);
    bool fetch_url_to_string(const std::string& url, std::string& out);
    std::string to_base64(const std::string& data);
};

} // namespace moon
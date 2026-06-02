#include "webhook.hpp"
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <algorithm>

static const std::string B64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
namespace moon {

static std::string trim(std::string str) {
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), str.end());
    return str;
}

class CurlGlobal {
public:
    CurlGlobal() {
        curl_global_init(CURL_GLOBAL_ALL);
    }
    ~CurlGlobal() {
        curl_global_cleanup();
    }
};

static CurlGlobal g_curl_global;

nlohmann::json Webhook::Embed::to_json() const {
    nlohmann::json j;
    if (!title.empty()) j["title"] = title;
    if (!description.empty()) j["description"] = description;
    j["color"] = color;
    if (!footer.empty()) j["footer"]["text"] = footer;
    if (!thumbnail.empty()) j["thumbnail"]["url"] = thumbnail;
    if (!image.empty()) j["image"]["url"] = image;

    if (!fields.empty()) {
        for (const auto& field : fields) {
            j["fields"].push_back({
                {"name", field.name},
                {"value", field.value},
                {"inline", field.is_inline}
            });
        }
    }
    return j;
}

Webhook::Webhook(const std::string& url) : m_url(trim(url)) {}

void Webhook::set_proxy(const std::string& proxy) {
    m_proxy = trim(proxy);
}

const std::string& Webhook::get_proxy() const {
    return m_proxy;
}

const std::string& Webhook::last_error() const {
    return m_last_error;
}

int Webhook::retry_after_ms() const {
    return m_retry_after_ms;
}

bool Webhook::send(const std::string& content) {
    return send(content, {});
}

bool Webhook::send(const Embed& embed) {
    return send("", {embed});
}

bool Webhook::send(const std::string& content, const std::vector<Embed>& embeds) {
    nlohmann::json payload;
    if (!content.empty()) payload["content"] = content;
    
    for (const auto& embed : embeds) {
        payload["embeds"].push_back(embed.to_json());
    }

    std::string payload_str = payload.dump();

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize cURL" << std::endl;
        return false;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    std::string clean_url = trim(m_url);

    curl_easy_setopt(curl, CURLOPT_URL, clean_url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)payload_str.size());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    if (!m_proxy.empty()) {
        std::string clean_proxy = trim(m_proxy);
        curl_easy_setopt(curl, CURLOPT_PROXY, clean_proxy.c_str());
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
    }

    std::string response_body;
    auto write_callback = [](char* data, size_t size, size_t nmemb, void* clientdata) -> size_t {
        reinterpret_cast<std::string*>(clientdata)->append(data, size * nmemb);
        return size * nmemb;
    };
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

    CURLcode res = curl_easy_perform(curl);
    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    bool success = false;
    m_retry_after_ms = 0;
    if (res != CURLE_OK) {
        m_last_error = std::string("cURL error: ") + curl_easy_strerror(res);
    } else {
        if (response_code == 204 || response_code == 200) {
            m_last_error.clear();
            success = true;
        } else if (response_code == 429) {
            double retry_after_secs = 1.0;
            try {
                auto j = nlohmann::json::parse(response_body);
                if (j.contains("retry_after")) {
                    retry_after_secs = j["retry_after"].get<double>();
                }
            } catch (...) {}
            m_retry_after_ms = static_cast<int>(retry_after_secs * 1000.0) + 50;
            m_last_error = "Rate limited (429) — retry after " + std::to_string(m_retry_after_ms) + "ms";
        } else {
            m_last_error = "HTTP " + std::to_string(response_code) + ": " + response_body;
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return success;
}

bool Webhook::set_name(const std::string& name)
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        m_last_error = "Failed to init cURL";
        return false;
    }

    nlohmann::json payload;
    payload["name"] = name;

    std::string payload_str = payload.dump();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    std::string url = trim(m_url);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)payload_str.size());

    std::string response;

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
        +[](char* data, size_t size, size_t nmemb, void* userdata) -> size_t {
            reinterpret_cast<std::string*>(userdata)->append(data, size * nmemb);
            return size * nmemb;
        });

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);

    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        m_last_error = curl_easy_strerror(res);
        return false;
    }

    if (code != 200)
    {
        m_last_error = "HTTP " + std::to_string(code) + ": " + response;
        return false;
    }

    m_last_error.clear();
    return true;
}

std::string Webhook::to_base64(const std::string& data) {
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(B64_CHARS[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6)
        out.push_back(B64_CHARS[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4)
        out.push_back('=');
    return out;
}

bool Webhook::fetch_url_to_string(const std::string& url, std::string& out) {
    CURL* curl = curl_easy_init();
    if (!curl) { m_last_error = "Failed to init cURL"; return false; }

    std::string clean_url = trim(url);

    curl_easy_setopt(curl, CURLOPT_URL, clean_url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
        +[](char* data, size_t size, size_t nmemb, void* userdata) -> size_t {
            reinterpret_cast<std::string*>(userdata)->append(data, size * nmemb);
            return size * nmemb;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        m_last_error = std::string("cURL error: ") + curl_easy_strerror(res);
        return false;
    }
    return true;
}

static std::string detect_mime(const std::string& data) {
    if (data.size() >= 4 &&
        (unsigned char)data[0] == 0x89 && data[1] == 'P' &&
        data[2] == 'N' && data[3] == 'G')
        return "image/png";
    if (data.size() >= 3 &&
        (unsigned char)data[0] == 0xFF &&
        (unsigned char)data[1] == 0xD8 &&
        (unsigned char)data[2] == 0xFF)
        return "image/jpeg";
    if (data.size() >= 6 &&
        (data.substr(0, 6) == "GIF87a" || data.substr(0, 6) == "GIF89a"))
        return "image/gif";
    return "image/png";
}

bool Webhook::patch_avatar(const std::string& base64_data, const std::string& mime_type) {
    CURL* curl = curl_easy_init();
    if (!curl) { m_last_error = "Failed to init cURL"; return false; }

    std::string data_uri = "data:" + mime_type + ";base64," + base64_data;

    nlohmann::json payload;
    payload["avatar"] = data_uri;
    std::string payload_str = payload.dump();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    std::string url = trim(m_url);

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)payload_str.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
        +[](char* data, size_t size, size_t nmemb, void* userdata) -> size_t {
            reinterpret_cast<std::string*>(userdata)->append(data, size * nmemb);
            return size * nmemb;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        m_last_error = curl_easy_strerror(res);
        return false;
    }
    if (code != 200) {
        m_last_error = "HTTP " + std::to_string(code) + ": " + response;
        return false;
    }

    m_last_error.clear();
    return true;
}

bool Webhook::set_avatar_from_url(const std::string& image_url) {
    std::string raw;
    if (!fetch_url_to_string(trim(image_url), raw)) return false;

    std::string mime = detect_mime(raw);
    std::string b64  = to_base64(raw);
    return patch_avatar(b64, mime);
}

bool Webhook::set_avatar_from_file(const std::string& file_path) {
    std::ifstream file(trim(file_path), std::ios::binary);
    if (!file) {
        m_last_error = "Failed to open file: " + file_path;
        return false;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string raw = ss.str();

    std::string mime = detect_mime(raw);
    std::string b64  = to_base64(raw);
    return patch_avatar(b64, mime);
}

std::string Webhook::get_name()
{
    CURL* curl = curl_easy_init();
    if (!curl) { m_last_error = "Failed to init cURL"; return ""; }

    std::string url = trim(m_url);
    std::string response;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
        +[](char* data, size_t size, size_t nmemb, void* userdata) -> size_t {
            reinterpret_cast<std::string*>(userdata)->append(data, size * nmemb);
            return size * nmemb;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) { m_last_error = curl_easy_strerror(res); return ""; }
    if (code != 200) { m_last_error = "HTTP " + std::to_string(code); return ""; }

    try {
        auto j = nlohmann::json::parse(response);
        return j.value("name", "");
    } catch (...) {
        m_last_error = "Failed to parse response";
        return "";
    }
}

bool Webhook::delete_webhook() {
    CURL* curl = curl_easy_init();
    if (!curl) { m_last_error = "Failed to init cURL"; return false; }

    std::string url = trim(m_url);
    std::string response;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
        +[](char* data, size_t size, size_t nmemb, void* userdata) -> size_t {
            reinterpret_cast<std::string*>(userdata)->append(data, size * nmemb);
            return size * nmemb;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) { m_last_error = curl_easy_strerror(res); return false; }
    if (code != 204) { m_last_error = "HTTP " + std::to_string(code) + ": " + response; return false; }

    m_last_error.clear();
    m_url.clear();
    return true;
}

} // namespace moon
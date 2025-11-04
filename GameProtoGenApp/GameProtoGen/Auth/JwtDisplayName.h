#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>
#include <cppcodec/base64_url_unpadded.hpp>

inline std::optional<nlohmann::json> DecodeJwtClaims(const std::string& jwt) {
    const auto p1 = jwt.find('.');
    if (p1 == std::string::npos) return std::nullopt;
    const auto p2 = jwt.find('.', p1 + 1);
    if (p2 == std::string::npos) return std::nullopt;

    const std::string payload_b64url = jwt.substr(p1 + 1, p2 - (p1 + 1));
    try {
        const auto bytes = cppcodec::base64_url_unpadded::decode(payload_b64url);
        const std::string payload_json(bytes.begin(), bytes.end());
        return nlohmann::json::parse(payload_json);
    }
    catch (...) {
        return std::nullopt;
    }
}

inline std::string DisplayNameFromClaims(const nlohmann::json& c) {
    if (c.contains("name") && c["name"].is_string()) return c["name"].get<std::string>();
    const std::string given = c.value("given_name", "");
    const std::string family = c.value("family_name", c.value("surname", ""));
    if (!given.empty() || !family.empty()) return given.empty() ? family : (given + " " + family);
    if (c.contains("preferred_username") && c["preferred_username"].is_string())
        return c["preferred_username"].get<std::string>();
    if (c.contains("emails") && c["emails"].is_array() && !c["emails"].empty() && c["emails"][0].is_string())
        return c["emails"][0].get<std::string>();
    for (auto it = c.begin(); it != c.end(); ++it) {
        if (it.key().rfind("extension_", 0) == 0 && it.value().is_string())
            return it.value().get<std::string>();
    }
    return {};
}

inline std::string DisplayNameFromIdToken(const std::string& id_token) {
    if (id_token.empty()) return {};
    auto claims = DecodeJwtClaims(id_token);
    if (!claims) return {};
    return DisplayNameFromClaims(*claims);
}

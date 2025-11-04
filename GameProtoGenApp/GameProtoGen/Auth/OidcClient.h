#pragma once
#include <string>
#include <vector>
#include <optional>

struct OidcTokens {
    std::string access_token;
    std::string id_token;
    std::string refresh_token;
    int         expires_in = 0;
};

struct OidcConfig {
    std::string client_id;
    std::string authorize_endpoint;
    std::string token_endpoint;
    std::vector<std::string> scopes;
};

class OidcClient {
public:
    explicit OidcClient(OidcConfig cfg) : m_Cfg(std::move(cfg)) {}

    std::optional<OidcTokens> AcquireTokenInteractive(std::string* err = nullptr);
    std::optional<OidcTokens> AcquireTokenByRefreshToken(const std::string& refresh_token,
        std::string* err = nullptr);
private:
    OidcConfig m_Cfg;
};

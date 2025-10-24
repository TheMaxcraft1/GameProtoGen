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
    std::string client_id;       // "2041dbc5-..."
    std::string authority;       // issuer base from .well-known (B2C policy)
    std::string authorize_endpoint; // from .well-known
    std::string token_endpoint;     // from .well-known
    std::vector<std::string> scopes{ "openid", "profile", "offline_access" };
};

class OidcClient {
public:
    explicit OidcClient(OidcConfig cfg) : m_Cfg(std::move(cfg)) {}

    // Lanza navegador, espera code y canjea por tokens.
    // Devuelve tokens o error (string)
    std::optional<OidcTokens> AcquireTokenInteractive(std::string* err);

    // Usa refresh_token para renovar access_token
    std::optional<OidcTokens> AcquireTokenByRefreshToken(const std::string& refresh_token, std::string* err);

private:
    OidcConfig m_Cfg;
};

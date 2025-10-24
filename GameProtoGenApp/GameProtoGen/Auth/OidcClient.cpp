#include "OidcClient.h"
#include "Pkce.h"
#include "LoopbackServer.h"
#include <nlohmann/json.hpp>
#include <cpr/cpr.h>
#include <cppcodec/base64_url_unpadded.hpp>
#include <sstream>
#include <windows.h>


static void open_browser(const std::string& url) {
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);


static std::string join_scopes(const std::vector<std::string>& scopes) {
    std::ostringstream oss;
    for (size_t i = 0; i < scopes.size(); ++i) {
        if (i) oss << ' ';
        oss << scopes[i];
    }
    return oss.str();
}

std::optional<OidcTokens> OidcClient::AcquireTokenInteractive(std::string* err) {
    LoopbackServer server;
    if (!server.Start()) {
        if (err) *err = "No pude abrir el loopback server";
        return std::nullopt;
    }

    // PKCE
    const auto pkce = GeneratePkcePair();

    // state anti-CSRF (simple, 16 bytes base64url)
    const std::string state = cppcodec::base64_url_unpadded::encode("st-" + pkce.verifier.substr(0, 16));

    // Armar URL de /authorize
    // Nota: usar EXACTAMENTE los endpoints que te dio .well-known (con tenantId y policy)
    cpr::Parameters q{
        {"client_id",     m_Cfg.client_id},
        {"response_type", "code"},
        {"redirect_uri",  server.RedirectUri()},
        {"response_mode", "query"},
        {"scope",         join_scopes(m_Cfg.scopes)},
        {"code_challenge", pkce.challenge},
        {"code_challenge_method", "S256"},
        {"state", state}
    };
    const std::string authz_url = m_Cfg.authorize_endpoint + "?" + q.GetContent(cpr::Url{}).substr(1); // sin '?'

    open_browser(authz_url);

    // Esperar callback
    if (!server.WaitForCode(180000)) { // 3 minutos
        if (err) *err = "Tiempo de espera agotado esperando el code.";
        return std::nullopt;
    }
    if (server.State() != state) {
        if (err) *err = "State inválido.";
        return std::nullopt;
    }
    const std::string code = server.Code();
    if (code.empty()) {
        if (err) *err = "No llegó 'code' en el callback.";
        return std::nullopt;
    }

    // Canjear code por tokens
    cpr::Payload form{
        {"grant_type", "authorization_code"},
        {"client_id", m_Cfg.client_id},
        {"code", code},
        {"redirect_uri", server.RedirectUri()},
        {"code_verifier", pkce.verifier},
        {"scope", join_scopes(m_Cfg.scopes)}
    };
    auto res = cpr::Post(
        cpr::Url{ m_Cfg.token_endpoint },
        cpr::Header{ {"Content-Type","application/x-www-form-urlencoded"} },
        form
    );
    if (res.error.code != cpr::ErrorCode::OK) {
        if (err) *err = "HTTP error: " + res.error.message;
        return std::nullopt;
    }
    if (res.status_code != 200) {
        if (err) *err = "Token endpoint status " + std::to_string(res.status_code) + ": " + res.text;
        return std::nullopt;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(res.text);
        OidcTokens t;
        t.access_token = j.value("access_token", "");
        t.id_token = j.value("id_token", "");
        t.refresh_token = j.value("refresh_token", "");
        t.expires_in = j.value("expires_in", 0);
        if (t.access_token.empty()) {
            if (err) *err = "Respuesta sin access_token.";
            return std::nullopt;
        }
        return t;
    }
    catch (const std::exception& e) {
        if (err) *err = std::string("Invalid JSON: ") + e.what();
        return std::nullopt;
    }
}

std::optional<OidcTokens> OidcClient::AcquireTokenByRefreshToken(const std::string& refresh_token, std::string* err) {
    cpr::Payload form{
        {"grant_type", "refresh_token"},
        {"client_id", m_Cfg.client_id},
        {"refresh_token", refresh_token},
        {"scope", join_scopes(m_Cfg.scopes)}
    };
    auto res = cpr::Post(
        cpr::Url{ m_Cfg.token_endpoint },
        cpr::Header{ {"Content-Type","application/x-www-form-urlencoded"} },
        form
    );
    if (res.error.code != cpr::ErrorCode::OK) {
        if (err) *err = "HTTP error: " + res.error.message;
        return std::nullopt;
    }
    if (res.status_code != 200) {
        if (err) *err = "Token endpoint status " + std::to_string(res.status_code) + ": " + res.text;
        return std::nullopt;
    }
    try {
        nlohmann::json j = nlohmann::json::parse(res.text);
        OidcTokens t;
        t.access_token = j.value("access_token", "");
        t.id_token = j.value("id_token", "");
        t.refresh_token = j.value("refresh_token", "");
        t.expires_in = j.value("expires_in", 0);
        if (t.access_token.empty()) {
            if (err) *err = "Respuesta sin access_token.";
            return std::nullopt;
        }
        return t;
    }
    catch (const std::exception& e) {
        if (err) *err = std::string("Invalid JSON: ") + e.what();
        return std::nullopt;
    }
}

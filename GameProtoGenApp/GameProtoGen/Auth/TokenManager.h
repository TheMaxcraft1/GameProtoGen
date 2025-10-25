#pragma once
#include <string>
#include <optional>
#include <chrono>
#include <memory>
#include <nlohmann/json.hpp>
#include "OidcClient.h"
#include <fstream>      
#include <filesystem>  

// Maneja almacenamiento y refresh del par (access, refresh).
class TokenManager {
public:
    struct State {
        std::string access_token;
        std::string refresh_token;
        // Momento (epoch seconds) en que expira el access_token
        long long access_expiry_epoch = 0;
    };

    // cfg: endpoints + client_id + scopes (de tu OIDC)
    explicit TokenManager(OidcConfig cfg, std::string savePath = "Saves/tokens.json")
        : m_cfg(std::move(cfg)), m_path(std::move(savePath)) {
    }

    // Persiste tokens tras login inicial
    void OnInteractiveLogin(const OidcTokens& t) {
        m_state.access_token = t.access_token;
        m_state.refresh_token = t.refresh_token;
        // Guardamos el vencimiento como epoch seconds con pequeño skew
        const long long now = NowEpoch();
        const long long skew = 60;
        m_state.access_expiry_epoch = now + (t.expires_in > 0 ? (t.expires_in - (int)skew) : 300);
        Save();
    }

    // Carga desde disco (si existe)
    bool Load() {
        try {
            std::ifstream ifs(m_path);
            if (!ifs) return false;
            nlohmann::json j; ifs >> j;
            m_state.access_token = j.value("access_token", "");
            m_state.refresh_token = j.value("refresh_token", "");
            m_state.access_expiry_epoch = j.value("access_expiry_epoch", 0LL);
            return !(m_state.access_token.empty() && m_state.refresh_token.empty());
        }
        catch (...) { return false; }
    }

    // Guarda a disco
    bool Save() const {
        try {
            namespace fs = std::filesystem;
            fs::create_directories(fs::path(m_path).parent_path());
            nlohmann::json j{
                {"access_token", m_state.access_token},
                {"refresh_token", m_state.refresh_token},
                {"access_expiry_epoch", m_state.access_expiry_epoch}
            };
            std::ofstream ofs(m_path);
            if (!ofs) return false;
            ofs << j.dump(2);
            return true;
        }
        catch (...) { return false; }
    }

    // Devuelve el access token actual (puede estar vencido)
    const std::string& AccessToken() const { return m_state.access_token; }

    // Intentá renovar si el access está por vencer o vencido.
    // Devuelve true si terminó con access válido (renovado o ya sano).
    bool EnsureFresh() {
        const long long now = NowEpoch();
        if (!m_state.access_token.empty() && now < m_state.access_expiry_epoch)
            return true; // todavía sirve
        return Refresh().has_value();
    }

    // Forzá un refresh con el refresh_token guardado.
    // Si renueva, actualiza y persiste; devuelve el nuevo access.
    std::optional<std::string> Refresh() {
        if (m_state.refresh_token.empty()) return std::nullopt;
        OidcClient client(m_cfg);
        std::string err;
        auto t = client.AcquireTokenByRefreshToken(m_state.refresh_token, &err);
        if (!t) {
            // refresh inválido/expirado: limpiamos access; dejamos refresh por si querés inspeccionar.
            m_state.access_token.clear();
            Save();
            return std::nullopt;
        }
        m_state.access_token = t->access_token;
        if (!t->refresh_token.empty())
            m_state.refresh_token = t->refresh_token; // rotate si el IdP te dio uno nuevo
        const long long now = NowEpoch();
        const long long skew = 60;
        m_state.access_expiry_epoch = now + (t->expires_in > 0 ? (t->expires_in - (int)skew) : 300);
        Save();
        return m_state.access_token;
    }

private:
    static long long NowEpoch() {
        using namespace std::chrono;
        return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    }

    OidcConfig m_cfg;
    std::string m_path;
    State m_state;
};

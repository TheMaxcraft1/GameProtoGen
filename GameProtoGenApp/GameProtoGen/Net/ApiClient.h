#pragma once
#include <string>
#include <optional>
#include <future>
#include <utility>
#include <nlohmann/json.hpp>
#include <functional>  

class ApiClient {
public:
    explicit ApiClient(std::string host = "127.0.0.1", int port = 5559)
        : m_Host(std::move(host)), m_Port(port) {
    }

    struct Result {
        std::optional<nlohmann::json> data;
        std::string error;
        bool ok() const { return data.has_value() && error.empty(); }
    };

    using RefreshFn = std::function<std::optional<std::string>()>;
    using PreflightFn = std::function<void()>;

    void SetTokenRefresher(RefreshFn fn) { m_OnRefresh = std::move(fn); }
    void SetPreflight(PreflightFn fn) { m_OnPreflight = std::move(fn); }

    std::optional<nlohmann::json> SendCommand(const std::string& prompt,
        const nlohmann::json& scene,
        std::string* errMsg = nullptr);

    std::future<Result> SendCommandAsync(std::string prompt,
        nlohmann::json scene);

    void SetTimeouts(int connectSec, int readSec, int writeSec) {
        m_ConnectTimeoutSec = connectSec; // tiempo máximo para establecer la conexión
		m_ReadTimeoutSec = readSec; // tiempo máximo para recibir la respuesta
		m_WriteTimeoutSec = writeSec; // tiempo máximo para enviar la request
    }

    void SetBasePath(std::string basePath) { m_BasePath = std::move(basePath); }
    void UseHttps(bool on) { m_UseHttps = on; }
    void SetVerifySsl(bool verify) { m_VerifySsl = verify; }
    void SetAccessToken(std::string token) { m_AccessToken = std::move(token); }

private:
    std::string m_Host;
    int m_Port;
    std::string m_BasePath = "/api";
    std::string m_AccessToken;

    RefreshFn  m_OnRefresh;    
    PreflightFn m_OnPreflight; 

    int m_ConnectTimeoutSec = 2;
    int m_ReadTimeoutSec = 5;
    int m_WriteTimeoutSec = 5;

    bool m_UseHttps = false;
    bool m_VerifySsl = true;

    std::string BuildUrl(const std::string& path) const;
    static std::string JoinPath(const std::string& a, const std::string& b);
};

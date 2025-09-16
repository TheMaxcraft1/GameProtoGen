#pragma once
#include <string>
#include <optional>
#include <future>
#include <utility>
#include <nlohmann/json.hpp>

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

    std::optional<nlohmann::json> SendCommand(const std::string& prompt,
        const nlohmann::json& scene,
        std::string* errMsg = nullptr);

    std::future<Result> SendCommandAsync(std::string prompt,
        nlohmann::json scene);

    // Conservamos la firma para no tocar tu EditorApp.
    void SetTimeouts(int connectSec, int readSec, int writeSec) {
        m_ConnectTimeoutSec = connectSec;
        m_ReadTimeoutSec = readSec;
        m_WriteTimeoutSec = writeSec;
    }

    void SetBasePath(std::string basePath) { m_BasePath = std::move(basePath); }

    // Útil cuando migres a AWS (HTTPS).
    void UseHttps(bool on) { m_UseHttps = on; }
    void SetVerifySsl(bool verify) { m_VerifySsl = verify; }

private:
    std::string m_Host;
    int m_Port;
    std::string m_BasePath = "/api";

    int m_ConnectTimeoutSec = 2;
    int m_ReadTimeoutSec = 5;
    int m_WriteTimeoutSec = 5;

    bool m_UseHttps = false;
    bool m_VerifySsl = true;

    std::string BuildUrl(const std::string& path) const;
    static std::string JoinPath(const std::string& a, const std::string& b);
};

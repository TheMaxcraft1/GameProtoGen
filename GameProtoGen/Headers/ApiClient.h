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

    // Sync y Async
    std::optional<nlohmann::json> SendCommand(const std::string& prompt, std::string* errMsg = nullptr);
    std::future<Result> SendCommandAsync(std::string prompt);

    // Timeouts (segundos)
    void SetTimeouts(int connectSec, int readSec, int writeSec) {
        m_ConnectTimeoutSec = connectSec; m_ReadTimeoutSec = readSec; m_WriteTimeoutSec = writeSec;
    }

    // 👇 NUEVO: prefijo de ruta (ej. "/api" o "")
    void SetBasePath(std::string basePath) { m_BasePath = std::move(basePath); }

private:
    std::string m_Host;
    int m_Port;
    std::string m_BasePath = "/api"; // 👈 por defecto, porque usás [Route("api/[controller]")]

    int m_ConnectTimeoutSec = 2;
    int m_ReadTimeoutSec = 5;
    int m_WriteTimeoutSec = 5;
};

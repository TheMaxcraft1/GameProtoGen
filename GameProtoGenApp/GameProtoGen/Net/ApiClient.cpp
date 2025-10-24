#include "ApiClient.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <algorithm>   // <-- necesario para std::max
#include <utility>

using json = nlohmann::json;

std::string ApiClient::JoinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (a.back() == '/' && !b.empty() && b.front() == '/') return a + b.substr(1);
    if (a.back() != '/' && !b.empty() && b.front() != '/') return a + "/" + b;
    return a + b;
}

std::string ApiClient::BuildUrl(const std::string& path) const {
    const bool host_has_scheme =
        m_Host.rfind("http://", 0) == 0 || m_Host.rfind("https://", 0) == 0;

    const std::string scheme = host_has_scheme ? "" : (m_UseHttps ? "https://" : "http://");

    std::string hostport = m_Host;
    if (!host_has_scheme) {
        if (hostport.find(':') == std::string::npos && m_Port > 0) {
            hostport += ":" + std::to_string(m_Port);
        }
    }

    return scheme + hostport + JoinPath(m_BasePath, path);
}

std::optional<json> ApiClient::SendCommand(const std::string& prompt,
    const json& scene,
    std::string* err) {
    const std::string url = BuildUrl("/chat/command");
    json req = { {"prompt", prompt}, {"scene", scene} };

    const long xfer_ms =
        static_cast<long>((std::max)(m_ReadTimeoutSec, m_WriteTimeoutSec)) * 1000L;
    
    cpr::Header hdr{ {"Content-Type","application/json"} };
    if (!m_AccessToken.empty()) {
        hdr["Authorization"] = "Bearer " + m_AccessToken;
    }

    cpr::Response res = cpr::Post(
        cpr::Url{ url },
        hdr,
        cpr::Body{ req.dump() },
        cpr::ConnectTimeout{ m_ConnectTimeoutSec * 1000 },
        cpr::Timeout{ xfer_ms },
        cpr::VerifySsl{ m_VerifySsl }
    );

    if (res.error.code != cpr::ErrorCode::OK) {
        if (err) *err = res.error.message;
        return std::nullopt;
    }
    if (res.status_code == 200) {
        try { return json::parse(res.text); }
        catch (const std::exception& e) {
            if (err) *err = std::string("Invalid JSON: ") + e.what();
            return std::nullopt;
        }
    }
    if (err) *err = "HTTP status " + std::to_string(res.status_code);
    return std::nullopt;
}

std::future<ApiClient::Result> ApiClient::SendCommandAsync(std::string prompt,
    json scene) {
    const std::string url = BuildUrl("/chat/command");
    const int connect_ms = m_ConnectTimeoutSec * 1000;
    const long xfer_ms =
        static_cast<long>((std::max)(m_ReadTimeoutSec, m_WriteTimeoutSec)) * 1000L;
    const bool verify = m_VerifySsl;

    cpr::Header hdr{ {"Content-Type","application/json"} };
    if (!m_AccessToken.empty()) {
        hdr["Authorization"] = "Bearer " + m_AccessToken;
    }

    auto async_resp = cpr::PostAsync(
        cpr::Url{ url },
        hdr,
        cpr::Body{ json({{"prompt", std::move(prompt)}, {"scene", std::move(scene)}}).dump() },
        cpr::ConnectTimeout{ connect_ms },
        cpr::Timeout{ xfer_ms },
        cpr::VerifySsl{ verify }
    );

    return std::async(std::launch::async, [ar = std::move(async_resp)]() mutable -> Result {
        Result r;
        cpr::Response res = ar.get();  // <-- .get() (no ->get())

        if (res.error.code != cpr::ErrorCode::OK) {
            r.error = res.error.message;
            return r;
        }
        if (res.status_code == 200) {
            try { r.data = json::parse(res.text); }
            catch (const std::exception& e) { r.error = std::string("Invalid JSON: ") + e.what(); }
        }
        else {
            r.error = "HTTP status " + std::to_string(res.status_code);
        }
        return r;
        });
}

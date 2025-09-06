#include "Headers/ApiClient.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

static void SetupClient(httplib::Client& cli, int ct, int rt, int wt) {
    cli.set_connection_timeout(ct);
    cli.set_read_timeout(rt);
    cli.set_write_timeout(wt);
    cli.set_follow_location(true);
}

static std::string JoinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (a.back() == '/' && !b.empty() && b.front() == '/') return a + b.substr(1);
    if (a.back() != '/' && !b.empty() && b.front() != '/') return a + "/" + b;
    return a + b;
}

std::optional<json> ApiClient::SendCommand(const std::string& prompt,
    const json& scene,
    std::string* err) {
    httplib::Client cli(m_Host.c_str(), m_Port);
    SetupClient(cli, m_ConnectTimeoutSec, m_ReadTimeoutSec, m_WriteTimeoutSec);

    const std::string path = JoinPath(m_BasePath, "/chat/command");
    json req = { {"prompt", prompt}, {"scene", scene} };

    if (auto res = cli.Post(path.c_str(), req.dump(), "application/json")) {
        if (res->status == 200) return json::parse(res->body);
        if (err) *err = "HTTP status " + std::to_string(res->status);
    }
    else {
        if (err) *err = "No response from server (can't connect to " + m_Host + ":" + std::to_string(m_Port) + ")";
    }
    return std::nullopt;
}

std::future<ApiClient::Result> ApiClient::SendCommandAsync(std::string prompt,
    json scene) {
    const std::string host = m_Host;
    const int port = m_Port;
    const int ct = m_ConnectTimeoutSec, rt = m_ReadTimeoutSec, wt = m_WriteTimeoutSec;
    const std::string basePath = m_BasePath;

    return std::async(std::launch::async, [host, port, ct, rt, wt, basePath, p = std::move(prompt), s = std::move(scene)]() -> Result {
        Result r;
        httplib::Client cli(host.c_str(), port);
        SetupClient(cli, ct, rt, wt);

        const std::string path = JoinPath(basePath, "/chat/command");
        json req = { {"prompt", p}, {"scene", s} };

        if (auto res = cli.Post(path.c_str(), req.dump(), "application/json")) {
            if (res->status == 200) r.data = json::parse(res->body);
            else r.error = "HTTP status " + std::to_string(res->status);
        }
        else {
            r.error = "No response from server (can't connect to " + host + ":" + std::to_string(port) + ") or TIMEOUT";
        }
        return r;
        });
}

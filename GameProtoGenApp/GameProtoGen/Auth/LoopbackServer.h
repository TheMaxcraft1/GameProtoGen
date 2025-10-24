#pragma once
#include <string>
#include <future>
#include <atomic>

class LoopbackServer {
public:
    // Intenta abrir un puerto efímero 49152-65535 y escuchar en 127.0.0.1
    bool Start();
    void Stop();

    // URI para registrar en el auth request
    std::string RedirectUri() const { return "http://127.0.0.1:" + std::to_string(m_Port) + "/"; }

    // Espera a que llegue el /?code=...&state=...
    // Devuelve true si obtuvo code (y state) antes del timeout_ms
    bool WaitForCode(int timeout_ms);

    const std::string& Code() const { return m_Code; }
    const std::string& State() const { return m_State; }

private:
    int m_Port = 0;
    std::atomic<bool> m_Running{ false };
    std::promise<void> m_Ready;
    std::future<void> m_ReadyFut = m_Ready.get_future();
    std::promise<void> m_Done;
    std::future<void> m_DoneFut = m_Done.get_future();

    std::string m_Code;
    std::string m_State;
};

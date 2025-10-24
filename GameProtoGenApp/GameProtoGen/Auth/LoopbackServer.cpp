#include "LoopbackServer.h"
#include <httplib.h>
#include <thread>
#include <random>

static int pick_ephemeral_port() {
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> dist(49152, 65535);
    return dist(rng);
}

bool LoopbackServer::Start() {
    if (m_Running.load()) return true;
    m_Running = true;

    std::thread([this]() {
        httplib::Server svr;
        svr.set_keep_alive_max_count(0);

        svr.Get("/", [this](const httplib::Request& req, httplib::Response& res) {
            if (req.has_param("code"))  m_Code = req.get_param_value("code");
            if (req.has_param("state")) m_State = req.get_param_value("state");

            res.set_content(
                "<html><body><h3>Login OK</h3><p>Ya podés volver a la app.</p></body></html>",
                "text/html"
            );
            // cerramos el server en breve
            std::thread([this]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                m_Done.set_value();
                }).detach();
            });

        // Probar varios puertos efímeros
        int tries = 30;
        while (tries-- > 0) {
            int port = pick_ephemeral_port();
            if (svr.bind_to_port("127.0.0.1", port)) {
                m_Port = port;
                break;
            }
        }
        if (m_Port == 0) {
            // Falló
            m_Ready.set_value();
            m_Running = false;
            return;
        }

        m_Ready.set_value();
        svr.listen_after_bind(); // bloquea hasta Stop
        m_Running = false;
        }).detach();

    // Espera a que el hilo haga bind (o falle)
    m_ReadyFut.wait();
    return m_Port != 0;
}

void LoopbackServer::Stop() {
    // La forma simple: conectamos al endpoint para liberar listen_after_bind (si aún no se activó)
    // Pero en nuestro flujo normal, la propia request de callback dispara m_Done.
}

bool LoopbackServer::WaitForCode(int timeout_ms) {
    if (!m_Running) return false;
    if (m_DoneFut.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::ready) {
        return !m_Code.empty();
    }
    return false;
}

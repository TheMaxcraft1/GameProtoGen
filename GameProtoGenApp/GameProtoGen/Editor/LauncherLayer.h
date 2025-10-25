#pragma once
#include "Core/Application.h"
#include <string>

// Forward decls
namespace gp { class Application; }

class LauncherLayer : public gp::Layer {
public:
    void OnAttach() override;
    void OnGuiRender() override;

private:
    bool m_loggedIn = false;
    std::string m_selected; // ruta a Saves/*.json

    // Acciones
    void DoLoginInteractive();   // login + token manager + refresher
    void DrawProjectPicker();    // lista Saves/*.json (si existe)
    void SeedNewScene();         // crea una escena mínima
    void EnterEditor();          // monta Viewport/Inspector/Chat y se auto-saca
};

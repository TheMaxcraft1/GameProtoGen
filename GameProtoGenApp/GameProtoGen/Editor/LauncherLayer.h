#pragma once
#include "Core/Application.h"
#include <string>
#include <SFML/Graphics/Texture.hpp>

// Forward decls
namespace gp { class Application; }

class LauncherLayer : public gp::Layer {
public:
    void OnAttach() override;
    void OnGuiRender() override;

private:
    bool m_loggedIn = false;
    std::string m_selected; // ruta a Saves/*.json
    bool m_newProjModal = false;
    char m_newProjName[128] = "NuevoProyecto";
    std::string m_newProjError;
    sf::Texture m_LogoTex;
    bool m_LogoOK = false;
    bool m_confirmDeleteModal = false;
    std::string m_toDeletePath;
    std::string m_deleteError;
    std::string m_displayName;

    // Acciones
    void DoLoginInteractive();   // login + token manager + refresher
    void DrawProjectPicker();    // lista Saves/*.json (si existe)
    void SeedNewScene();         // crea una escena mínima
    void EnterEditor();          // monta Viewport/Inspector/Chat y se auto-saca
    bool TryAutoLogin();
};

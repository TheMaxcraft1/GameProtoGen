#pragma once
#include "Core/Application.h"

class EditorDockLayer : public gp::Layer {
public:
    void OnGuiRender() override;

private:
    bool m_BuiltDock = false;
};
#pragma once
#include "Application.h"

class InspectorPanel : public gp::Layer {
public:
    InspectorPanel() = default;
    void OnGuiRender() override;
};

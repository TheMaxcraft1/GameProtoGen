#include "Headers/InspectorPanel.h"
#include "Headers/SceneContext.h"

#include <imgui.h>

void InspectorPanel::OnGuiRender() {
    auto& ctx = SceneContext::Get();

    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    ImGui::Text("Controles del círculo");

    ImGui::SliderFloat("Radio", &ctx.radius, 10.f, 200.f);
    ImGui::ColorEdit3("Color", (float*)&ctx.color);

    ImGui::End();
}

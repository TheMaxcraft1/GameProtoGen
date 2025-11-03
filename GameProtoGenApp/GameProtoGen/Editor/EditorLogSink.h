#pragma once
#include "Core/Log.h"
#include "Editor/Panels/ViewportPanel.h"

struct ImGuiConsoleSink : ILogSink {
    void info(const std::string& m) override { ViewportPanel::AppendLog("[INFO] " + m); }
    void error(const std::string& m) override { ViewportPanel::AppendLog("[ERR ] " + m); }
};

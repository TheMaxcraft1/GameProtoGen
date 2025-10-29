#pragma once
#ifdef GP_BUILD_EDITOR
#include <imgui.h>
namespace EditorFonts {
    inline ImFont* Regular = nullptr;
    inline ImFont* H2 = nullptr;
    inline ImFont* H1 = nullptr;
}
#else
namespace EditorFonts { /* stubs vacíos en player */ }
#endif

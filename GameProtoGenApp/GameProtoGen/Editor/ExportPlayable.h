#pragma once
#include <string>

namespace Exporter {
    // Devuelve true si todo ok; loguea en la consola del editor.
    bool ExportPlayable(const std::string& outDir);
}

#pragma once
// Task 200: Export logs + system info to a zip on the Desktop
#include <string>
namespace aura {
class DiagnosticsExporter {
public:
    // Copies log files, latest crash dump (if any), and system info text
    // into a zip at Desktop\AuraCastPro_Diagnostics_YYYYMMDD.zip
    // Returns path of created zip, or empty string on failure.
    static std::string exportToDesktop();
};
} // namespace aura

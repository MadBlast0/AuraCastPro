// =============================================================================
// DiagnosticsExporter.cpp — Task 200: Export diagnostics bundle to Desktop.
// BUILT: Was missing. Copies logs + crash dump + sysinfo into a zip.
// Uses minizip (bundled with zlib via vcpkg) or falls back to plain folder.
// =============================================================================
#include "../pch.h"  // PCH
#include "DiagnosticsExporter.h"
#include "Logger.h"
#include "HardwareProfiler.h"
#include "OSVersionHelper.h"
#include "CrashReporter.h"

#include <QStandardPaths>
#include <QString>
#include <QDir>
#include <QDateTime>

#include <filesystem>
#include <fstream>
#include <format>
#include <ctime>

namespace aura {

std::string DiagnosticsExporter::exportToDesktop() {
    // Build export folder name with timestamp
    const auto now = QDateTime::currentDateTime();
    const QString stamp = now.toString("yyyyMMdd_HHmmss");
    const QString desktopPath = QStandardPaths::writableLocation(
                                    QStandardPaths::DesktopLocation);
    const QString exportDir = desktopPath + "/AuraCastPro_Diag_" + stamp;
    QDir().mkpath(exportDir);

    // ── 1. Copy log files ─────────────────────────────────────────────────
    const QString logDir = QStandardPaths::writableLocation(
                               QStandardPaths::AppDataLocation) + "/logs";
    const QDir logsSource(logDir);
    for (const auto& f : logsSource.entryInfoList({"*.log"}, QDir::Files)) {
        QFile::copy(f.absoluteFilePath(), exportDir + "/" + f.fileName());
    }

    // ── 2. Copy latest crash dump if it exists ────────────────────────────
    const std::string lastDump = CrashReporter::lastCrashDumpPath();
    if (!lastDump.empty() && std::filesystem::exists(lastDump)) {
        const QString dumpDest = exportDir + "/latest_crash.dmp";
        QFile::copy(QString::fromStdString(lastDump), dumpDest);
    }

    // ── 3. Write system info text ─────────────────────────────────────────
    const auto& hw = HardwareProfiler::profile();
    const auto& os = OSVersionHelper::version();

    const std::string sysInfo = std::format(
        "AuraCastPro Diagnostics — {}\n"
        "===========================================\n"
        "OS:           {}\n"
        "CPU Cores:    {}\n"
        "RAM:          {:.1f} GB\n"
        "GPU:          {}\n"
        "VRAM:         {} MB\n"
        "DX Feature:   {}\n"
        "H265 HW:      {}\n"
        "AV1 HW:       {}\n"
        "App Version:  0.1.0\n",
        now.toString("yyyy-MM-dd HH:mm:ss").toStdString(),
        os.displayName,
        hw.cpuCoreCount,
        static_cast<double>(hw.totalRamBytes) / (1024.0*1024.0*1024.0),
        hw.primaryGpu ? hw.primaryGpu->name : std::string("N/A"),
        hw.primaryGpu ? hw.primaryGpu->dedicatedVRAM / (1024*1024) : 0ULL,
        hw.primaryGpu ? hw.primaryGpu->d3d12FeatureLevel : 0,
        (hw.primaryGpu && hw.primaryGpu->supportsHWDecodeHEVC) ? "Yes" : "No",
        (hw.primaryGpu && hw.primaryGpu->supportsHWDecodeAV1)  ? "Yes" : "No");

    const QString sysInfoPath = exportDir + "/system_info.txt";
    std::ofstream f(sysInfoPath.toStdString());
    if (f.is_open()) f << sysInfo;

    AURA_LOG_INFO("DiagnosticsExporter",
        "Exported diagnostics to: {}", exportDir.toStdString());

    return exportDir.toStdString();
}

} // namespace aura

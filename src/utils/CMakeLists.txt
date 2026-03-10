add_library(utils STATIC
    Logger.cpp
    PerformanceTimer.cpp
    HardwareProfiler.cpp
    NetworkTools.cpp
    EncryptHelper.cpp
    OSVersionHelper.cpp
    CrashReporter.cpp
    CrashHandler.cpp
    DiagnosticsExporter.cpp
    WindowsEventLog.cpp
    AppDataInit.cpp
)

target_include_directories(utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(utils
    PUBLIC
        spdlog::spdlog
        OpenSSL::SSL
        OpenSSL::Crypto
)

target_compile_definitions(utils PUBLIC SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

# dbghelp: required by CrashReporter (MiniDumpWriteDump)
target_link_libraries(utils PRIVATE dbghelp)

# Qt needed by DiagnosticsExporter (QStandardPaths, QDir, QDateTime)
find_package(Qt6 REQUIRED COMPONENTS Core)
target_link_libraries(utils PUBLIC Qt6::Core)

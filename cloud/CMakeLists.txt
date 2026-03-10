add_library(cloud STATIC
    LicenseClient.cpp
    UpdateService.cpp
    TelemetryClient.cpp
    ProxySettings.cpp
    WinHttpHelper.cpp
)

target_include_directories(cloud PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(cloud
    PUBLIC utils
    PRIVATE
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        nlohmann_json::nlohmann_json
        Qt6::Network
        winhttp
)

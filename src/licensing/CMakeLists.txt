set(CMAKE_AUTOMOC ON)

add_library(licensing STATIC
    LicenseManager.cpp
    LicenseValidator.cpp
    FeatureGate.cpp
)

target_include_directories(licensing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(licensing PUBLIC
    utils
    Qt6::Core
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

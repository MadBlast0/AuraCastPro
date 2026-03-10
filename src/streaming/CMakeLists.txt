add_library(streaming STATIC
    HardwareEncoder.cpp
    NVENCWrapper.cpp
    AMFWrapper.cpp
    QSVWrapper.cpp
    StreamBroadcaster.cpp
    RTMPOutput.cpp
)

target_include_directories(streaming PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(streaming PUBLIC utils display)

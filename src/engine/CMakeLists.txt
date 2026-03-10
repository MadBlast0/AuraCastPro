add_library(engine STATIC
    # Network
    ReceiverSocket.cpp
    UDPServerThreadPool.cpp
    PacketReorderCache.cpp
    PacketValidator.cpp
    FECRecovery.cpp
    BitratePID.cpp
    NetworkPredictor.cpp
    FrameTimingQueue.cpp
    # Protocol
    AirPlay2Host.cpp
    CastV2Host.cpp
    ADBBridge.cpp
    ProtocolHandshake.cpp
    # Video pipeline
    NALParser.cpp
    H265Demuxer.cpp
    AV1Demuxer.cpp
    VideoDecoder.cpp
    ADBServerLifecycle.cpp
    H264Demuxer.cpp
    PortConflict.cpp
    ReconnectManager.cpp
    USBHotplug.cpp
    StreamHealthMonitor.cpp
    SessionLogger.cpp
    NetworkAdaptation.cpp
)

target_include_directories(engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(engine
    PUBLIC
        utils
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
    PRIVATE
        nlohmann_json::nlohmann_json
)

# Windows system libraries required by this module
target_link_libraries(engine PRIVATE
    mf.lib mfplat.lib mfuuid.lib
    ws2_32.lib
)

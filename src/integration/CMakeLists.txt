add_library(integration STATIC
    VCamBridge.cpp
    AudioDeviceEnumerator.cpp
    AudioLoopback.cpp
    AudioResampler.cpp
    AudioMixer.cpp
    AVSync.cpp
    StreamRecorder.cpp
    RecordingSystem.cpp
    VirtualAudioDriver.cpp
    DiskSpaceMonitor.cpp
    SharedMemoryIPC.cpp
    MP4MuxerFragments/MP4Muxer.cpp
)

target_include_directories(integration PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(integration
    PUBLIC utils
    PRIVATE
        OpenSSL::SSL
        mf.lib mfplat.lib mfuuid.lib
        wmcodecdspuuid.lib
        avcodec avformat avutil swresample swscale
)

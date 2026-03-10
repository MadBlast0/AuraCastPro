find_package(GTest REQUIRED)
include(GoogleTest)

# ── Test fixture path — available as AURA_FIXTURES_DIR in all test targets ───
set(AURA_FIXTURES_DIR "${CMAKE_SOURCE_DIR}/tests/fixtures")

# Generate synthetic fixtures if not already present (no ffmpeg required)
if(NOT EXISTS "${AURA_FIXTURES_DIR}/h265_keyframe.bin")
    message(STATUS "Generating synthetic test fixtures...")
    execute_process(
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/generate_synthetic_fixtures.py"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE FIXTURE_GEN_RESULT
    )
    if(NOT FIXTURE_GEN_RESULT EQUAL 0)
        message(WARNING "Fixture generation failed — some tests may skip.")
    endif()
endif()

# ─── Unit Tests ───────────────────────────────────────────────────────────────

add_executable(ParserTests           unit/ParserTests.cpp)
add_executable(SecurityTests         unit/SecurityTests.cpp)
add_executable(BitratePIDTests       unit/BitratePIDTests.cpp)
add_executable(PacketRecoveryTests   unit/PacketRecoveryTests.cpp)
add_executable(AudioSyncTests        unit/AudioSyncTests.cpp)
add_executable(ProtocolConformanceTests unit/ProtocolConformanceTests.cpp)
add_executable(RotationHandlingTests unit/RotationHandlingTests.cpp)
add_executable(DeviceManagerTests    unit/DeviceManagerTests.cpp)
add_executable(AudioMixerTests       unit/AudioMixerTests.cpp)
add_executable(NALParserTests        unit/NALParserTests.cpp)
add_executable(NetworkPredictorTests unit/NetworkPredictorTests.cpp)

foreach(target
        ParserTests SecurityTests BitratePIDTests PacketRecoveryTests
        AudioSyncTests ProtocolConformanceTests RotationHandlingTests
        DeviceManagerTests AudioMixerTests NALParserTests NetworkPredictorTests)
    target_link_libraries(${target}
        PRIVATE GTest::gtest_main engine utils manager integration)
    target_compile_definitions(${target}
        PRIVATE AURA_FIXTURES_DIR="${AURA_FIXTURES_DIR}")
    gtest_discover_tests(${target})
endforeach()

# ─── Integration Tests ────────────────────────────────────────────────────────

add_executable(StreamEndToEnd integration/StreamEndToEnd.cpp)
target_link_libraries(StreamEndToEnd
    PRIVATE GTest::gtest_main engine display integration utils)
target_compile_definitions(StreamEndToEnd
    PRIVATE AURA_FIXTURES_DIR="${AURA_FIXTURES_DIR}")
gtest_discover_tests(StreamEndToEnd)

# ─── Performance Tests ────────────────────────────────────────────────────────

add_executable(LatencyMonitor        performance/LatencyMonitor.cpp)
add_executable(GPUThroughputBenchmark performance/GPUThroughputBenchmark.cpp)

target_link_libraries(LatencyMonitor        PRIVATE engine utils)
target_link_libraries(GPUThroughputBenchmark PRIVATE display utils)

add_executable(AudioLatencyBenchmark performance/AudioLatencyBenchmark.cpp)
target_link_libraries(AudioLatencyBenchmark
    PRIVATE GTest::gtest_main integration utils
    winmm ole32 oleaut32)
gtest_discover_tests(AudioLatencyBenchmark)
